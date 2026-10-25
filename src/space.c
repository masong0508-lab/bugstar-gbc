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
#include "wram.h"
#include "fx.h"
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
#include "warp.h"
#include "space_int.h"

#define VIEW_W    11
#define VIEW_H    9
#define VMAX      32             /* top speed, 1/16 px a frame: 2 px */
#define VBOOST    56             /* with B held: 3.5 px */

UINT16 shx, shy;                 /* the ship, 1/16 px (so 65536 = 4096 px: it wraps by itself) */
INT16  vx, vy;
static UINT8  dir;               /* 0 = up, clockwise */
UINT16 cam_x, cam_y;      /* top-left of the screen, world px */
static UINT8  left_mt, top_mt;
static UINT8  pk[4];             /* the kind each planet palette (4..7) holds now */
static UINT8  o1, o2;            /* the starfield hash's offsets, from the seed */
UINT16 rng;
static UINT8  told;              /* the how-to-fly talk, once per power-on */
UINT8  inv;               /* frames the ship can not be hurt (after a hit) */

static Laser  las[NLAS];
UINT8  b_on, b_dead, b_fight;    /* the Technobeast: here at all, dying (frames left), fighting (space_foe.c runs it) */
UINT16 b_x, b_y;
static const INT8 dir_x[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const INT8 dir_y[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const INT8 thr_x[8] = { 0, 2, 3, 2, 0, -2, -3, -2 };   /* thrust: diagonals a bit less on each axis */
static const INT8 thr_y[8] = { -3, -2, 0, 2, 3, 2, 0, -2 };
static const palette_color_t space_twinkle[2] = { RGB(31, 31, 31), RGB(20, 24, 31) };

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
static void gfx_tiles(void);
static void redraw(void){ if(sys_gfx_lost) gfx_tiles(); SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; txt_pal_reset(); pals(); draw_all(); SPRITES_8x16; }

static void sprite_pals(UINT8 oam, UINT8 n, UINT8 pal){ while(n--){ set_sprite_prop(oam, pal | 0x08); oam++; } }

/* ------------------------------------------------------------ the HUD line for a planet you are over */
UINT8 shown;                                                   /* the star whose label is up, 255 for the HUD, 253 = redraw whichever */
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
static INT16 drag(INT16 v){
    if(v > 0){ v -= 1 + (v >> 4); if(v < 0) v = 0; }
    else if(v < 0){ v += 1 + ((-v) >> 4); if(v > 0) v = 0; }
    return v;
}
static UINT8  aim_ok, aim_beast;  /* where the arrow points (and SELECT warps to): a world, or the beast */
static UINT16 aim_x, aim_y;
static void arrow(void){                                         /* point at the nearest other world (one not freed yet, if there is one near), if it is off the screen */
    UINT16 px = shx >> 4, py = shy >> 4; UINT8 cx = (UINT8)(px >> 8), cy = (UINT8)(py >> 8), i, j, best = 255, d;
    INT16 bdx = 0, bdy = 0; UINT16 bd = 0xFFFF;
    aim_beast = 0;
    if(b_on && !b_dead){ bdx = wrapd(b_x, px); bdy = wrapd(b_y, py); best = 0; aim_beast = 1; goto aim; }
    for(j = 0; j < 3; j++) for(i = 0; i < 3; i++){
        UINT8 s = (UINT8)((((cy + j - 1) & 15) << 4) | ((cx + i - 1) & 15));
        INT16 dx = wrapd(star_px(s), px), dy = wrapd(star_py(s), py);
        UINT16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, m = ax > ay ? ax + ay / 2 : ay + ax / 2;
        if(world_freed(s)) m += 300;                                /* freed worlds only if nothing else is near */
        if(m > 40 && m < bd){ bd = m; best = s; bdx = dx; bdy = dy; }
    }
aim:
    aim_ok = best != 255; aim_x = (px + bdx) & 4095; aim_y = (py + bdy) & 4095;
    if(!opt.arrow || best == 255 || (bdx > -88 && bdx < 88 && bdy > -72 && bdy < 72)){ hide2(OAM_ARR); return; }
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
typedef struct { UINT16 x, y; UINT8 life; } Puff;
static Puff   puff[2];

static void gfx_tiles(void){                                     /* the pictures and palettes (after the creator reused the tile memory) */
    VBK_REG = VBK_BANK_1;
    space_tiles_load(0, SPR0);
    VBK_REG = VBK_BANK_0;
    set_sprite_palette(0, 6, sp_spr_pal);
    sprite_pals(OAM_SHIP, 2, 0); sprite_pals(OAM_LAS, NLAS * 2, 2); sprite_pals(OAM_PIR, NPIR * 2, 1); sprite_pals(OAM_ARR, 2, 2); sprite_pals(OAM_ORB, 2, 3);
    foes_gfx();
}
static void gfx(void){                                           /* everything onto the screen (LCD off): it fades in */
    UINT8 i;
    gfx_tiles();
    SPRITES_8x16;
    txt_pal_reset(); pals();
    camera(); SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; draw_all();
    hud_init(); shown = 255;
    music(MUS_SPACE);
    SHOW_BKG; SHOW_SPRITES;
    spr16(OAM_SHIP, SP_T_SHIP(dir), SHIP_SX, SHIP_SY);
    fade_in();
}

int space_run(void) BANKED {
    UINT8 k, i, frame = 0, cool = 0;
    UINT32 ws = sv.seed;
    fade_out(); DISPLAY_OFF;
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
    inv = 0;
    puff[0].life = puff[1].life = 0;
    foes_reset();
    gfx();
    if(!told){ told = 1; space_say(SAY_HOWTO); }
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
        if((k & J_B) && !b_on && !(frame & 7)){ Puff *f = &puff[(frame >> 3) & 1];      /* engine puffs while boosting */
            f->x = ((shx >> 4) - dir_x[dir] * 11) & 4095; f->y = ((shy >> 4) - dir_y[dir] * 11) & 4095; f->life = 14; }
        camera(); scroll_fill();
        /* lasers */
        for(i = 0; i < NLAS; i++){ Laser *l = &las[i];
            if(!l->life) continue;
            l->life--; l->x = (l->x + l->dx) & 4095; l->y = (l->y + l->dy) & 4095;
            foes_hit(l);
        }
        if(foes_step(frame)){ k = SP_END; goto out; }
        if(inv) inv--;
        if(cool) cool--;
        /* the screen */
        wait_vbl_done();
        shake_tick(); SCX_REG = (UINT8)cam_x + shake_x; SCY_REG = (UINT8)cam_y + shake_y;
        planet_pals();
        if(!(frame & 31)) set_bkg_palette_entry(3, 3, space_twinkle[(frame >> 5) & 1]);
        if(inv & 4) hide2(OAM_SHIP); else spr16(OAM_SHIP, SP_T_SHIP(dir), SHIP_SX, SHIP_SY);
        for(i = 0; i < NLAS; i++){
            if(las[i].life) spr16(OAM_LAS + i * 2, SP_T_LASER, wrapd(las[i].x, cam_x), wrapd(las[i].y, cam_y)); else hide2(OAM_LAS + i * 2);
        }
        foes_draw(frame);
        if(!b_on) for(i = 0; i < 2; i++){                            /* engine puffs (sprites 0..3: the beast's when it is about) */
            if(puff[i].life){ puff[i].life--; set_sprite_prop(i * 2, 3 | 0x08); set_sprite_prop(i * 2 + 1, 3 | 0x08);
                spr16(i * 2, SP_T_BOOM(puff[i].life > 9), wrapd(puff[i].x, cam_x), wrapd(puff[i].y, cam_y)); }
            else hide2(i * 2);
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
        if(HIT(J_SELECT) && aim_ok && !b_dead){                      /* hyperspace! */
            WarpResult wr;
            fade_out(); DISPLAY_OFF; hud_hide();
            warp_run(aim_beast ? 2100 : 1500, &wr);
            fade_out(); DISPLAY_OFF;
            shx = aim_x << 4; shy = ((aim_y + (aim_beast ? 150 : 50)) & 4095) << 4; vx = 0; vy = -16; dir = 0;
            for(i = 0; i < NLAS; i++) las[i].life = 0;
            puff[0].life = puff[1].life = 0; inv = 0; foes_clear();
            gfx();
            warp_report(&wr);
            shown = 253;
            continue;
        }
        if(HIT(J_START)){
            if(sysmenu_run(redraw) == SYS_TITLE){ k = SP_TITLE; goto out; }
            shown = 253;
        }
    }
    k = SP_LAND;
out:
    fade_out(); DISPLAY_OFF;
    for(i = 0; i < 40; i++) move_sprite(i, 0, 0);
    hud_hide(); SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); txt_cls();
    DISPLAY_ON;
    return k;
}

void space_pals_copy(UINT8 dst, UINT8 src, UINT8 n) BANKED { set_sprite_palette(dst, n, &sp_spr_pal[src * 4]); }
void title_pal_load(UINT8 pal) BANKED { set_sprite_palette(pal, 1, sp_spr_pal); }   /* for the title: the ship's colours */
