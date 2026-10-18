#pragma bank 6
/* BUGSTAR GBC - step 8: space and the ship.
   Space is 256x256 metatiles of 16x16 px (4096 px each way, wrapping), cut into 16x16 cells of 256 px. Each cell holds one star
   with its world: star n = cell (n % 16, n / 16), and star 0 (the top-left cell) is the home world. A star's world, its kind and
   its name all come from world_seed(n) (save.h), so the planet you land on is the one the map showed.
   Nothing of space is stored either: every metatile is worked out from the cell's star and a small hash when it scrolls into view.
   The planets are drawn in BG palettes 4..7 by the cell's place on a checkerboard of 2x2 cells (the screen never shows more than
   2x2 cells, so the four planets in view always have a palette each); each palette is set to its planet's kind as you fly.
   The starfield borrows text palette 3 (grey: nothing else on this screen uses it).
   You fly with the d-pad (8 ways, with drift), B boosts, A shoots; over a planet A lands. Pirate drones chase you; ramming hurts
   both of you. An arrow points at the nearest other world.
   With all 8 data cores back (step 10) the Technobeast waits halfway across space: armour that shrugs off lasers, a core that opens
   and glows for a moment, orbs it spits at you. The arrow points at it. Beat it and the ending plays (SP_END). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"
#include "sysmenu.h"
#include "menus.h"
#include "space_art.h"
#include "space.h"

#define SPR0      64             /* sprite tiles in VRAM bank 1 (the starfield uses bank-1 tiles 0..31) */
#define OAM_SHIP  20             /* 2 sprites */
#define OAM_LAS   22             /* 4 lasers x 2 */
#define OAM_PIR   30             /* 3 pirates x 2 */
#define OAM_ARR   36             /* the arrow, 2 */
#define NLAS      4
#define NPIR      3
#define VIEW_W    11
#define VIEW_H    9
#define SHIP_SX   80             /* the ship's middle on the screen */
#define SHIP_SY   64
#define VMAX      32             /* top speed, 1/16 px a frame: 2 px */
#define VBOOST    56             /* with B held: 3.5 px */

static UINT8  star_j[256];       /* where each star's planet sits in its cell: x | y << 4 (metatiles, 2..12) */
static UINT8  star_k[256];       /* and its kind */
static UINT16 shx, shy;          /* the ship, 1/16 px (so 65536 = 4096 px: it wraps by itself) */
static INT16  vx, vy;
static UINT8  dir;               /* 0 = up, clockwise */
static UINT16 cam_x, cam_y;      /* top-left of the screen, world px */
static UINT8  left_mt, top_mt;
static UINT8  pk[4];             /* the kind each planet palette (4..7) holds now */
static UINT8  o1, o2;            /* the starfield hash's offsets, from the seed */
static UINT16 rng;
static UINT8  told;              /* the how-to-fly talk, once per power-on */
static UINT8  inv;               /* frames the ship can not be hurt (after a hit) */

typedef struct { UINT16 x, y; INT8 dx, dy; UINT8 life; } Laser;
typedef struct { UINT16 x, y; UINT8 st, hp; } Pirate;       /* st: 0 none, 1 flying, 2.. exploding (counts up) */
static Laser  las[NLAS];
static Pirate pir[NPIR];
static UINT8  b_on, b_dead;      /* the Technobeast: here at all, dying (frames left) */
static UINT16 b_x, b_y;          /* its middle, world px */

static const INT8 dir_x[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const INT8 dir_y[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const INT8 thr_x[8] = { 0, 2, 3, 2, 0, -2, -3, -2 };   /* thrust: diagonals a bit less on each axis */
static const INT8 thr_y[8] = { -3, -2, 0, 2, 3, 2, 0, -2 };
static const palette_color_t space_twinkle[2] = { RGB(31, 31, 31), RGB(20, 24, 31) };

static UINT8 rnd(void){ rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; return (UINT8)rng; }
static INT16 wrapd(UINT16 a, UINT16 b){ INT16 d = (INT16)((a - b) & 4095); return d > 2048 ? d - 4096 : d; }   /* a - b, the short way round */
static UINT16 star_px(UINT8 s){ return ((UINT16)(s & 15) << 8) + ((star_j[s] & 15) << 4) + 16; }   /* a planet's middle, world px */
static UINT16 star_py(UINT8 s){ return ((UINT16)(s >> 4) << 8) + ((star_j[s] >> 4) << 4) + 16; }

/* ------------------------------------------------------------ the map */
static UINT8 pal_of;
static UINT8 sky(UINT8 x, UINT8 y){                           /* the metatile at (x, y); its palette goes in pal_of */
    UINT8 s = (y & 0xF0) | (x >> 4), j = star_j[s], dx = (x & 15) - (j & 15), dy = (y & 15) - (j >> 4), r;
    if(dx < 2 && dy < 2){ pal_of = 4 + ((x >> 4) & 1) + (((y >> 4) & 1) << 1); return SP_PL_TL + (dy << 1) + dx; }
    pal_of = 3;
    r = sp_perm[(UINT8)(sp_perm[(UINT8)(x + o1)] + y + o2)];
    return r < 150 ? SP_STAR0 : r < 222 ? SP_STAR1 : r < 246 ? SP_STAR2 : SP_STAR3;
}
static void draw_mt(UINT8 x, UINT8 y){
    UINT8 mt = sky(x, y), t[4], a[4], bx2 = (x & 15) << 1, by2 = (y & 15) << 1;
    t[0] = mt * 4; t[1] = mt * 4 + 1; t[2] = mt * 4 + 2; t[3] = mt * 4 + 3;
    a[0] = a[1] = a[2] = a[3] = 0x08 | pal_of;
    VBK_REG = VBK_BANK_1; set_bkg_tiles(bx2, by2, 2, 2, a);
    VBK_REG = VBK_BANK_0; set_bkg_tiles(bx2, by2, 2, 2, t);
}
static void draw_all(void){
    UINT8 i, j;
    left_mt = (UINT8)(cam_x >> 4); top_mt = (UINT8)(cam_y >> 4);
    for(j = 0; j < VIEW_H + 1; j++) for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt + j);
}
static void scroll_fill(void){
    UINT8 l = (UINT8)(cam_x >> 4), t = (UINT8)(cam_y >> 4), i;
    while(l != left_mt){
        if((UINT8)(l - left_mt) < 128){ left_mt++; for(i = 0; i < VIEW_H + 1; i++) draw_mt(left_mt + VIEW_W - 1, top_mt + i); }
        else { left_mt--; for(i = 0; i < VIEW_H + 1; i++) draw_mt(left_mt, top_mt + i); }
    }
    while(t != top_mt){
        if((UINT8)(t - top_mt) < 128){ top_mt++; for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt + VIEW_H); }
        else { top_mt--; for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt); }
    }
}
static void planet_pals(void){                                 /* give each planet palette the kind of the planet nearest in its colour class */
    UINT16 px = shx >> 4, py = shy >> 4;
    UINT8 cx = (UINT8)(((px - 128) & 4095) >> 8), cy = (UINT8)(((py - 128) & 4095) >> 8), i;
    for(i = 0; i < 4; i++){
        UINT8 x = (cx + (i & 1)) & 15, y = (cy + (i >> 1)) & 15, p = (x & 1) + ((y & 1) << 1), k = star_k[(y << 4) | x];
        if(pk[p] != k){ pk[p] = k; set_bkg_palette(4 + p, 1, &sp_kind_pal[k * 4]); }
    }
}
static void camera(void){ cam_x = ((shx >> 4) - SHIP_SX) & 4095; cam_y = ((shy >> 4) - SHIP_SY) & 4095; }
static void pals(void){
    set_bkg_palette(3, 1, sp_space_pal);
    pk[0] = pk[1] = pk[2] = pk[3] = 255;
    planet_pals();
}
static void redraw(void){ SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; txt_pal_reset(); pals(); draw_all(); SPRITES_8x16; }

/* ------------------------------------------------------------ sprites */
static void spr16(UINT8 oam, UINT8 tile, INT16 sx, INT16 sy){  /* a 16x16 sprite with its middle at screen (sx, sy); off screen hides it */
    if(sx < -8 || sx > 168 || sy < -8 || sy > 136){ move_sprite(oam, 0, 0); move_sprite(oam + 1, 0, 0); return; }
    set_sprite_tile(oam, SPR0 + tile); set_sprite_tile(oam + 1, SPR0 + tile + 2);
    move_sprite(oam, (UINT8)sx, (UINT8)(sy + 8)); move_sprite(oam + 1, (UINT8)(sx + 8), (UINT8)(sy + 8));
}
static void hide2(UINT8 oam){ move_sprite(oam, 0, 0); move_sprite(oam + 1, 0, 0); }
static void sprite_pals(UINT8 oam, UINT8 n, UINT8 pal){ while(n--){ set_sprite_prop(oam, pal | 0x08); oam++; } }

/* ------------------------------------------------------------ the HUD line for a planet you are over */
static UINT8 shown;                                            /* the star whose label is up, 255 for the HUD, 253 = redraw whichever */
static void label(UINT8 s){
    char b[24], *d;
    if(s == shown) return;
    shown = s;
    if(s == 255){ hud_redraw(); return; }
    txt_win(1);
    d = world_name(b, world_seed(s)); *d++ = ' '; scat(d, kind_name[star_k[s]]);
    txt_line(0, 0, " ", 1); txt_line(1, 0, b, 19); txt_color(0, 0, 20, 1, TXT_GOLD);
    txt_line(0, 1, s == sv.star ? " A: LAND (HOME BASE)" : " A: LAND", 20); txt_color(0, 1, 20, 1, TXT_WHITE);
    txt_win(0);
}

/* ------------------------------------------------------------ flying */
static UINT8 over_star(void){                                   /* the star whose planet is under the ship, or 255 */
    UINT16 px = shx >> 4, py = shy >> 4; UINT8 s = (UINT8)(((py >> 8) << 4) | (px >> 8));
    INT16 dx = wrapd(px, star_px(s)), dy = wrapd(py, star_py(s));
    return (dx > -18 && dx < 18 && dy > -18 && dy < 18) ? s : 255;
}
static UINT8 near_any(UINT16 px, UINT16 py, INT16 r){             /* a planet within r px of (px, py)? (its own cell is enough: planets sit inside) */
    UINT8 s = (UINT8)(((py >> 8) << 4) | (px >> 8)); INT16 dx = wrapd(px, star_px(s)), dy = wrapd(py, star_py(s));
    return dx > -r && dx < r && dy > -r && dy < r;
}
static INT16 drag(INT16 v){
    if(v > 0){ v -= 1 + (v >> 4); if(v < 0) v = 0; }
    else if(v < 0){ v += 1 + ((-v) >> 4); if(v > 0) v = 0; }
    return v;
}
static void arrow(void){                                         /* point at the nearest other world, if it is off the screen */
    UINT16 px = shx >> 4, py = shy >> 4; UINT8 cx = (UINT8)(px >> 8), cy = (UINT8)(py >> 8), i, j, best = 255, d;
    INT16 bdx = 0, bdy = 0; UINT16 bd = 0xFFFF;
    if(!opt.arrow){ hide2(OAM_ARR); return; }
    if(b_on && !b_dead){ bdx = wrapd(b_x, px); bdy = wrapd(b_y, py); best = 0; goto aim; }
    for(j = 0; j < 3; j++) for(i = 0; i < 3; i++){
        UINT8 s = (UINT8)((((cy + j - 1) & 15) << 4) | ((cx + i - 1) & 15));
        INT16 dx = wrapd(star_px(s), px), dy = wrapd(star_py(s), py);
        UINT16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, m = ax > ay ? ax + ay / 2 : ay + ax / 2;
        if(m > 40 && m < bd){ bd = m; best = s; bdx = dx; bdy = dy; }
    }
aim:
    if(best == 255 || (bdx > -88 && bdx < 88 && bdy > -72 && bdy < 72)){ hide2(OAM_ARR); return; }
    { UINT16 ax = bdx < 0 ? -bdx : bdx, ay = bdy < 0 ? -bdy : bdy;
      if(ax > ay * 2) d = bdx > 0 ? 2 : 6;
      else if(ay > ax * 2) d = bdy > 0 ? 4 : 0;
      else d = bdx > 0 ? (bdy > 0 ? 3 : 1) : (bdy > 0 ? 5 : 7); }
    spr16(OAM_ARR, SP_T_ARROW(d), SHIP_SX + dir_x[d] * ((d & 1) ? 22 : 30), SHIP_SY + dir_y[d] * ((d & 1) ? 22 : 30));
}
static void fire(void){
    UINT8 i;
    for(i = 0; i < NLAS; i++) if(!las[i].life){
        las[i].x = shx >> 4; las[i].y = shy >> 4;
        las[i].dx = dir_x[dir] * 5 + (INT8)(vx >> 4); las[i].dy = dir_y[dir] * 5 + (INT8)(vy >> 4);
        las[i].life = 36; sfx(SFX_LASER); return;
    }
}
static void spawn(void){
    UINT8 i, d; UINT16 px = shx >> 4, py = shy >> 4;
    if(near_any(px, py, 96)) return;                             /* planets are safe */
    for(i = 0; i < NPIR; i++) if(!pir[i].st){
        d = rnd() & 7;
        pir[i].x = (px + dir_x[d] * 112) & 4095; pir[i].y = (py + dir_y[d] * 100) & 4095;
        pir[i].st = 1; pir[i].hp = 2 + (sv.zone > 3 ? 3 : sv.zone) / 2;
        return;
    }
}
static void boom(Pirate *p){ p->st = 2; }
static void hurt(UINT8 dmg, INT16 dx, INT16 dy, INT8 push){     /* the ship is hit; dx, dy: from what hit it to the ship */
    if(inv) return;
    sv.hp = sv.hp > dmg ? sv.hp - dmg : 0;
    inv = 60; sfx(SFX_HURT);
    vx = dx > 0 ? push : -push; vy = dy > 0 ? push : -push;       /* knocked away */
}

/* ------------------------------------------------------------ the Technobeast */
#define BEAST_X  2048            /* its lair, where four cells meet: halfway across space from home, both ways */
#define BEAST_Y  2048
#define BEAST_HP 36
#define OAM_CORE 0               /* 2 sprites: drawn over the body */
#define OAM_BODY 2               /* 18 sprites */
#define OAM_ORB  38              /* 2 orbs, one 8x16 sprite each */
typedef struct { UINT16 x, y; INT8 dx, dy; UINT8 life; } Orb;
static Orb    orb[2];
static UINT8  b_hp, b_flash, b_fight, b_t, b_ang;
static const INT8 circ[32] = { 0, 6, 11, 17, 21, 25, 28, 29, 30, 29, 28, 25, 21, 17, 11, 6, 0, -6, -11, -17, -21, -25, -28, -29, -30, -29, -28, -25, -21, -17, -11, -6 };
#define CORE_OPEN() ((b_t & 127) < 56)   /* open a little under half the time */
static void beast_hide(void){ UINT8 i; for(i = 0; i < 20; i++) move_sprite(OAM_CORE + i, 0, 0); }
static void beast_show(UINT8 frame){
    INT16 sx = wrapd(b_x, cam_x), sy = wrapd(b_y, cam_y); UINT8 cx, cy, o, pal = b_flash ? 3 : 4;
    if(!b_on || (b_dead && (frame & 2)) || sx < -32 || sx > 192 || sy < -32 || sy > 168){ beast_hide(); return; }
    for(cx = 0; cx < 6; cx++) for(cy = 0; cy < 3; cy++){
        INT16 x = sx - 16 + cx * 8, y = sy - 8 + cy * 16;          /* OAM position: (top-left + 8, top-left + 16) */
        o = OAM_BODY + cx * 3 + cy;
        set_sprite_prop(o, pal | 0x08);
        if(x <= 0 || x >= 168 || y <= 0 || y >= 160) move_sprite(o, 0, 0); else move_sprite(o, (UINT8)x, (UINT8)y);
    }
    set_sprite_prop(OAM_CORE, (b_flash ? 3 : CORE_OPEN() ? 5 : 4) | 0x08); set_sprite_prop(OAM_CORE + 1, (b_flash ? 3 : CORE_OPEN() ? 5 : 4) | 0x08);
    spr16(OAM_CORE, SP_T_CORE(CORE_OPEN()), sx, sy);
}
static void orb_fire(INT16 dx, INT16 dy){                        /* spit an orb the way of (dx, dy) */
    UINT8 i, d; UINT16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if(ax > ay * 2) d = dx > 0 ? 2 : 6; else if(ay > ax * 2) d = dy > 0 ? 4 : 0; else d = dx > 0 ? (dy > 0 ? 3 : 1) : (dy > 0 ? 5 : 7);
    for(i = 0; i < 2; i++) if(!orb[i].life){ orb[i].x = b_x; orb[i].y = b_y; orb[i].dx = dir_x[d] * 2; orb[i].dy = dir_y[d] * 2; orb[i].life = 150; sfx(SFX_FIRE); return; }
}

int space_run(void) BANKED {
    UINT8 k, i, frame = 0, cool = 0, spawn_t = 200;
    UINT32 ws = sv.seed;
    DISPLAY_OFF;
    cpu_fast();
    for(k = 0; k < 40; k++) move_sprite(k, 0, 0);
    { UINT32 mul = 0;                                             /* where every star is, and its kind: world_seed(i) without 256 multiplies */
      for(i = 0;; i++){
        ws = sv.seed ^ mul; mul += 0x9E3779B1UL;
        star_j[i] = (UINT8)((2 + (UINT8)(ws >> 16) % 11) | ((2 + (UINT8)(ws >> 20) % 11) << 4));
        star_k[i] = world_kind(ws);
        if(i == 255) break;
      } }
    o1 = (UINT8)sv.seed; o2 = (UINT8)(sv.seed >> 24); rng = (UINT16)sv.seed | 1;
    shx = star_px(sv.star) << 4; shy = (star_py(sv.star) - 36) << 4; vx = 0; vy = -24; dir = 0;
#ifdef TEST_BEAST
    sv.cores = 8; told = 1; sv.level = 30; sv.hp = hud_hpmax(30); shx = (UINT16)BEAST_X << 4; shy = (UINT16)(BEAST_Y + 72) << 4;   /* (test builds: just below the beast) */
#endif
    for(i = 0; i < NLAS; i++) las[i].life = 0;
    for(i = 0; i < NPIR; i++) pir[i].st = 0;
    inv = 0;
    b_on = sv.cores >= 8 && !(sv.story & ST_DONE); b_hp = BEAST_HP; b_flash = b_fight = b_dead = 0; orb[0].life = orb[1].life = 0;
#ifdef TEST_BEAST
    b_hp = 3;
#endif
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, SP_NMT * 4, sp_bg);
    set_sprite_data(SPR0, SP_NSPR, sp_spr);
    VBK_REG = VBK_BANK_0;
    set_sprite_palette(0, 6, sp_spr_pal);
    sprite_pals(OAM_SHIP, 2, 0); sprite_pals(OAM_LAS, NLAS * 2, 2); sprite_pals(OAM_PIR, NPIR * 2, 1); sprite_pals(OAM_ARR, 2, 2); sprite_pals(OAM_ORB, 2, 3);
    for(i = 0; i < 18; i++) set_sprite_tile(OAM_BODY + i, SPR0 + SP_T_BEAST(i / 3, i % 3));
    set_sprite_tile(OAM_ORB, SPR0 + SP_T_ORB); set_sprite_tile(OAM_ORB + 1, SPR0 + SP_T_ORB);
    SPRITES_8x16;
    txt_pal_reset(); pals();
    camera(); SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; draw_all();
    hud_init(); shown = 255;
    music(MUS_SPACE);
    SHOW_BKG; SHOW_SPRITES;
    DISPLAY_ON;
    spr16(OAM_SHIP, SP_T_SHIP(dir), SHIP_SX, SHIP_SY);
    if(!told){ told = 1; dlg_say("BOLT", "SPACE! STEER WITH THE D-PAD. B GOES FASTER AND A SHOOTS. THE ARROW POINTS TO THE NEAREST WORLD: FLY OVER ONE AND PRESS A TO LAND."); }
    for(;;){
        input_poll();
        k = keys; frame++;
        /* the ship */
        if(k & (J_UP | J_DOWN | J_LEFT | J_RIGHT)){
            if(k & J_UP) dir = (k & J_RIGHT) ? 1 : (k & J_LEFT) ? 7 : 0;
            else if(k & J_DOWN) dir = (k & J_RIGHT) ? 3 : (k & J_LEFT) ? 5 : 4;
            else dir = (k & J_RIGHT) ? 2 : 6;
            vx += thr_x[dir]; vy += thr_y[dir];
        } else if(frame & 1){ vx = drag(vx); vy = drag(vy); }
        { INT16 m = (k & J_B) ? VBOOST : VMAX;
          if(k & J_B){ vx += thr_x[dir]; vy += thr_y[dir]; }
          if(vx > m) vx = m; if(vx < -m) vx = -m; if(vy > m) vy = m; if(vy < -m) vy = -m; }
        shx += vx; shy += vy;
        camera(); scroll_fill();
        /* lasers */
        for(i = 0; i < NLAS; i++){ Laser *l = &las[i];
            if(!l->life) continue;
            l->life--; l->x = (l->x + l->dx) & 4095; l->y = (l->y + l->dy) & 4095;
            if(b_on && !b_dead){ INT16 dx = wrapd(l->x, b_x), dy = wrapd(l->y, b_y);
                if(dx > -11 && dx < 11 && dy > -11 && dy < 11 && CORE_OPEN()){    /* the core: that hurts it */
                    l->life = 0; b_flash = 6; sfx(SFX_HIT);
                    if(!--b_hp){ b_dead = 90; sfx(SFX_BOOM); }
                    continue;
                }
                if(!CORE_OPEN() && dx > -20 && dx < 20 && dy > -22 && dy < 22){ l->life = 0; sfx(SFX_THUD); continue; }   /* shut: the armour goes clang */
            }
            for(k = 0; k < NPIR; k++){ Pirate *p = &pir[k]; INT16 dx, dy;
                if(p->st != 1) continue;
                dx = wrapd(l->x, p->x); dy = wrapd(l->y, p->y);
                if(dx > -10 && dx < 10 && dy > -10 && dy < 10){
                    l->life = 0;
                    sfx(SFX_HIT);
                    if(!--p->hp){ boom(p); sfx(SFX_BOOM); sv.gold += 5 + sv.zone * 3; gain_xp(2 + sv.zone); shown = 253; }   /* (a level-up message covers the window: put it back) */
                    break;
                }
            }
        }
        /* pirates */
        if(!--spawn_t){ spawn(); spawn_t = b_fight ? 70 + (rnd() & 63) : 120 + (rnd() & 127); }
        /* the Technobeast */
        if(b_on){ INT16 dx, dy;
            b_t++; if(!(b_t & 7)) b_ang = (b_ang + 1) & 31;
            b_x = BEAST_X + circ[b_ang]; b_y = BEAST_Y + circ[(b_ang + 8) & 31];
            dx = wrapd(shx >> 4, b_x); dy = wrapd(shy >> 4, b_y);
            if(!b_fight && dx > -200 && dx < 200 && dy > -180 && dy < 180){
                b_fight = 1; music(MUS_BOSS);
                if(!(sv.story & ST_BEAST)){ sv.story |= ST_BEAST; dlg_say("BOLT", "THERE IT IS: THE TECHNOBEAST! LASERS BOUNCE OFF ITS ARMOUR. SHOOT ITS CORE WHEN IT GLOWS PINK!"); shown = 253; }
            } else if(b_fight && (dx > 320 || dx < -320 || dy > 320 || dy < -320)){ b_fight = 0; music(MUS_SPACE); }
            if(b_flash) b_flash--;
            if(b_dead){
                if(!(b_dead & 15)){ Pirate *p = &pir[b_dead >> 4 & 1 ? 0 : 1]; p->st = 2; p->x = b_x - 16 + (rnd() & 31); p->y = b_y - 16 + (rnd() & 31); sfx(SFX_BOOM); }
                if(!--b_dead){
                    UINT8 n; for(n = 0; n < 40; n++) move_sprite(n, 0, 0);
                    sv.story |= ST_DONE; sv.gold += 5000; b_on = 0;
                    dlg_say("BOLT", "NOW! I SAY MY NAME... I AM DAWN!");
                    dlg_say(0, "THE BEAST'S HEART OPENS WIDE. EVERY SHIP IT EVER ATE FLIES OUT, BACK TO THE STARS.");
                    gain_xp(200);
                    k = SP_END; goto out;
                }
            } else if(b_fight){
                if(!(b_t & 63)) orb_fire(dx, dy);
                if(dx > -20 && dx < 20 && dy > -20 && dy < 20) hurt(4 + sv.zone / 2, dx, dy, 32);
            }
        }
        for(i = 0; i < 2; i++){ Orb *o = &orb[i]; INT16 dx, dy;
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
            if((frame & 3) || sv.zone >= 2){                              /* 3/4 of your top speed (all of it, later on) */
                if(dx > 1) p->x++; else if(dx < -1) p->x--;
                if(dy > 1) p->y++; else if(dy < -1) p->y--;
                p->x &= 4095; p->y &= 4095;
            }
            if(!inv && dx > -12 && dx < 12 && dy > -12 && dy < 12){ hurt(2 + sv.zone, dx, dy, 24); boom(p); }   /* rammed */
        }
        if(inv) inv--;
        if(cool) cool--;
        /* the screen */
        wait_vbl_done();
        SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y;
        planet_pals();
        if(!(frame & 31)) set_bkg_palette_entry(3, 3, space_twinkle[(frame >> 5) & 1]);
        if(inv & 4) hide2(OAM_SHIP); else spr16(OAM_SHIP, SP_T_SHIP(dir), SHIP_SX, SHIP_SY);
        for(i = 0; i < NLAS; i++){
            if(las[i].life) spr16(OAM_LAS + i * 2, SP_T_LASER, wrapd(las[i].x, cam_x), wrapd(las[i].y, cam_y)); else hide2(OAM_LAS + i * 2);
        }
        for(i = 0; i < NPIR; i++){ Pirate *p = &pir[i];
            if(!p->st){ hide2(OAM_PIR + i * 2); continue; }
            set_sprite_prop(OAM_PIR + i * 2, (p->st > 1 ? 3 : 1) | 0x08); set_sprite_prop(OAM_PIR + i * 2 + 1, (p->st > 1 ? 3 : 1) | 0x08);
            spr16(OAM_PIR + i * 2, p->st > 1 ? SP_T_BOOM(p->st > 10) : SP_T_PIRATE((frame >> 3) & 1), wrapd(p->x, cam_x), wrapd(p->y, cam_y));
        }
        beast_show(frame);
        for(i = 0; i < 2; i++){
            INT16 x = wrapd(orb[i].x, cam_x), y = wrapd(orb[i].y, cam_y);
            if(!orb[i].life || x < -4 || x > 164 || y < -4 || y > 132) move_sprite(OAM_ORB + i, 0, 0); else move_sprite(OAM_ORB + i, (UINT8)(x + 4), (UINT8)(y + 12));
        }
        arrow();
        label(over_star());
        if(shown == 255) hud_update();
        if(!sv.hp){
            UINT8 n; char b[96], *d;
            for(n = 0; n < 40; n++) move_sprite(n, 0, 0);
            sv.gold -= sv.gold / 4; sv.hp = hud_hpmax(sv.level) / 2; if(!sv.hp) sv.hp = 1;
            d = scat(b, b_fight ? "OUCH! THE BEAST WRECKED THE SHIP." : "OUCH! THE PIRATES WRECKED THE SHIP."); d = scat(d, " I LIMPED BACK TO "); d = world_name(d, world_seed(sv.star)); scat(d, ". SOME GOLD FELL OUT.");
            dlg_say("BOLT", b);
            break;
        }
        if(HIT(J_A)){
            UINT8 o = over_star();
            if(o != 255){
                char b[64], *d;
                sv.star = o;
                d = scat(b, "LANDING ON "); d = world_name(d, world_seed(o)); d = scat(d, ", A "); d = scat(d, kind_name[star_k[o]]); scat(d, " WORLD.");
                hide2(OAM_ARR); sfx(SFX_STAIRS);
                dlg_say("BOLT", b);
                if(opt.autosave) slot_write(cur_slot);
                break;
            }
            if(!cool){ fire(); cool = 8; }
        }
        if(HIT(J_START)){
            if(sysmenu_run(redraw) == SYS_TITLE){ k = SP_TITLE; goto out; }
            shown = 253;
        }
    }
    k = SP_LAND;
out:
    for(i = 0; i < 40; i++) move_sprite(i, 0, 0);
    hud_hide(); SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); txt_cls();
    return k;
}
