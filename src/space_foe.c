#pragma bank 10
/* BUGSTAR GBC - space's foes (split from space.c, see space_int.h): pirate drones that chase and ram (and shoot, from the first
   freed world on), darts that aim, blink and dive, mines that drift and spit orbs in crosses; debris and a white sky on big blasts;
   and, with all 8 data cores back, the Technobeast: armour that shrugs off lasers, a core that opens and glows for a moment, orbs. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "space_int.h"
#include "save.h"
#include "hud.h"
#include "fx.h"
#include "sound.h"
#include "warp.h"
#include "space.h"
#include "econ.h"
#include "menus.h"
#include "text.h"
void toast(const char *t1, const char *t2) {              /* two lines over the HUD for a moment (from the foes, bank 10) */
    shown = 252; toast_t = 110;
    txt_win(1);
    txt_line(0, 0, " ", 1); txt_line(1, 0, t1, 19); txt_color(0, 0, 20, 1, TXT_GOLD);
    txt_line(0, 1, " ", 1); txt_line(1, 1, t2, 19); txt_color(0, 1, 20, 1, TXT_WHITE);
    txt_win(0);
}

typedef struct { UINT16 x, y; UINT8 st, hp, kind, ph, t, d; } Pirate;   /* st: 0 none, 1 flying, 2.. exploding (counts up) */
enum { PK_DRONE, PK_DART, PK_MINE };
enum { DP_CHASE, DP_AIM, DP_DASH, DP_DRIFT };                 /* a dart's moods */
typedef struct { UINT16 x, y; INT8 dx, dy; UINT8 life; } Orb;
typedef struct { UINT16 x, y; INT8 dx, dy; UINT8 life; } Spark;

static Pirate pir[NPIR];
static Orb    orb[NORB];
static Spark  spark[NSPARK];
#define NPOD 2                                                    /* cargo crates pirates drop: sprites 12 and 13 (not while the beast is about) */
typedef struct { UINT16 x, y; UINT8 g, life; } Pod;
static Pod    pod[NPOD];
static const UINT8 pod_goods[3][4] = { { G_FOOD, G_ORE, G_SILK, G_AMBER }, { G_ORE, G_TECH, G_MEDS, G_ORE }, { G_CRYSTAL, G_NECTAR, G_TECH, G_CRYSTAL } };
static UINT8  flash_t, spawn_t;
static UINT8  b_hp, b_flash, b_t, b_ang;
static const INT8 dir_x[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const INT8 dir_y[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const INT8 circ[32] = { 0, 6, 11, 17, 21, 25, 28, 29, 30, 29, 28, 25, 21, 17, 11, 6, 0, -6, -11, -17, -21, -25, -28, -29, -30, -29, -28, -25, -21, -17, -11, -6 };
#define CORE_OPEN() ((b_t & 127) < 56)   /* open a little under half the time */

static UINT8 dir8(INT16 dx, INT16 dy){                        /* the nearest of the 8 directions to (dx, dy) */
    UINT16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if(ax > ay * 2) return dx > 0 ? 2 : 6;
    if(ay > ax * 2) return dy > 0 ? 4 : 0;
    return dx > 0 ? (dy > 0 ? 3 : 1) : (dy > 0 ? 5 : 7);
}
static UINT8 near_planet(UINT16 px, UINT16 py, INT16 r){      /* a planet within r px of (px, py)? (its own cell is enough: planets sit inside) */
    UINT8 s = (UINT8)(((py >> 8) << 4) | (px >> 8)); INT16 dx = wrapd(px, star_px(s)), dy = wrapd(py, star_py(s));
    return dx > -r && dx < r && dy > -r && dy < r;
}
static void hurt(UINT8 dmg, INT16 dx, INT16 dy, INT8 push){     /* the ship is hit; dx, dy: from what hit it to the ship */
    if(inv) return;
    { UINT8 a = UPG(UP_HULL) + UPG(SK_GUARD) / 3; dmg = dmg > a ? dmg - a : 1; }
    if(UPG(UP_SHIELD) && sv.mp){                                 /* the shield soaks it up: 2 MP for every point of damage */
        UINT8 soak = sv.mp / 2 > dmg ? dmg : (UINT8)(sv.mp / 2);
        sv.mp -= soak * 2; dmg -= soak;
        if(!dmg){ inv = 45; sfx(SFX_SPARK); shake(5); return; }
    }
    sv.hp = sv.hp > dmg ? sv.hp - dmg : 0;
    inv = 60; sfx(SFX_HURT); shake(12);
    vx = dx > 0 ? push : -push; vy = dy > 0 ? push : -push;       /* knocked away */
}
static void boom(Pirate *p){ p->st = 2; }
static void blast(UINT16 x, UINT16 y, UINT8 big){                /* debris flies out of (x, y); big ones flash the sky */
    static const INT8 sx[4] = { -2, 2, -1, 3 }, sy[4] = { -2, -1, 3, 2 };
    UINT8 i;
    if(!b_on) for(i = 0; i < NSPARK; i++){ spark[i].x = x; spark[i].y = y; spark[i].dx = sx[i] * (big ? 2 : 1); spark[i].dy = sy[i] * (big ? 2 : 1); spark[i].life = 18; }
    if(big) flash_t = 3;
    shake(big ? 14 : 6);
}
static void orb_dir(UINT16 x, UINT16 y, UINT8 d){                /* an orb from (x, y) in direction d */
    UINT8 i, n = b_on ? 2 : NORB;
    for(i = 0; i < n; i++) if(!orb[i].life){ orb[i].x = x; orb[i].y = y; orb[i].dx = dir_x[d] * 2; orb[i].dy = dir_y[d] * 2; orb[i].life = 150; sfx(SFX_FIRE); return; }
}
static void orb_fire(UINT16 x, UINT16 y, INT16 dx, INT16 dy){ orb_dir(x, y, dir8(dx, dy)); }   /* ...the way of (dx, dy) */
static void spawn(void){
    UINT8 i, d; UINT16 px = shx >> 4, py = shy >> 4;
    if(near_planet(px, py, 96)) return;                          /* planets are safe */
    for(i = 0; i < NPIR; i++) if(!pir[i].st){
        UINT8 r = rnd(), z = sv.zone > 3 ? 3 : sv.zone; Pirate *p = &pir[i];
        d = rnd() & 7;
        p->x = (px + dir_x[d] * 112) & 4095; p->y = (py + dir_y[d] * 100) & 4095;
        p->kind = (sv.zone && r < 70) ? PK_MINE : (r < 70 + 40 + z * 20) ? PK_DART : PK_DRONE;
        p->st = 1; p->ph = DP_CHASE; p->t = rnd() & 63; p->d = 0;
        p->hp = p->kind == PK_DART ? 1 : p->kind == PK_MINE ? 4 + z : 2 + z / 2;
        return;
    }
}

static void pod_drop(UINT16 x, UINT16 y, UINT8 kind){
    UINT8 i;
    if(b_on || (rnd() & 127) >= 52) return;                         /* about 4 in 10 */
    for(i = 0; i < NPOD; i++) if(!pod[i].life){ pod[i].x = x; pod[i].y = y; pod[i].g = pod_goods[kind > 2 ? 0 : kind][rnd() & 3]; pod[i].life = 255; return; }
}
static void pods_step(UINT8 frame){
    UINT8 i;
    for(i = 0; i < NPOD; i++){ Pod *p = &pod[i]; INT16 dx, dy;
        if(!p->life) continue;
        if(!(frame & 3)) p->life--;
        dx = wrapd(shx >> 4, p->x); dy = wrapd(shy >> 4, p->y);
        if(dx > -12 && dx < 12 && dy > -12 && dy < 12){
            UINT8 n = 1 + (rnd() % 3), room = HOLD_CAP() > econ_load() ? HOLD_CAP() - econ_load() : 0;
            if(!room){ if(!(frame & 31)) toast("THE HOLD IS FULL", "SELL SOMETHING FIRST"); continue; }
            if(n > room) n = room;
            sv.el.cargo[p->g] += n; p->life = 0; sfx(SFX_PICK);
            { char b[20], *d = u16s(b, n); d = scat(d, " "); good_label(p->g, d); toast(b, "ADDED TO THE HOLD"); }
        }
    }
}
void foes_clear(void) BANKED {
    UINT8 i;
    for(i = 0; i < NPOD; i++) pod[i].life = 0;
    for(i = 0; i < NPIR; i++) pir[i].st = 0;
    for(i = 0; i < NORB; i++) orb[i].life = 0;
    for(i = 0; i < NSPARK; i++) spark[i].life = 0;
    flash_t = 0; spawn_t = 200; b_fight = 0; b_flash = 0;
}
void foes_reset(void) BANKED {
    foes_clear();
    b_on = sv.cores >= 8 && !(sv.story & ST_DONE); b_hp = BEAST_HP; b_dead = 0;
#ifdef TEST_BEAST
    b_hp = 3;
#endif
}
void foes_gfx(void) BANKED {
    UINT8 i;
    for(i = 0; i < NORB; i++) set_sprite_tile(ORB_OAM(i), SPR0 + SP_T_ORB);
    for(i = 0; i < 18; i++) set_sprite_tile(OAM_BODY + i, SPR0 + SP_T_BEAST(i / 3, i % 3));
    for(i = 0; i < 2; i++) set_sprite_prop(OAM_ORB + i, 3 | 0x08);
    for(i = 0; i < NPOD; i++){ set_sprite_tile(12 + i, SPR0 + SP_T_POD); set_sprite_prop(12 + i, 2 | 0x08); }
    for(i = 0; i < NPIR * 2; i++) set_sprite_prop(OAM_PIR + i, 1 | 0x08);
}

UINT8 foes_hit(Laser *l) BANKED {
    UINT8 k;
    if(b_on && !b_dead){ INT16 dx = wrapd(l->x, b_x), dy = wrapd(l->y, b_y);
        if(dx > -11 && dx < 11 && dy > -11 && dy < 11 && CORE_OPEN()){    /* the core: that hurts it */
            l->life = 0; b_flash = 6; sfx(SFX_HIT); shake(4);
            if(!--b_hp){ b_dead = 90; sfx(SFX_BOOM); shake(90); }
            return 1;
        }
        if(!CORE_OPEN() && dx > -20 && dx < 20 && dy > -22 && dy < 22){ l->life = 0; sfx(SFX_THUD); return 1; }   /* shut: the armour goes clang */
    }
    for(k = 0; k < NPIR; k++){ Pirate *p = &pir[k]; INT16 dx, dy;
        if(p->st != 1) continue;
        dx = wrapd(l->x, p->x); dy = wrapd(l->y, p->y);
        if(dx > -10 && dx < 10 && dy > -10 && dy < 10){
            l->life = 0;
            sfx(SFX_HIT);
            { UINT8 ld = 1 + (UPG(UP_LASER) + 1) / 2; p->hp = p->hp > ld ? p->hp - ld : 0; }
            if(!p->hp){ boom(p); sfx(SFX_BOOM); blast(p->x, p->y, p->kind == PK_MINE);
                sv.gold += SP_GOLD((p->kind == PK_MINE ? 12 : p->kind == PK_DART ? 6 : 5) + sv.zone * 3); gain_xp((p->kind == PK_MINE ? 4 : 2) + sv.zone); shown = 253;   /* (a level-up message covers the window: put it back) */
                pod_drop(p->x, p->y, p->kind); if(elite_kill(p->kind)) toast("JOB COMPLETE!", "DOCK TO GET PAID"); }
            return 1;
        }
    }
    return 0;
}

static UINT8 beast_step(void){                                   /* 1 when it has just died */
    INT16 dx, dy;
    b_t++; if(!(b_t & 7)) b_ang = (b_ang + 1) & 31;
    b_x = BEAST_X + circ[b_ang]; b_y = BEAST_Y + circ[(b_ang + 8) & 31];
    dx = wrapd(shx >> 4, b_x); dy = wrapd(shy >> 4, b_y);
    if(!b_fight && dx > -200 && dx < 200 && dy > -180 && dy < 180){
        b_fight = 1; music(MUS_BOSS);
        if(!(sv.story & ST_BEAST)){ sv.story |= ST_BEAST; banner(BN_BEAST); space_say(SAY_BEAST); shown = 253; }
    } else if(b_fight && (dx > 320 || dx < -320 || dy > 320 || dy < -320)){ b_fight = 0; music(MUS_SPACE); }
    if(b_flash) b_flash--;
    if(b_dead){
        if(!(b_dead & 7)){ Pirate *p = &pir[(b_dead >> 3) % NPIR]; p->st = 2; p->x = b_x - 20 + (rnd() & 39); p->y = b_y - 20 + (rnd() & 39); sfx(SFX_BOOM); flash_t = 2; }
        if(!--b_dead){
            UINT8 n; for(n = 0; n < 40; n++) move_sprite(n, 0, 0);
            sv.story |= ST_DONE; sv.gold += 5000; fame_add(300); b_on = 0;
            space_say(SAY_DAWN);
            gain_xp(200);
            return 1;
        }
    } else if(b_fight){
        if(!(b_t & 63)) orb_fire(b_x, b_y, dx, dy);
        if(dx > -20 && dx < 20 && dy > -20 && dy < 20) hurt(4 + sv.zone / 2, dx, dy, 32);
    }
    return 0;
}

UINT8 foes_step(UINT8 frame) BANKED {
    UINT8 i;
    if(!--spawn_t){ spawn(); spawn_t = b_fight ? 70 + (rnd() & 63) : 120 + (rnd() & 127); }
    if(b_on && beast_step()) return 1;
    pods_step(frame);
    for(i = 0; i < NSPARK; i++) if(spark[i].life){ spark[i].life--; spark[i].x = (spark[i].x + spark[i].dx) & 4095; spark[i].y = (spark[i].y + spark[i].dy) & 4095; }
    for(i = 0; i < NORB; i++){ Orb *o = &orb[i]; INT16 dx, dy;
        if(!o->life) continue;
        o->life--; o->x = (o->x + o->dx) & 4095; o->y = (o->y + o->dy) & 4095;
        dx = wrapd(shx >> 4, o->x); dy = wrapd(shy >> 4, o->y);
        if(dx > -7 && dx < 7 && dy > -7 && dy < 7){ o->life = 0; hurt(3 + sv.zone / 2, dx, dy, 10); }
    }
    for(i = 0; i < NPIR; i++){ Pirate *p = &pir[i]; INT16 dx, dy;
        if(!p->st) continue;
        if(p->st > 1){ if(++p->st > 24) p->st = 0; continue; }
        dx = wrapd(shx >> 4, p->x); dy = wrapd(shy >> 4, p->y);
        if(dx > 320 || dx < -320 || dy > 320 || dy < -320){ p->st = 0; continue; }   /* lost you */
        if(p->kind == PK_DART){
            if(p->ph == DP_AIM){ p->d = dir8(dx, dy); if(!--p->t){ p->ph = DP_DASH; p->t = 34; sfx(SFX_HISS); } }   /* blinks while it aims */
            else if(p->ph == DP_DASH){ p->x += dir_x[p->d] * 4; p->y += dir_y[p->d] * 4; if(!--p->t){ p->ph = DP_DRIFT; p->t = 50; } }
            else if(p->ph == DP_DRIFT){ if(!(frame & 1)){ p->x += dir_x[p->d]; p->y += dir_y[p->d]; } if(!--p->t) p->ph = DP_CHASE; }
            else {
                if(!(frame & 7)) p->d = dir8(dx, dy);
                p->x += dir_x[p->d]; p->y += dir_y[p->d];
                if(dx > -88 && dx < 88 && dy > -72 && dy < 72){ p->ph = DP_AIM; p->t = 28; }
            }
        } else if(p->kind == PK_MINE){
            if(!(frame & 3)){ if(dx > 1) p->x++; else if(dx < -1) p->x--; if(dy > 1) p->y++; else if(dy < -1) p->y--; }
            if(!++p->t || p->t == 128){ if(dx > -88 && dx < 88 && dy > -72 && dy < 72){       /* four orbs, + then x */
                UINT8 q = (p->t & 128) ? 1 : 0; orb_dir(p->x, p->y, q); orb_dir(p->x, p->y, q + 2); orb_dir(p->x, p->y, q + 4); orb_dir(p->x, p->y, q + 6); } }
        } else {
            if((frame & 3) || sv.zone >= 2){                          /* 3/4 of your top speed (all of it, later on) */
                if(dx > 1) p->x++; else if(dx < -1) p->x--;
                if(dy > 1) p->y++; else if(dy < -1) p->y--;
            }
            if(sv.zone && !(((UINT8)(frame + i * 43)) & 127) && dx > -80 && dx < 80 && dy > -64 && dy < 64) orb_fire(p->x, p->y, dx, dy);   /* and they shoot */
        }
        p->x &= 4095; p->y &= 4095;
        if(!inv && dx > -12 && dx < 12 && dy > -12 && dy < 12){ hurt(p->kind == PK_MINE ? 4 + sv.zone : 2 + sv.zone, dx, dy, 24); boom(p); blast(p->x, p->y, 0); }   /* rammed */
    }
    return 0;
}

static void beast_show(UINT8 frame){
    INT16 sx = wrapd(b_x, cam_x), sy = wrapd(b_y, cam_y); UINT8 cx, cy, o, pal = b_flash ? 3 : 4;
    if(!b_on || (b_dead && (frame & 2)) || sx < -32 || sx > 192 || sy < -32 || sy > 168){ for(o = 0; o < 20; o++) move_sprite(OAM_CORE + o, 0, 0); return; }
    for(cx = 0; cx < 6; cx++) for(cy = 0; cy < 3; cy++){
        INT16 x = sx - 16 + cx * 8, y = sy - 8 + cy * 16;          /* OAM position: (top-left + 8, top-left + 16) */
        o = OAM_BODY + cx * 3 + cy;
        set_sprite_prop(o, pal | 0x08);
        if(x <= 0 || x >= 168 || y <= 0 || y >= 160) move_sprite(o, 0, 0); else move_sprite(o, (UINT8)x, (UINT8)y);
    }
    pal = (b_flash ? 3 : CORE_OPEN() ? 5 : 4) | 0x08;
    set_sprite_prop(OAM_CORE, pal); set_sprite_prop(OAM_CORE + 1, pal);
    spr16(OAM_CORE, SP_T_CORE(CORE_OPEN()), sx, sy);
}
void foes_draw(UINT8 frame) BANKED {
    UINT8 i;
    for(i = 0; i < NPIR; i++){ Pirate *p = &pir[i];
        if(!p->st){ hide2(OAM_PIR + i * 2); continue; }
        { UINT8 pal = p->st > 1 ? 3 : p->kind == PK_DART ? 3 : p->kind == PK_MINE ? 5 : 1, t;
          if(p->st > 1) t = SP_T_BOOM(p->st > 10);
          else if(p->kind == PK_DART){ t = SP_T_DART(p->d); if(p->ph == DP_AIM && (frame & 4)) pal = 0; }   /* aiming: flashes red */
          else if(p->kind == PK_MINE) t = SP_T_MINE((frame >> 3) & 1);
          else t = SP_T_PIRATE((frame >> 3) & 1);
          set_sprite_prop(OAM_PIR + i * 2, pal | 0x08); set_sprite_prop(OAM_PIR + i * 2 + 1, pal | 0x08);
          spr16(OAM_PIR + i * 2, t, wrapd(p->x, cam_x), wrapd(p->y, cam_y)); }
    }
    if(b_on) beast_show(frame);
    else for(i = 0; i < NPOD; i++){ Pod *p = &pod[i]; INT16 x = wrapd(p->x, cam_x), y = wrapd(p->y, cam_y);
        if(!p->life || (p->life < 60 && (frame & 4)) || x < -4 || x > 164 || y < -4 || y > 132) move_sprite(12 + i, 0, 0); else move_sprite(12 + i, (UINT8)(x + 4), (UINT8)(y + 12)); }
    for(i = 0; i < NORB; i++){
        INT16 x = wrapd(orb[i].x, cam_x), y = wrapd(orb[i].y, cam_y);
        if(i >= 2 && b_on) break;
        if(i >= 2){ set_sprite_tile(ORB_OAM(i), SPR0 + SP_T_ORB); set_sprite_prop(ORB_OAM(i), 3 | 0x08); }
        if(!orb[i].life || x < -4 || x > 164 || y < -4 || y > 132) move_sprite(ORB_OAM(i), 0, 0); else move_sprite(ORB_OAM(i), (UINT8)(x + 4), (UINT8)(y + 12));
    }
    if(!b_on) for(i = 0; i < NSPARK; i++){
        INT16 x = wrapd(spark[i].x, cam_x), y = wrapd(spark[i].y, cam_y);
        set_sprite_tile(8 + i, SPR0 + SP_T_ORB); set_sprite_prop(8 + i, ((spark[i].life & 2) ? 3 : 2) | 0x08);
        if(!spark[i].life || x < -4 || x > 164 || y < -4 || y > 132) move_sprite(8 + i, 0, 0); else move_sprite(8 + i, (UINT8)(x + 4), (UINT8)(y + 12));
    }
    if(flash_t){ flash_t--; set_bkg_palette_entry(3, 0, flash_t ? RGB(31, 31, 28) : RGB(0, 0, 3)); }   /* (RGB(0,0,3): the space background, sp_space_pal[0]) */
}
