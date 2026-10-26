#pragma bank 19
/* BUGSTAR GBC - the 3D view of space (see space3d.h).
   The galaxy is the flat view's: 16x16 cells of 256 units, one star (a planet and its station) in each, wrapping at 4096. Here every
   star also has a height, and you fly a ship with a heading (yaw) and a climb (pitch). Everything you see is a point in space:
   its offset from the ship (dx, dy, dz) is turned into camera coordinates with the ship's three axes (forward, right, up: sin / cos
   from a table, 64 = 1.0) and drawn at 80 + right * 80 / forward, 60 - up * 80 / forward. Bigger as it comes near: a world is a shaded
   ball drawn from 4 pre-rendered sizes (art: tools/make_space3d.py), a pirate is a 16x16 ship or a dot, and 5 specks of dust fly past
   to show your speed. The star background is one picture that slides as you turn (it is infinitely far, so it only turns).
   Positions of things that move are in 1/16 units (65536 = 4096 units, so subtracting two of them is the short way round for free);
   stars are in units. The ship is shx, shy (shared with the flat view), sz here, yaw (8.8, 256 = a full turn) and pit (-40..40).
   All the maths is done before the VBlank, then the sprites are written from a list.
   Sprites are 8x16 in VRAM bank 1, tile 16 on (S0): ours, then the flat view's pirate, explosion, orb and crate. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "wram.h"
#include "fx.h"
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"
#include "sysmenu.h"
#include "spell.h"
#include "menus.h"
#include "space_art.h"
#include "space.h"
#include "warp.h"
#include "econ.h"
#include "elite.h"
#include "space_int.h"
#include "space3d_art.h"
#include "kart.h"

#include "space3d.h"

#include "space3d_int.h"

static UINT8 msg_t, told3, lab_t;           /* the message timer, the how-to-fly talk (once), frames since the target line was drawn */
#define basis s3_basis
#define dust_born s3_dust_born
#define scan s3_scan

/* ---------------------------------------------------------------- messages on the window's third row */
static void say3(const char *s){ txt_win(1); txt_line(0, 2, s, 20); txt_color(0, 2, 20, 1, TXT_GOLD); txt_win(0); msg_t = 90; }
static void line3(void){                                                    /* "<target> <distance>   A: LAND" */
    char b[24], *d;
    UINT8 s = near_pl != 255 ? near_pl : near_st != 255 ? near_st : tgt;
    d = world_name(b, world_seed(s)); *d++ = ' ';
    if(near_pl != 255) d = scat(d, "A:LAND"); else if(near_st != 255) d = scat(d, "A:DOCK STN"); else d = u16s(d, tgt_d);
    *d = 0;
    txt_win(1); txt_line(0, 2, b, 20); txt_color(0, 2, 20, 1, (near_pl != 255 || near_st != 255) ? TXT_GOLD : TXT_WHITE); txt_win(0);
}

static void gfx3(void){                                                     /* LCD off: everything on the screen, then it fades in */
    s3_load();
    hud_init(); hud_pos(HUD3_Y);
    txt_win(1); txt_line(0, 2, "", 20); txt_win(0);
    music(MUS_SPACE);
    SHOW_BKG; SHOW_SPRITES;
    msg_t = 0; lab_t = 99;
    fade_in();
}
static void redraw3(void){ if(sys_gfx_lost) s3_load(); txt_pal_reset(); space3_pals(); SPRITES_8x16; }
static void hud_back(void){ hud_redraw(); hud_pos(HUD3_Y); txt_win(1); txt_line(0, 2, "", 20); txt_win(0); msg_t = 0; lab_t = 99; }

static void objects_clear(void){
    UINT8 i;
    for(i = 0; i < NP3; i++) pir[i].st = 0;
    for(i = 0; i < NB3; i++) bolt[i].life = 0;
    for(i = 0; i < NO3; i++) orb[i].life = 0;
    for(i = 0; i < NPOD3; i++) pod[i].life = 0;
    for(i = 0; i < NDUST; i++) dust_born(i, 0);
}

/* ---------------------------------------------------------------- the ship is hit */
static void hurt3(UINT8 dmg){
    if(inv) return;
    { UINT8 a = UPG(UP_HULL) + UPG(SK_GUARD) / 3; dmg = dmg > a ? dmg - a : 1; }
    if(UPG(UP_SHIELD) && sv.mp){                                           /* the shield soaks it up: 2 MP for every point */
        UINT8 soak = sv.mp / 2 > dmg ? dmg : (UINT8)(sv.mp / 2);
        sv.mp -= soak * 2; dmg -= soak;
        if(!dmg){ inv = 45; sfx(SFX_SPARK); shake(5); return; }
    }
    sv.hp = sv.hp > dmg ? sv.hp - dmg : 0;
    inv = 60; sfx(SFX_HURT); shake(12);
}

/* ---------------------------------------------------------------- pirates */
static void norm_step(INT16 dx, INT16 dy, INT16 dz, UINT8 speed, INT16 *ox, INT16 *oy, INT16 *oz){   /* a step of `speed` (1/16 unit) along (dx, dy, dz) */
    INT16 m = aabs(dx), t;
    t = aabs(dy); if(t > m) m = t; t = aabs(dz); if(t > m) m = t;
    if(m < 1){ *ox = *oy = *oz = 0; return; }
    *ox = (dx * speed) / m; *oy = (dy * speed) / m; *oz = (dz * speed) / m;
}
static P3 *pirate_spawn(UINT8 n){
    UINT8 i, a = rnd();
    for(i = 0; i < NP3; i++) if(!pir[i].st){
        P3 *p = &pir[i];
        p->x = shx + (INT16)s3_sin[a] * 64; p->y = shy + (INT16)s3_sin[(UINT8)(a + 64)] * 64;      /* 64 * 64 = 4096 = 256 units away */
        p->z = sz + (INT16)(INT8)rnd() * 4;
        p->st = 1; p->t = rnd(); p->hp = 2 + sv.zone / 2 + (sv.level > 8 ? 1 : 0); p->kind = n;
        return p;
    }
    return 0;
}
static UINT8 ar_wave, ar_left, ar_bombs, ar_gap, ar_pause, ar_boss;       /* the Arena: wave, pirates still to come, bombs, spawn gap, pause, the next one is the flagship */
static UINT16 ar_gold, ar_kills;
static UINT8 lvl3(void){ UINT8 l = arena_on ? ar_wave : sv.zone; return l > 8 ? 8 : l; }   /* how hard the pirates are */
static void orb_fire(P3 *p, INT8 sp){
    UINT8 i;
    for(i = 0; i < NO3; i++) if(!orb[i].life){
        B3 *o = &orb[i];
        INT16 dx = unit(shx, p->x), dy = unit(shy, p->y), dz = (sz - p->z) >> 4, vx_, vy_, vz_;
        norm_step(dx, dy, dz, 34, &vx_, &vy_, &vz_); vx_ += sp; vy_ -= sp;
        o->x = p->x; o->y = p->y; o->z = p->z; o->dx = vx_; o->dy = vy_; o->dz = vz_; o->life = 140;
        sfx(SFX_FIRE); return;
    }
}
static void pod_drop(P3 *p){
    UINT8 i;
    if((rnd() & 127) >= 60) return;                                        /* about half */
    for(i = 0; i < NPOD3; i++) if(!pod[i].life){ pod[i].x = p->x; pod[i].y = p->y; pod[i].z = p->z; pod[i].g = rnd() & 7; pod[i].life = 250; return; }
}

/* ---------------------------------------------------------------- the flight (many small functions: one big one makes SDCC crawl) */
static UINT8 cool, turn_h, spawn_t, regen;
static const UINT8 gun_cool3[6] = { 16, 13, 11, 9, 8, 7 };

static void fly(UINT8 k, UINT8 frame){
    INT16 want;
    if(k & J_LEFT){ if(turn_h < 20) turn_h++; yaw -= 280 + turn_h * 12; }
    else if(k & J_RIGHT){ if(turn_h < 20) turn_h++; yaw += 280 + turn_h * 12; }
    else turn_h = 0;
    if(k & J_UP){ if(pit16 < 40 * 256) pit16 += 200; }
    else if(k & J_DOWN){ if(pit16 > -40 * 256) pit16 -= 200; }
    else if(!(frame & 3)){ if(pit16 > 100) pit16 -= 100; else if(pit16 < -100) pit16 += 100; else pit16 = 0; }
    basis();
    if(arena_on){ vel = 0; return; }                                       /* the Arena: the ship hovers */
    want = (k & J_B) ? 56 + 6 * UPG(UP_TURBO) : 24 + 2 * UPG(UP_ENGINE);
    if(near_pl != 255 || near_st != 255) want = 14;                        /* gentle near a world: easy to dock */
    if(vel < want) vel += 2; else if(vel > want) vel -= 3;
    shx += (vfx * vel) >> 6; shy += (vfy * vel) >> 6; sz += (vfz * vel) >> 6;
    if(sz > 3000) sz = 3000;
    if(sz < -3000) sz = -3000;
}
static void shoot(void){                                                    /* along the nose */
    UINT8 i;
    for(i = 0; i < NB3; i++) if(!bolt[i].life){
        B3 *b = &bolt[i];
        b->x = shx; b->y = shy; b->z = sz;
        b->dx = (vfx * 112) >> 6; b->dy = (vfy * 112) >> 6; b->dz = (vfz * 112) >> 6; b->life = 34;
        cool = gun_cool3[UPG(UP_GUNS) > 5 ? 5 : UPG(UP_GUNS)]; sfx(SFX_LASER); return;
    }
}
static void pirate_hit(P3 *p, UINT8 dmg){
    if(p->hp > dmg){ p->hp -= dmg; shake(3); return; }
    p->st = 2; sfx(SFX_BOOM); shake(8); pod_drop(p);
    if(arena_on){ ar_kills++; ar_gold += p->kind ? 80 + 15 * ar_wave : 12 + 4 * ar_wave; elite_kill(1); }
    else if(elite_kill(1)) say3("JOB DONE! DOCK TO GET PAID");
}
static void step_bolts(void){
    UINT8 i, j;
    for(i = 0; i < NB3; i++){ B3 *b = &bolt[i];
        if(!b->life) continue;
        b->life--; b->x += b->dx; b->y += b->dy; b->z += b->dz;
        for(j = 0; j < NP3; j++){ P3 *p = &pir[j];
            INT16 dx, dy, dz;
            if(p->st != 1) continue;
            dx = unit(b->x, p->x); dy = unit(b->y, p->y); dz = (b->z - p->z) >> 4;
            if(aabs(dx) < 22 && aabs(dy) < 22 && aabs(dz) < 22){
                UINT8 dmg = 1 + UPG(UP_LASER) / 2;
                b->life = 0; sfx(SFX_HIT);
                pirate_hit(p, dmg);
                break;
            }
        }
    }
}
static void step_pirates(void){                                             /* they come at you, circle, shoot */
    UINT8 i;
    for(i = 0; i < NP3; i++){ P3 *p = &pir[i];
        INT16 dx, dy, dz, ax, ay, az, dist;
        if(!p->st) continue;
        if(p->st >= 2){ if(++p->st > 24) p->st = 0; continue; }
        dx = unit(shx, p->x); dy = unit(shy, p->y); dz = (sz - p->z) >> 4;
        dist = aabs(dx) > aabs(dy) ? aabs(dx) + aabs(dy) / 2 : aabs(dy) + aabs(dx) / 2;
        if(dist > 700 && !arena_on){ p->st = 0; continue; }                              /* lost interest */
        if(dist > 130) norm_step(dx, dy, dz, 21 + lvl3(), &ax, &ay, &az);
        else { norm_step(-dy, dx, dz / 2, 19, &ax, &ay, &az); if(p->t & 64){ ax = -ax; ay = -ay; } }     /* circle */
        p->x += ax; p->y += ay; p->z += az; p->t++;
        if(p->t % (84 - lvl3() * 4) == 0 && dist < 330 && dist > 50){ orb_fire(p, 0); if(p->kind){ orb_fire(p, 7); orb_fire(p, -7); } }
    }
}
static void step_orbs(void){
    UINT8 i;
    for(i = 0; i < NO3; i++){ B3 *o = &orb[i];
        INT16 dx, dy, dz;
        if(!o->life) continue;
        o->life--; o->x += o->dx; o->y += o->dy; o->z += o->dz;
        dx = unit(o->x, shx); dy = unit(o->y, shy); dz = (o->z - sz) >> 4;
        if(aabs(dx) < 11 && aabs(dy) < 11 && aabs(dz) < 11){ o->life = 0; hurt3(2 + lvl3() / 2); }
    }
}
static void step_pods(void){                                                /* fly into a crate */
    UINT8 i;
    for(i = 0; i < NPOD3; i++){ Pod3 *q = &pod[i];
        INT16 dx, dy, dz, r = 22 + 6 * UPG(UP_MAGNET);
        if(!q->life) continue;
        q->life--;
        dx = unit(q->x, shx); dy = unit(q->y, shy); dz = (q->z - sz) >> 4;
        if(aabs(dx) < r && aabs(dy) < r && aabs(dz) < r){
            UINT8 n = 1 + (rnd() % 3), room = HOLD_CAP() > econ_load() ? HOLD_CAP() - econ_load() : 0;
            q->life = 0;
            if(!room) say3("THE HOLD IS FULL");
            else {
                char b[24], *d;
                if(n > room) n = room;
                sv.el.cargo[q->g] += n; sfx(SFX_PICK);
                d = u16s(b, n); d = scat(d, " "); good_label(q->g, d); while(*d) d++;
                scat(d, " ADDED"); say3(b);
            }
        }
    }
}
static void step_spawn(void){                                               /* new pirates now and then (not at a world or station) */
    UINT8 i, alive = 0, maxp = 1 + (sv.zone > 3 ? 1 : 0) + (sv.level > 10 ? 1 : 0);
    if(spawn_t){ spawn_t--; return; }
    for(i = 0; i < NP3; i++) if(pir[i].st) alive++;
    if(alive < maxp && near_st == 255 && near_pl == 255) pirate_spawn(0);
    spawn_t = 260 + (rnd() & 127);
}
static void step_dust(void){
    UINT8 i, sp = (UINT8)(vel >> 2);
    if(sp < 3) sp = 3;
    for(i = 0; i < NDUST; i++){ if(dust_z[i] <= 8 + sp) dust_born(i, 1); else dust_z[i] -= sp; }
}

/* the sprite list: the cross and the target mark, shots, pirates, crates, dust, then worlds and stations nearest first */
static UINT8 chart(void){                                                   /* SELECT: the galaxy chart, then the warp; 1 if we jumped */
    UINT16 r; UINT8 cell = (UINT8)((((shy >> 4) >> 8) << 4) | ((shx >> 4) >> 8));
    WarpResult wr;
    HIDE_SPRITES;
    r = galaxy_run(cell, 0);
    SHOW_SPRITES;
    hud_back();
    if(r == GAL_NONE) return 0;
    fade_out(); DISPLAY_OFF; hud_hide();
    warp_run(1500, &wr);
    fade_out(); DISPLAY_OFF;
    shx = star_px((UINT8)r) << 4; shy = (star_py((UINT8)r) + 120) << 4; sz = star_z((UINT8)r) * 16; yaw = 0x8000; pit16 = 0; vel = 24; inv = 0;
    objects_clear(); basis();
    gfx3();
    warp_report(&wr);
    if(sv.el.fuel < 3) dlg_say("BOLT", "FUEL IS LOW. DOCK AT A STATION TO REFUEL, OR REST AT YOUR SHIP ON ANY WORLD FOR A LITTLE FREE FUEL.");
    return 1;
}
static void wrecked(void){
    UINT8 n; char b[96], *d;
    for(n = 0; n < 40; n++) move_sprite(n, 0, 0);
    sv.gold -= sv.gold / 4; sv.hp = hud_hpmax(sv.level) / 2; if(!sv.hp) sv.hp = 1;
    d = scat(b, "OUCH! THE PIRATES WRECKED THE SHIP. I LIMPED BACK TO "); d = world_name(d, world_seed(sv.star)); scat(d, ". SOME GOLD FELL OUT.");
    dlg_say("BOLT", b);
}
static void land(UINT8 s){
    char b[64], *d;
    sv.star = s;
    d = scat(b, "LANDING ON "); d = world_name(d, world_seed(s)); d = scat(d, ", A "); d = scat(d, kind_name[star_k[s]]); scat(d, " WORLD.");
    sfx(SFX_STAIRS); dlg_say("BOLT", b);
    if(opt.autosave) slot_write(cur_slot);
}


/* ================================================================ the Arena
   A hovering ship, waves of pirates from all round, every 4th wave a flagship that fires three orbs at once. B drops a bomb (clears the
   orbs, hurts every pirate). Gold for every kill and every wave; a wreck halves the pay. The best wave is kept in the save. */
static void wave_start(void){
    char b[16], *d = scat(b, "WAVE "); d = u16s(d, ar_wave); *d = 0;
    ar_boss = !(ar_wave & 3);
    ar_left = ar_boss ? 3 : (ar_wave / 2 + 2 > 8 ? 8 : ar_wave / 2 + 2);
    ar_pause = 100; ar_gap = 0; say3(b);
}
static void ar_spawn(void){
    UINT8 i, alive = 0, cap = 1 + ar_wave / 2;
    if(cap > NP3) cap = NP3;
    if(ar_pause){ ar_pause--; return; }
    for(i = 0; i < NP3; i++) if(pir[i].st) alive++;
    if(ar_gap) ar_gap--;
    if(ar_left && alive < cap && !ar_gap){
        P3 *p = pirate_spawn(ar_boss);
        if(p){
            if(ar_boss){ p->hp = 10 + 2 * ar_wave; ar_boss = 0; }
            else { p->hp = 2 + ar_wave / 3 + (sv.level > 8 ? 1 : 0); }
            ar_left--; ar_gap = 45;
        }
    } else if(!ar_left && !alive){
        ar_gold += 20 + 8 * ar_wave; sfx(SFX_FANFARE);
        if(!(ar_wave & 3) && ar_bombs < 3) ar_bombs++;
        if(ar_wave < 99) ar_wave++;
        wave_start();
    }
}
static void ar_bomb(void){
    UINT8 i;
    ar_bombs--; sfx(SFX_BOOM); shake(14);
    for(i = 0; i < NO3; i++) orb[i].life = 0;
    for(i = 0; i < NP3; i++) if(pir[i].st == 1) pirate_hit(&pir[i], 6 + UPG(UP_LASER));
}
static void ar_arrow(void){                                                 /* an arrow to the nearest pirate when it is off the screen */
    UINT8 i, best = 255; INT16 bd = 32000;
    for(i = 0; i < NP3; i++) if(pir[i].st == 1){
        INT16 d = aabs(unit(pir[i].x, shx)) + aabs(unit(pir[i].y, shy));
        if(d < bd){ bd = d; best = i; }
    }
    edge_on = best != 255;
    if(edge_on){ ed_x = unit(pir[best].x, shx); ed_y = unit(pir[best].y, shy); ed_z = (pir[best].z - sz) >> 4; }
}
static void ar_line(void){
    char b[24], *d = scat(b, "WAVE "); d = u16s(d, ar_wave); d = scat(d, " BOMB "); d = u16s(d, ar_bombs); d = scat(d, " +"); d = u16s(d, ar_gold); *d = 0;
    txt_win(1); txt_line(0, 2, b, 20); txt_color(0, 2, 20, 1, TXT_WHITE); txt_win(0);
}
static void page_start(void){ fade_out(); DISPLAY_OFF; HIDE_SPRITES; HIDE_WIN; hud_hide(); SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); txt_cls(); }
static void page_wait(const char *foot){ txt_line(2, 16, foot, 16); DISPLAY_ON; }
static UINT8 ar_intro(void){
    char b[24], *d;
    page_start();
    txt_box(0, 0, 20, 18);
    txt_line(5, 1, "THE ARENA", 9); txt_color(5, 1, 9, 1, TXT_GOLD);
    txt_line(2, 3, "WAVES OF PIRATES", 16); txt_line(2, 4, "FLY AT YOU.", 16);
    txt_line(2, 6, "D-PAD", 16); txt_line(8, 6, "AIM", 8);
    txt_line(2, 7, "A", 16); txt_line(8, 7, "FIRE", 8);
    txt_line(2, 8, "B", 16); txt_line(8, 8, "BOMB ALL", 10);
    txt_line(2, 9, "START", 16); txt_line(8, 9, "CASH OUT", 10);
    txt_color(2, 6, 5, 4, TXT_GOLD); txt_color(8, 6, 10, 4, TXT_WHITE);
    txt_line(2, 11, "EVERY 4TH WAVE HAS", 16); txt_line(2, 12, "A BOSS. BIG PAY!", 16); txt_color(2, 11, 16, 2, TXT_GREY);
    d = scat(b, "BEST WAVE "); d = u16s(d, ARENA_BEST); *d = 0;
    txt_line(2, 14, b, 16); txt_color(2, 14, 16, 1, TXT_GOLD);
    txt_line(2, 16, "A: GO   B: BACK", 16); txt_color(2, 16, 16, 1, TXT_GREY);
    DISPLAY_ON;
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A)) return 1; if(HIT(J_B)) return 0; }
}
static UINT8 ar_cash(void){                                                 /* START: cash out? */
    say3("A:CASH OUT B:FIGHT");
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A)) return 1; if(HIT(J_B) || HIT(J_START)){ msg_t = 1; return 0; } }
}
static void ar_results(UINT8 wreck, UINT8 cleared){
    char b[24], *d;
    page_start();
    txt_box(0, 0, 20, 18);
    txt_line(4, 2, wreck ? "SHIP WRECKED!" : "CASHED OUT", 12); txt_color(4, 2, 12, 1, wreck ? TXT_RED : TXT_GOLD);
    d = scat(b, "WAVES CLEARED "); d = u16s(d, cleared); *d = 0; txt_line(2, 5, b, 16);
    d = scat(b, "PIRATES SHOT "); d = u16s(d, ar_kills); *d = 0; txt_line(2, 6, b, 16);
    d = scat(b, "GOLD EARNED "); d = u16s(d, ar_gold); *d = 0; txt_line(2, 8, b, 16); txt_color(2, 8, 16, 1, TXT_GOLD);
    if(wreck) txt_line(2, 9, "HALF LOST", 16);
    if(cleared > ARENA_BEST){ txt_line(2, 11, "NEW BEST WAVE!", 14); txt_color(2, 11, 14, 1, TXT_GOLD); }
    else { d = scat(b, "BEST WAVE "); d = u16s(d, ARENA_BEST); *d = 0; txt_line(2, 11, b, 16); }
    page_wait("A: CONTINUE");
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B)) break; }
}
void arena_run(void) BANKED {
    UINT16 sx = shx, sy = shy; INT16 s_z = sz, s_p = pit16, s_v = vel; UINT16 s_yaw = yaw;
    UINT8 k, frame = 0, i, wreck = 0, cleared;
    if(!ar_intro()){ fade_out(); DISPLAY_OFF; return; }
    fade_out(); DISPLAY_OFF;
    cpu_fast(); space_tables();
    if(!rng) rng = 0xA5A5;
    rng ^= DIV_REG; if(!rng) rng = 1;
    arena_on = 1; edge_on = 0;
    yaw = 0; pit16 = 0; vel = 0; inv = 0; cool = 0; turn_h = 0; regen = 0;
    ar_wave = 1; ar_bombs = 2; ar_gold = 0; ar_kills = 0;
    objects_clear(); basis();
    gfx3();
    wave_start(); ar_line(); msg_t = 90;
    for(;;){
        input_poll(); k = keys; frame++;
        fly(k, frame);
        if(cool) cool--;
        if(inv) inv--;
        if((k & J_A) && !cool) shoot();
        if(HIT(J_B) && ar_bombs) ar_bomb();
        step_bolts(); step_pirates(); step_orbs(); step_pods(); ar_spawn(); step_dust(); ar_arrow();
        s3_render(frame);
        wait_vbl_done();
        s3_flush(frame);
        if(msg_t){ if(!--msg_t){ lab_t = 99; } }
        else if(++lab_t >= 8){ lab_t = 0; ar_line(); }
        hud_update();
        if(UPG(UP_SHIELD) && !inv && sv.mp < hud_mpmax(sv.level)){ if(++regen >= 60 - 8 * UPG(UP_SHIELD)){ regen = 0; sv.mp++; } }
        if(!sv.hp){ wreck = 1; break; }
        if(HIT(J_START) && ar_cash()) break;
    }
    for(i = 0; i < 40; i++) move_sprite(i, 0, 0);
    cleared = ar_wave - 1;
    if(wreck){ ar_gold >>= 1; sv.hp = hud_hpmax(sv.level) / 2; if(!sv.hp) sv.hp = 1; }
    sv.gold += ar_gold;
    ar_results(wreck, cleared);
    if(cleared > ARENA_BEST) ARENA_BEST = cleared;
    if(opt.autosave) slot_write(cur_slot);
    arena_on = 0; edge_on = 0; objects_clear();
    shx = sx; shy = sy; sz = s_z; pit16 = s_p; vel = s_v; yaw = s_yaw;
    fade_out(); DISPLAY_OFF;
    hud_hide(); SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); txt_cls();
}

/* ================================================================ EXTRAS: what you have unlocked */
#define HELL_OPEN (sv.level >= 8)
#define MOON_OPEN (sv.level >= 10)
#define KART_OPEN (sv.level >= 5)
#define EX_N 5
static void ex_card(UINT8 y, UINT8 on, const char *nm, const char *st, UINT8 c1, const char *info){
    txt_box(1, y, 18, 4);
    txt_line(3, y + 1, on ? S_CURSOR : " ", 1); txt_line(4, y + 1, nm, 9); txt_color(3, y + 1, 12, 1, on ? TXT_GOLD : TXT_WHITE);
    txt_line(14, y + 1, st, 4); txt_color(14, y + 1, 4, 1, c1);
    txt_line(4, y + 2, info, 14); txt_color(4, y + 2, 14, 1, TXT_GREY);
}
static void ex_one(UINT8 i, UINT8 y, UINT8 on){
    char b[24], *d;
    switch(i){
    case 0: if(V3_OPEN) ex_card(y, on, "3D VIEW", V3_ON ? "ON" : "OFF", TXT_GOLD, "A: SWITCH"); else ex_card(y, on, "3D VIEW", "LOCK", TXT_RED, "NEED RANK POOR"); break;
    case 1: if(ARENA_OPEN){ d = scat(b, "BEST WAVE "); d = u16s(d, ARENA_BEST); *d = 0; ex_card(y, on, "ARENA", "OPEN", TXT_GOLD, b); }
            else ex_card(y, on, "ARENA", "LOCK", TXT_RED, "NEED RANK AVER"); break;
    case 2: if(HELL_OPEN){ d = scat(b, "BEST DEPTH "); d = u16s(d, EX_HELL); *d = 0; ex_card(y, on, "HELL DEEP", "OPEN", TXT_GOLD, b); }
            else ex_card(y, on, "HELL DEEP", "LOCK", TXT_RED, "NEED LEVEL 8"); break;
    case 3: if(MOON_OPEN){ d = scat(b, "BEST DEPTH "); d = u16s(d, EX_MOON); *d = 0; ex_card(y, on, "MOON DEEP", "OPEN", TXT_GOLD, b); }
            else ex_card(y, on, "MOON DEEP", "LOCK", TXT_RED, "NEED LEVEL 10"); break;
    default: if(KART_OPEN){ if(EX_KART){ d = scat(b, "BEST LAP "); d = u16s(d, EX_KART / 60); d = scat(d, "."); d = u16s(d, (EX_KART % 60) * 10 / 60); *d = 0; } else scat(b, "3D KART RACE"); ex_card(y, on, "KART WORLD", "OPEN", TXT_GOLD, b); }
            else ex_card(y, on, "KART WORLD", "LOCK", TXT_RED, "NEED LEVEL 5"); break;
    }
}
static void ex_draw(UINT8 sel, UINT8 in_space, const char *msg){
    UINT8 top = sel > 2 ? sel - 2 : 0, k;
    txt_cls();
    txt_box(0, 0, 20, 18);
    txt_line(7, 1, "EXTRAS", 6); txt_color(7, 1, 6, 1, TXT_GOLD);
    for(k = 0; k < 3; k++) ex_one(top + k, 3 + k * 4, top + k == sel);
    txt_line(18, 3, top ? "^" : " ", 1); txt_line(18, 13, top + 3 < EX_N ? "v" : " ", 1);
    txt_line(2, 15, msg, 16); txt_color(2, 15, 16, 1, TXT_RED);
    txt_line(2, 16, "A: GO  B: BACK", 16); txt_color(2, 16, 16, 1, TXT_GREY);
}
UINT8 extras_run(UINT8 in_space) BANKED {
    UINT8 sel = 0, on = 0, d = 1;
    const char *msg = "";
    unlocks_update();
    for(;;){
        if(d){ ex_draw(sel, in_space, msg); d = 0; if(!on){ txt_pal_reset(); DISPLAY_ON; on = 1; } }
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) return 0;
        if(HIT(J_UP)){ sel = sel ? sel - 1 : EX_N - 1; msg = ""; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_DOWN)){ sel = sel < EX_N - 1 ? sel + 1 : 0; msg = ""; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_A)){
            d = 1; msg = "";
            if(sel == 0){
                if(!V3_OPEN){ msg = "NOT YET UNLOCKED"; sfx(SFX_HURT); }
                else { sv.el.rsv[0] ^= 2; sfx(SFX_OK); if(in_space) return 1; }
            } else if(sel == 1){
                if(!ARENA_OPEN){ msg = "NOT YET UNLOCKED"; sfx(SFX_HURT); }
                else if(!in_space){ msg = "FLY TO PLAY IT"; sfx(SFX_HURT); }
                else if(sv.hp * 2 < hud_hpmax(sv.level)){ msg = "REPAIR FIRST"; sfx(SFX_HURT); }
                else return 2;
            } else if(sel == 4){
                if(!KART_OPEN){ msg = "NOT YET UNLOCKED"; sfx(SFX_HURT); }
                else { fade_out(); DISPLAY_OFF; kart_run(); DISPLAY_OFF; txt_pal_reset(); on = 0; }
            } else {
                if(sel == 2 ? !HELL_OPEN : !MOON_OPEN){ msg = "NOT YET UNLOCKED"; sfx(SFX_HURT); }
                else if(in_space){ msg = "LAND ON A WORLD"; sfx(SFX_HURT); }
                else if(sv.hp * 2 < hud_hpmax(sv.level)){ msg = "HEAL FIRST"; sfx(SFX_HURT); }
                else return sel == 2 ? 3 : 4;
            }
        }
    }
}

int space3d_run(UINT8 keep) BANKED {
    UINT8 k, frame = 0, out = SP_LAND, i;
    fade_out(); DISPLAY_OFF;
    cpu_fast();
    space_tables();
    if(!rng) rng = 0xA5A5;
    rng ^= DIV_REG; if(!rng) rng = 1;
    if(!keep){                                                              /* start beside the home world, facing it */
        shx = star_px(sv.star) << 4; shy = (star_py(sv.star) + 110) << 4; sz = star_z(sv.star) * 16; yaw = 0x8000; pit16 = 0;
    }
#ifdef TEST_DOCK
    shx = (star_px(sv.star) + ST_DX - 30) << 4; shy = (star_py(sv.star) + 30) << 4; sz = (star_z(sv.star) + 8) * 16; yaw = 0x5200; told3 = 1;     /* (test builds: just short of the station) */
#endif
    vel = 24; inv = 0; tgt = sv.star; cool = 0; turn_h = 0; spawn_t = 200; regen = 0;
    objects_clear();
    basis();
    gfx3();
    if(!told3){ told3 = 1; space_say(SAY_HOW3); }
    if(sv.cores >= 8 && !(sv.story & ST_DONE) && !keep) space_say(SAY_BEAST3);
    for(;;){
        input_poll(); k = keys; frame++;
        fly(k, frame);
        scan();
        if(cool) cool--;
        if(inv) inv--;
        if((k & J_A) && !cool) shoot();
        step_bolts(); step_pirates(); step_orbs(); step_pods(); step_spawn(); step_dust();
        s3_render(frame);
        wait_vbl_done();
        s3_flush(frame);
        if(msg_t){ if(!--msg_t){ lab_t = 99; txt_win(1); txt_line(0, 2, "", 20); txt_win(0); } }
        else if(++lab_t >= 8){ lab_t = 0; line3(); }
        hud_update();
        if(UPG(UP_SHIELD) && !inv && sv.mp < hud_mpmax(sv.level)){ if(++regen >= 60 - 8 * UPG(UP_SHIELD)){ regen = 0; sv.mp++; } }
        if(!sv.hp){ wrecked(); break; }
        if(HIT(J_A)){
            if(near_pl != 255){ land(near_pl); break; }
            if(near_st != 255){
                if(elite_dock(near_st)){ out = SP_FLIP; break; }
                hud_back(); inv = 60;
                shx -= (vfx * 70) >> 2; shy -= (vfy * 70) >> 2; vel = 24;     /* back away so A does not dock again at once */
                objects_clear();
                continue;
            }
        }
        if(HIT(J_SELECT)){ chart(); continue; }
        if(HIT(J_START)){
            int sr;
            sys_space = 1; sr = sysmenu_run(redraw3); sys_space = 0;
            if(sr == SYS_TITLE){ out = SP_TITLE; break; }
            if(sr == SYS_FLIP){ out = SP_FLIP; break; }
            if(sr == SYS_ARENA){ arena_run(); out = SP_FLIP; break; }
            hud_back();
        }
    }
    fade_out(); DISPLAY_OFF;
    for(i = 0; i < 40; i++) move_sprite(i, 0, 0);
    hud_hide(); SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); txt_cls();
    DISPLAY_ON;
    return out;
}
