#pragma bank 4
/* BUGSTAR GBC - step 6: the planet, top-down.
   A world is 256x256 metatiles of 16x16 px (4096 px each way, wrapping), grown from the save's seed. Nothing of it is stored:
   every metatile is worked out from smooth value noise when it scrolls into view (height: water, sand, grass, rocks up high;
   a second noise for where trees and flowers grow). The kind of world (GARDEN, DESERT, ICE, JUNGLE, OCEAN, DUSK) comes from the
   seed too, and only changes the colours and how much water and forest there is.
   The hardware BG map is 32x32 tiles (16x16 metatiles); as the camera moves, the row or column that comes into view is drawn.
   The ship you landed in is a sprite: A next to it saves and heals, and can fly you back into space (step 8).
   Berry bushes heal a little. The door leads down (step 7). Each star has its own world: see world_seed() in save.h. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "fx.h"
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"
#include "sysmenu.h"
#include "wram.h"
#include "menus.h"
#include "bug.h"
#include "bugspr.h"
#include "planet_art.h"
#include "planet.h"
#include "dungeon.h"
#include "shop.h"
#include "econ.h"
#include "town.h"
#include "feat.h"
#include "wild.h"

#define SHIP_TILE0 104           /* sprite tiles in VRAM bank 1 (the terrain uses bank-1 tiles 0..PL_NTILES-1 = 0..85) */
#define CRIT_TILE0 120           /* the sparkle and butterfly sprites (bank 1, after the ship) */
#define CRIT_OAM0  28            /* sprites 28..37: 6 sparkles, 3 butterflies, the golden moth */
#define SHIP_OAM0  20
#define SHIP_PAL   0
#define VIEW_W     11            /* metatiles drawn across (160 px + one for scrolling) */
#define VIEW_H     9             /* and down (the HUD covers the bottom 16 px) */
#define BUG_SX     72            /* where the bug's feet stand on the screen */
#define BUG_SY     68

typedef struct { UINT8 water, shore, sand, rock, trees, flowers, berries; } WorldKind;
static const WorldKind kinds[PL_NWORLD] = {
    /* water  shore sand  rock  trees flowers berries   (heights 0..255; densities out of 256) */
    {  78,    88,   100,  205,  64,   28,     12 },   /* GARDEN */
    {  44,    52,   150,  190,  12,   10,     4  },   /* DESERT: dunes, rocks, a few cacti */
    {  70,    80,   96,   200,  50,   20,     6  },   /* ICE */
    {  66,    74,   84,   215,  150,  30,     14 },   /* JUNGLE */
    {  128,   140,  156,  225,  70,   26,     10 },   /* OCEAN: islands */
    {  80,    90,   104,  195,  80,   16,     8  },   /* DUSK */
};

static UINT16 seed_a, seed_b;                 /* the world's seed, in two halves */
static UINT8  wk;                              /* the kind of world */
static UINT8  wv;                              /* its weird trait (wild.h), 0 = none */
static UINT8  ship_mx, ship_my, door_mx, door_my;
static UINT16 bx, by;                          /* the bug's feet, world pixels (wrap at 4096) */
static UINT16 cam_x, cam_y;                    /* the top-left of the screen, world pixels */
static UINT8  left_mt, top_mt;                 /* the metatile at the top-left of what is drawn */
static UINT8  picked[32][2]; static UINT8 npicked;   /* berry bushes eaten (the last 32) */

/* ------------------------------------------------------------ the ground, worked out from the seed */
static UINT8 sa1, sa2, sb1, sb2;                /* the seed, as offsets into the shuffle table */
#define HASH(x, y, o1, o2) planet_perm[(UINT8)(planet_perm[(UINT8)((x) + (o1))] + (y) + (o2))]
static UINT8 lerp8(UINT8 a, UINT8 b, UINT8 t){ return (UINT8)(a + (INT16)((((INT16)b - a) * (INT16)(t >> 1)) >> 7)); }   /* (t halved: the product must fit in 16 bits) */
static UINT8 vnoise(UINT8 x, UINT8 y, UINT8 sh, UINT8 o1, UINT8 o2){   /* value noise, lattice every 2^sh metatiles, smooth between */
    UINT8 m = (1 << sh) - 1, x0 = x & ~m, y0 = y & ~m, x1 = x0 + m + 1, y1 = y0 + m + 1;
    UINT8 fx = planet_ease[(UINT8)((x & m) << (8 - sh))], fy = planet_ease[(UINT8)((y & m) << (8 - sh))];
    return lerp8(lerp8(HASH(x0, y0, o1, o2), HASH(x1, y0, o1, o2), fx), lerp8(HASH(x0, y1, o1, o2), HASH(x1, y1, o1, o2), fx), fy);
}
static UINT8 height(UINT8 x, UINT8 y){
    UINT8 a = vnoise(x, y, 4, sa1, sa2), b = vnoise(x, y, 2, sa2, sa1);
    return (UINT8)(((UINT16)a + a + b) / 3);                   /* big hills, and a little roughness */
}
static UINT8 is_picked(UINT8 x, UINT8 y){ UINT8 i; for(i = 0; i < npicked; i++) if(picked[i][0] == x && picked[i][1] == y) return 1; return 0; }
/* The cities: 3 a world, each a square footprint (3, 5, 7, 9 or 13 metatiles wide by tier) laid over the land. Roads of paving cross it, houses and
   stalls fill the blocks, towns of tier 2+ have a wall with a gate at the bottom middle. Stepping on the gate walks into town.c. */
static const UINT8 tw_w[5] = { 3, 5, 7, 9, 13 };
static UINT8 tw_x[3], tw_y[3], tw_t[3], tw_n, tw_seen;                 /* top-left corner, tier, how many are placed */
static UINT8 town_cell(UINT8 t, UINT8 dx, UINT8 dy, UINT8 x, UINT8 y){
    UINT8 w = tw_w[t], c = w >> 1, h = HASH(x, y, sb1, sb2);
    if(dy == w - 1 && dx == c) return MT_GATE;
    if(t >= 2 && (dx == 0 || dy == 0 || dx == w - 1 || dy == w - 1)) return MT_WALL;
    if(dx == c || (t && dy == c) || (t >= 3 && (dx == (c >> 1) || dx == c + (c >> 1) || dy == (c >> 1) || dy == c + (c >> 1)))) return MT_SAND;
    if(dy >= w - (t ? 2 : 1)) return (h & 15) ? MT_GRASS : MT_FLOWERS;     /* the lawn in front */
    if((h & 3) == 0) return (h & 16) ? MT_FLOWERS : MT_GRASS;              /* a gap between the houses */
    return ((h & 0x60) == 0x60) ? MT_STALL : MT_HOUSE;
}
static UINT8 ground(UINT8 x, UINT8 y){                          /* the metatile at (x, y) */
    const WorldKind *k = &kinds[wk]; UINT8 h, v, r;
    UINT8 dx = x - ship_mx + 1, dy = y - ship_my + 1;
    if(dx < 4 && dy < 4) return MT_PAD;                           /* the landing pad around the ship (you step out onto its bottom row) */
    if(x == door_mx && y == door_my) return MT_DOOR;
    { UINT8 i; for(i = 0; i < tw_n; i++){ UINT8 w = tw_w[tw_t[i]], ex = x - tw_x[i], ey = y - tw_y[i]; if(ex < w && ey < w) return town_cell(tw_t[i], ex, ey, x, y); } }
    h = height(x, y);
    if(h < k->water) return MT_DEEP;
    if(h < k->shore) return MT_SHALLOW;
    r = HASH(x, y, sb1, sb2);
    if(h < k->sand) return MT_SAND;
    if(h >= k->rock) return (r & 3) ? MT_ROCK : MT_SAND;          /* high ground: boulders */
    v = vnoise(x, y, 3, sb1, sb2);                                 /* the second noise: where things grow */
    if(v > 255 - k->trees && (r & 7)) return MT_TREE;
    if(r < k->berries && !is_picked(x, y)) return MT_BERRY;
    if(r > 255 - k->flowers) return MT_FLOWERS;
    return (r & 16) ? MT_GRASS2 : MT_GRASS;
}
static UINT8 solid(UINT8 mt){ return mt == MT_DEEP || mt == MT_TREE || mt == MT_ROCK || mt == MT_HOUSE || mt == MT_STALL || mt == MT_WALL; }

/* ------------------------------------------------------------ drawing */
/* The ground is blended (planet_art.h): each quarter of a metatile looks at its cell and the three cells towards that corner.
   If they are all the same kind of ground the quarter is a plain texture; otherwise it is one of the blend tiles for the two
   kinds that meet, flipped to face the right corner. The kinds of ground of the cells on screen (and one ring around them) are
   kept in a small ring buffer so that each cell is worked out once. */
#define MTC WRAM_AT(WRAM_STARS, UINT8)                         /* 16 x 16 ring: the metatile at (x & 15, y & 15) (wram.h; space makes its own again) */
static UINT8 cell(UINT8 x, UINT8 y){ return MTC[((y & 15) << 4) | (x & 15)]; }
static void cell_fill(UINT8 x, UINT8 y){ MTC[((y & 15) << 4) | (x & 15)] = ground(x, y); }
static void cache_col(UINT8 x){ UINT8 j; for(j = 0; j < VIEW_H + 3; j++) cell_fill(x, top_mt - 1 + j); }
static void cache_row(UINT8 y){ UINT8 i; for(i = 0; i < VIEW_W + 2; i++) cell_fill(left_mt - 1 + i, y); }
static void draw_mt(UINT8 x, UINT8 y){                          /* metatile (x, y) into the BG map, where it falls */
    UINT8 mt = cell(x, y), t[4], a[4], bx2 = (x & 15) << 1, by2 = (y & 15) << 1, q, lo, hi, l0, l1, l2, l3, pat, sx, sy;
    if(planet_mtlvl[mt] >= 4){                                   /* a tree, a rock, a bush, the door, the pad: drawn as they are */
        t[0] = mt * 4; t[1] = mt * 4 + 1; t[2] = mt * 4 + 2; t[3] = mt * 4 + 3;
        a[0] = a[1] = a[2] = a[3] = 0x08 | (4 + planet_mtpal[mt]);
    } else {
        l0 = planet_mtlvl[mt] & 3;
        for(q = 0; q < 4; q++){
            sx = (q & 1) ? x + 1 : x - 1; sy = (q & 2) ? y + 1 : y - 1;
            l1 = planet_mtlvl[cell(sx, y)] & 3; l2 = planet_mtlvl[cell(x, sy)] & 3; l3 = planet_mtlvl[cell(sx, sy)] & 3;
            lo = l0; if(l1 < lo) lo = l1; if(l2 < lo) lo = l2; if(l3 < lo) lo = l3;
            if(l0 == l1 && l0 == l2 && l0 == l3){ t[q] = mt * 4 + q; a[q] = 0x08 | (4 + planet_mtpal[mt]); }
            else {
                pat = (l0 > lo) | ((l1 > lo) << 1) | ((l2 > lo) << 2) | ((l3 > lo) << 3);
                t[q] = PL_NMT * 4 + lo * 14 + pat - 1;
                a[q] = 0x08 | (4 + lo) | ((q & 1) ? 0x20 : 0) | ((q & 2) ? 0x40 : 0);   /* bank 1 tiles, the pair's palette, flipped towards the corner */
            }
        }
    }
    VBK_REG = VBK_BANK_1; set_bkg_tiles(bx2, by2, 2, 2, a);
    VBK_REG = VBK_BANK_0; set_bkg_tiles(bx2, by2, 2, 2, t);
}
static void draw_all(void){
    UINT8 i, j;
    left_mt = (UINT8)(cam_x >> 4); top_mt = (UINT8)(cam_y >> 4);
    for(j = 0; j < VIEW_H + 3; j++) for(i = 0; i < VIEW_W + 2; i++) cell_fill(left_mt - 1 + i, top_mt - 1 + j);
    for(j = 0; j < VIEW_H + 1; j++) for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt + j);
}
static void scroll_fill(void){                                   /* draw the column or row that has just come into view */
    UINT8 l = (UINT8)(cam_x >> 4), t = (UINT8)(cam_y >> 4), i;
    while(l != left_mt){
        if((UINT8)(l - left_mt) < 128){ left_mt++; cache_col(left_mt + VIEW_W); for(i = 0; i < VIEW_H + 1; i++) draw_mt(left_mt + VIEW_W - 1, top_mt + i); }
        else { left_mt--; cache_col(left_mt - 1); for(i = 0; i < VIEW_H + 1; i++) draw_mt(left_mt, top_mt + i); }
    }
    while(t != top_mt){
        if((UINT8)(t - top_mt) < 128){ top_mt++; cache_row(top_mt + VIEW_H + 1); for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt + VIEW_H); }
        else { top_mt--; cache_row(top_mt - 1); for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt); }
    }
}
static void ship_show(void){                                   /* the ship sprite, if it is near the screen */
    INT16 sx = (INT16)((((UINT16)ship_mx << 4) - 8 - cam_x) & 4095), sy = (INT16)((((UINT16)ship_my << 4) - 8 - cam_y) & 4095);
    UINT8 i;
    if(sx > 2048) sx -= 4096; if(sy > 2048) sy -= 4096;
    for(i = 0; i < 8; i++){
        INT16 x = sx + (i >> 1) * 8, y = sy + (i & 1) * 16;
        if(x < -8 || x > 168 || y < -16 || y > 128) move_sprite(SHIP_OAM0 + i, 0, 0);
        else move_sprite(SHIP_OAM0 + i, (UINT8)(x + 8), (UINT8)(y + 16));
    }
}

/* ------------------------------------------------------------ setting up */
static UINT8 open_around(UINT8 x, UINT8 y, UINT8 r){           /* no water, trees or rocks within r metatiles (and land at the middle) */
    INT8 dx, dy;
    for(dy = -(INT8)r; dy <= (INT8)r; dy++) for(dx = -(INT8)r; dx <= (INT8)r; dx++){ UINT8 g = ground(x + dx, y + dy); if(solid(g) || g == MT_SHALLOW) return 0; }
    return 1; }
static void find_land(UINT8 *ox, UINT8 *oy, UINT8 cx, UINT8 cy, UINT8 r){   /* the nearest open ground to (cx, cy), searching outwards ring by ring */
    UINT8 d, i; INT8 dx, dy;
    for(d = 0; d < 100; d++){
        for(i = 0; i < 8 * (UINT16)d + (d == 0); i++){
            UINT8 side = d ? i / (2 * d) : 0, k = d ? i % (2 * d) : 0;
            if(side == 0){ dx = -(INT8)d + k; dy = -(INT8)d; } else if(side == 1){ dx = d; dy = -(INT8)d + k; } else if(side == 2){ dx = d - k; dy = d; } else { dx = -(INT8)d; dy = d - k; }
            if(((dx | dy) & 1) == 0 && open_around(cx + dx, cy + dy, r)){ *ox = cx + dx; *oy = cy + dy; return; }   /* (every other spot: twice as quick, still close) */
        }
    }
    *ox = cx; *oy = cy;
}
static void world_setup(void){
    UINT32 ws = world_seed(sv.star);
    seed_a = (UINT16)ws; seed_b = (UINT16)(ws >> 16) ^ 0x1234u; if(!seed_b) seed_b = 1;
    sa1 = (UINT8)seed_a; sa2 = (UINT8)(seed_a >> 8); sb1 = (UINT8)seed_b; sb2 = (UINT8)(seed_b >> 8) ^ 0x5A;
    wk = world_kind(ws); wv = weird_of(sv.star);
    ship_mx = 250; ship_my = 250;                                /* (out of the way while we look for land) */
    door_mx = 0; door_my = 0;
    find_land(&ship_mx, &ship_my, 128, 128, 2);                  /* a clearing big enough to land in */
    { UINT8 d = (UINT8)(seed_b & 3), dx = d == 0 ? 14 : d == 1 ? (UINT8)-14 : 3, dy = d == 2 ? 14 : d == 3 ? (UINT8)-14 : 5;
      find_land(&door_mx, &door_my, ship_mx + dx, ship_my + dy, 1); }
    tw_n = 0; tw_seen = 0;
    { UINT8 i;
      static const INT8 ox[3] = { 22, 31, 17 }, oy[3] = { 9, 13, 38 };
      for(i = 0; i < 3; i++){                                      /* the cities: nearer ones are smaller, the farthest is the biggest */
          UINT8 t = i + ((seed_a >> (i * 4)) & 7) % 3, lx, ly;
          INT8 dx = (seed_b >> i) & 1 ? -ox[i] : ox[i], dy = (seed_b >> (i + 3)) & 1 ? -oy[i] : oy[i];
          find_land(&lx, &ly, ship_mx + dx, ship_my + dy, 1);
          tw_x[i] = lx - (tw_w[t] >> 1); tw_y[i] = ly - (tw_w[t] >> 1); tw_t[i] = t; tw_n = i + 1;
      } }
    bx = ((UINT16)ship_mx << 4) + 8; by = ((UINT16)ship_my << 4) + 40;   /* you step out just below the ship */
    npicked = 0;
#ifdef TEST_TOWN
    sv.gold = 500; sv.friends = 3; ex.fish = 5; ex.rare = 3;
    bx = ((UINT16)(tw_x[TEST_TOWN] + (tw_w[tw_t[TEST_TOWN]] >> 1)) << 4) + 8; by = ((UINT16)(tw_y[TEST_TOWN] + tw_w[tw_t[TEST_TOWN]]) << 4) + 20;
#endif
#ifdef TEST_DOOR
    bx = ((UINT16)door_mx << 4) + 8; by = ((UINT16)door_my << 4) - 6;   /* (test builds: start just above the door) */
#endif
}
static const palette_color_t glow_pal[4] = { RGB(0, 0, 0), RGB(8, 12, 2), RGB(31, 29, 6), RGB(31, 31, 20) };   /* the butterflies glow at night */
static void apply_pal(void){                                    /* the colours, darkened by the time of day */
    palette_color_t b[16], sp[4];
    memcpy(b, &planet_pals[wk * 16], 32); tod_tint(b, 16); if(wv) weird_pal(b, 16, wv, (UINT8)frames32()); set_bkg_palette(4, 4, b);
    memcpy(sp, ship_pal, 8); tod_tint(sp, 4); set_sprite_palette(SHIP_PAL, 1, sp);
    if(tod_dark() >= 9) set_sprite_palette(2, 1, glow_pal); else set_sprite_palette(2, 1, &crit_pal[4]);
}
static void load_gfx(void){
    UINT8 i;
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, PL_NTILES, planet_tiles);
    set_sprite_data(SHIP_TILE0, 16, ship_tiles);
    set_sprite_data(CRIT_TILE0, 8, crit_tiles);
    VBK_REG = VBK_BANK_0;
    set_sprite_palette(1, 2, crit_pal);
    life_gfx(wk);
    apply_pal();
    pet_load();
    for(i = 0; i < 8; i++){ set_sprite_tile(SHIP_OAM0 + i, SHIP_TILE0 + i * 2); set_sprite_prop(SHIP_OAM0 + i, SHIP_PAL | 0x08); }
}
static void camera(void){ cam_x = (bx - BUG_SX) & 4095; cam_y = (by - BUG_SY) & 4095; }
static void redraw(void){ if(sys_gfx_lost) load_gfx(); SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; txt_pal_reset(); apply_pal(); draw_all(); }

/* ------------------------------------------------------------ walking */
static UINT8 at(UINT16 x, UINT16 y){ return ground((UINT8)((x & 4095) >> 4), (UINT8)((y & 4095) >> 4)); }
static UINT8 blocked(UINT16 x, UINT16 y){                       /* the bug's feet: a box 10 wide and 4 tall */
    return solid(at(x - 5, y - 3)) || solid(at(x + 4, y - 3)) || solid(at(x - 5, y)) || solid(at(x + 4, y));
}
static void eat_berry(UINT8 x, UINT8 y){
    if(npicked < 32){ picked[npicked][0] = x; picked[npicked][1] = y; npicked++; }
    else { memmove(picked[0], picked[1], 31 * 2); picked[31][0] = x; picked[31][1] = y; }
    cell_fill(x, y); draw_mt(x, y); sfx(SFX_PICK);
    { UINT16 mx = hud_hpmax(sv.level); sv.hp = sv.hp + 3 > mx ? mx : sv.hp + 3; }
    sv.gold += 2; sv.xp += 1;
}
static UINT8 fish_hint;
static UINT8 water_near(void){                                   /* water within a short step of the bug's feet */
    UINT8 a = at(bx - 14, by - 2), b = at(bx + 14, by - 2), c = at(bx, by - 14), d = at(bx, by + 12);
    return a == MT_SHALLOW || a == MT_DEEP || b == MT_SHALLOW || b == MT_DEEP || c == MT_SHALLOW || c == MT_DEEP || d == MT_SHALLOW || d == MT_DEEP;
}
static UINT8 near_ship(void){
    UINT16 dx = (bx - ((UINT16)ship_mx << 4) - 8) & 4095, dy = (by - ((UINT16)ship_my << 4)) & 4095;
    if(dx > 2048) dx = 4096 - dx; if(dy > 2048) dy = 4096 - dy;
    return dx < 28 && dy < 40;
}


/* ------------------------------------------------------------ things to find: treasure sparkles, butterflies, a golden moth
   Every landing hides 6 treasures within 20 metatiles of the ship (different each time): they twinkle on the map. Walk over one for a
   prize (gold, a potion, XP, a heal, fuel); find all 6 for a bonus. Sometimes a golden moth flutters about: it runs from you, so
   hold B to chase it. Butterflies are just for the look. */
#define NTRE 6
#define NBUT 3
static UINT8  tre_x[NTRE], tre_y[NTRE], tre_got, tre_n, tre_found;
static UINT16 but_x[NBUT], but_y[NBUT]; static INT8 but_vx[NBUT], but_vy[NBUT];
static UINT16 moth_x, moth_y; static UINT8 moth_on, note_t;
static UINT16 prng;
static UINT8 pr(void){ prng ^= prng << 7; prng ^= prng >> 9; prng ^= prng << 8; return (UINT8)prng; }
static INT16 wd(UINT16 a, UINT16 b){ INT16 d = (INT16)((a - b) & 4095); return d > 2048 ? d - 4096 : d; }   /* a - b, the short way round */
static INT16 iabs(INT16 v){ return v < 0 ? -v : v; }
static void note(const char *t){                                 /* a line under the HUD for a moment: the HUD slides up 8 px to show it */
    txt_win(1); txt_line(0, 2, t, 20); txt_color(0, 2, 20, 1, TXT_GOLD); txt_win(0);
    hud_pos(HUD_Y - 8); note_t = 110;
}
static void note_off(void){ txt_win(1); txt_line(0, 2, "", 20); txt_win(0); hud_pos(HUD_Y); }
static void place_things(void){
    UINT8 i, tries;
    prng = (UINT16)(sv.secs ^ ((UINT16)DIV_REG << 8) ^ seed_a ^ 0xACE1u); if(!prng) prng = 1;
    tre_got = 0; tre_n = 0; tre_found = 0;
    for(i = 0; i < NTRE; i++){
        for(tries = 0; tries < 80; tries++){
            INT8 dx = (INT8)(pr() % 41) - 20, dy = (INT8)(pr() % 41) - 20; UINT8 x = ship_mx + dx, y = ship_my + dy, g;
            if(dx > -4 && dx < 4 && dy > -4 && dy < 4) continue;
            g = ground(x, y);
            if(g == MT_SAND || g == MT_GRASS || g == MT_GRASS2 || g == MT_FLOWERS){ tre_x[i] = x; tre_y[i] = y; tre_n++; break; }
        }
        if(tries == 80) tre_got |= 1 << i;                       /* (no room: that one is simply not there) */
    }
    for(i = 0; i < NBUT; i++){ but_x[i] = bx + (UINT8)pr() - 128; but_y[i] = by + (UINT8)pr() / 2 - 64; but_vx[i] = (INT8)(pr() % 3) - 1; but_vy[i] = (INT8)(pr() % 3) - 1; }
    moth_on = (pr() & 3) == 0 ? 0 : 1;                           /* three landings in four */
    moth_x = (((UINT16)ship_mx << 4) + 8 + (INT16)(INT8)pr() * 2) & 4095; moth_y = (((UINT16)ship_my << 4) - 80 - (pr() & 63)) & 4095;
}
static void prize(UINT8 i){
    char b[24], *d; UINT16 g; UINT8 t = pr() % 6, mx = 0;
    tre_got |= 1 << i; tre_found++; sfx(SFX_COIN);
    if(t == 1 && sv.potions >= 9) t = 0;
    if(t == 0 || t == 5){ g = t ? 40 + sv.level * 6 : 15 + sv.level * 3; sv.gold += g; d = scat(b, "+"); d = u16s(d, g); d = scat(d, " GOLD"); }
    else if(t == 1){ sv.potions++; d = scat(b, "A HEALTH POTION"); }
    else if(t == 2){ g = 10 + sv.level * 2; gain_xp(g); d = scat(b, "+"); d = u16s(d, g); d = scat(d, " XP"); }
    else if(t == 3){ sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level); d = scat(b, "FULLY HEALED"); }
    else { sv.el.fuel += 2; if(sv.el.fuel > TANK_CAP()) sv.el.fuel = TANK_CAP(); d = scat(b, "+2 SHIP FUEL"); }
    (void)mx;
    d = scat(d, "  "); d = u16s(d, tre_found); d = scat(d, "/"); u16s(d, tre_n);
    note(b);
    if(tre_found >= tre_n){
        sv.gold += 100 + sv.level * 10; if(sv.potions < 9) sv.potions++; sfx(SFX_FANFARE);
        dlg_say("BOLT", "ALL THE TREASURE IS FOUND! HERE IS A BONUS: GOLD AND A POTION.");
    }
}
static void crit_step(UINT8 frame){
    UINT8 i;
    for(i = 0; i < NTRE; i++) if(!(tre_got & (1 << i))){
        INT16 dx = wd(bx, ((UINT16)tre_x[i] << 4) + 8), dy = wd(by - 6, ((UINT16)tre_y[i] << 4) + 8);
        if(iabs(dx) < 10 && iabs(dy) < 10){ prize(i); break; }
    }
    for(i = 0; i < NBUT; i++){
        INT16 dx = wd(but_x[i], bx), dy = wd(but_y[i], by);
        if(!((frame + i * 11) & 31)){ but_vx[i] = (INT8)(pr() % 3) - 1; but_vy[i] = (INT8)(pr() % 3) - 1; }
        if(dx > 100) but_vx[i] = -1; else if(dx < -100) but_vx[i] = 1;
        if(dy > 70) but_vy[i] = -1; else if(dy < -70) but_vy[i] = 1;
        if(frame & 1){ but_x[i] = (but_x[i] + but_vx[i]) & 4095; but_y[i] = (but_y[i] + but_vy[i]) & 4095; }
    }
    if(moth_on){
        INT16 dx = wd(moth_x, bx), dy = wd(moth_y, by - 6), dist = iabs(dx) + iabs(dy);
        if(dist < 16){
            UINT16 g = 60 + sv.level * 8; char b[24], *d;
            moth_on = 0; sv.gold += g; gain_xp(20 + sv.level); sfx(SFX_FANFARE);
            d = scat(b, "GOLDEN MOTH! +"); d = u16s(d, g); scat(d, " GOLD"); note(b);
        } else if(dist < 70){                                     /* it flees: as fast as you walk, so run (B) to catch it */
            INT8 mx = dx > 0 ? 1 : -1, my = dy > 0 ? 1 : -1;
            if(!(frame & 7)){ if(pr() & 1) mx = -mx; else my = -my; }       /* a swerve now and then */
            moth_x = (moth_x + mx) & 4095; moth_y = (moth_y + my) & 4095;
        } else if(!(frame & 3)){ moth_x = (moth_x + (INT8)(pr() % 3) - 1) & 4095; moth_y = (moth_y + (INT8)(pr() % 3) - 1) & 4095; }
    }
    if(note_t && !--note_t) note_off();
}
static void crit_put(UINT8 oam, UINT16 wx, UINT16 wy, UINT8 tile, UINT8 pal){
    INT16 sx = (INT16)((wx - cam_x) & 4095), sy = (INT16)((wy - cam_y) & 4095);
    if(sx > 2048) sx -= 4096; if(sy > 2048) sy -= 4096;
    sx -= 4; sy -= 4;
    if(sx < -8 || sx > 168 || sy < -8 || sy > 120){ move_sprite(oam, 0, 0); return; }
    set_sprite_tile(oam, tile); set_sprite_prop(oam, pal | 0x08); move_sprite(oam, (UINT8)(sx + 8), (UINT8)(sy + 16));
}
static void crit_draw(UINT8 frame){
    UINT8 i, sp = CRIT_TILE0 + ((frame >> 4) & 1) * 2, bt = CRIT_TILE0 + 4 + ((frame >> 3) & 1) * 2;
    for(i = 0; i < NTRE; i++){
        if(tre_got & (1 << i)) move_sprite(CRIT_OAM0 + i, 0, 0);
        else crit_put(CRIT_OAM0 + i, ((UINT16)tre_x[i] << 4) + 8, ((UINT16)tre_y[i] << 4) + 8, sp, 1);
    }
    for(i = 0; i < NBUT; i++) crit_put(CRIT_OAM0 + NTRE + i, but_x[i], but_y[i], bt, 2);
    if(moth_on) crit_put(CRIT_OAM0 + NTRE + NBUT, moth_x, moth_y, bt, 1); else move_sprite(CRIT_OAM0 + NTRE + NBUT, 0, 0);
}

static void leave(void){                                       /* clear the screen for the next scene */
    UINT8 k;
    fade_out(); DISPLAY_OFF;
    hud_hide(); bugspr_hide(); for(k = 0; k < 40; k++) move_sprite(k, 0, 0); SCX_REG = 0; SCY_REG = 0; txt_cls(); txt_pal_reset();
    DISPLAY_ON;
}

static void enter(void){                                       /* the planet onto the screen (again, after a dungeon) */
    fade_out(); DISPLAY_OFF;
    load_gfx();
    txt_pal_reset();
    camera(); SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; draw_all();
    hud_init();
    bugspr_load(&sv.look);
    music(MUS_PLANET_K(wk));
    SHOW_BKG; SHOW_SPRITES;
    fade_in();
}
int planet_run(void) BANKED {                                    /* returns PL_TITLE when the person quits to the title */
    UINT8 frame = 0, step = 0, flip = 0, k, ox, oy, was_door = 0, tick = 0, was_gate = 0; char msg[140], *d;
    fade_out(); DISPLAY_OFF;
    cpu_fast();
    for(k = 0; k < 40; k++) move_sprite(k, 0, 0);               /* nothing left over from the menus */
    world_setup();
    place_things(); note_t = 0; fish_hint = 0;
    life_load(wk, ((UINT16)ship_mx << 4) + 8, ((UINT16)ship_my << 4) + 8);
    ex.seen[sv.star >> 3] |= 1 << (sv.star & 7);
    enter(); pet_reset(bx, by);
    play_reset(sv.secs);
    ship_show(); bugspr_show(BUG_SX - bugspr_w() / 2, BUG_SY - BUGSPR_UP, flip, 0);
    d = scat(msg, "WE LANDED ON "); d = world_name(d, world_seed(sv.star)); d = scat(d, ", A "); d = scat(d, planet_world_name[wk]);
    d = scat(d, world_freed(sv.star) ? " WORLD. IT IS FREE NOW, THANKS TO YOU." : " WORLD. FIND THE DOOR DOWN TO ITS QUEEN.");
    scat(d, " PRESS A AT THE SHIP TO SAVE OR TAKE OFF.");
    if(!(sv.story & ST_INTRO)){
        sv.story |= ST_INTRO;
        dlg_say("BOLT", "HI, I AM BOLT, YOUR SHIP. I LOST MY MEMORY. EVERY WORLD HAS A QUEEN DEEP DOWN, AND EACH ONE HOLDS ONE OF MY 8 DATA CORES. PLEASE GET THEM BACK!");
    }
    dlg_say("BOLT", msg);
    weird_land(wv);
#ifdef TEST_FISH
    fish_run(wk);
#endif
    for(;;){
        input_poll();
        k = keys;
        { INT8 vx = (k & J_LEFT) ? -1 : (k & J_RIGHT) ? 1 : 0, vy = (k & J_UP) ? -1 : (k & J_DOWN) ? 1 : 0; UINT8 sp = (k & J_B) ? 2 : 1, n;
          if(vx < 0) flip = 1; else if(vx > 0) flip = 0;
          for(n = 0; n < sp; n++){
              if(vx && !blocked(bx + vx, by)) bx = (bx + vx) & 4095;
              if(vy && !blocked(bx, by + vy)) by = (by + vy) & 4095;
          }
          if(vx || vy){ if(++step >= 6){ step = 0; frame ^= 1; } } }
        camera(); scroll_fill();
        ox = (UINT8)(bx >> 4); oy = (UINT8)((by - 2) >> 4);
        if(ground(ox, oy) == MT_BERRY) eat_berry(ox, oy);
        crit_step(++tick); pet_push(bx, by);
        if(!(tick & 1)){ UINT8 i; UINT16 lx, ly; life_step(bx, by, tick); for(i = 0; i < 4; i++) if(life_peek(i, &lx, &ly) && blocked(lx, ly)) life_back(i); }
        if(wv && !(tick & 63)) weird_tick(wv, tick);
        if(ground(ox, oy) == MT_GATE){
            if(!was_gate){ UINT8 i; was_gate = 1;
                for(i = 0; i < tw_n; i++) if((UINT8)(ox - tw_x[i]) < tw_w[tw_t[i]] && (UINT8)(oy - tw_y[i]) < tw_w[tw_t[i]]) break;
                if(i < tw_n){
                    INT16 dx = (INT16)door_mx - ox, dy = (INT16)door_my - oy;
                    if(dx > 127) dx -= 256; if(dx < -128) dx += 256; if(dy > 127) dy -= 256; if(dy < -128) dy += 256;
                    ex.city[sv.star >> 3] |= 1 << (sv.star & 7);
                    sfx(SFX_STAIRS);
                    k = town_run(sv.star, i, tw_t[i], (INT8)dx, (INT8)dy);
                    if(k){                                           /* the pit: a fight in one room, then back out of the gate */
                        int r = dungeon_pit(k - 1);
                        if((r & 15) == DG_TITLE){ leave(); return PL_TITLE; }
                        camera(); enter(); pet_reset(bx, by);
                        town_pit_reward((UINT8)r, k - 1);
                    }
                    if(opt.autosave) slot_write(cur_slot);
                }
            }
        } else was_gate = 0;
        if(!(tick & 7) && !note_t){                                  /* a banner the first time a city comes close */
            UINT8 i;
            for(i = 0; i < tw_n; i++) if(!(tw_seen & (1 << i))){
                INT8 ax = (INT8)(ox - tw_x[i] - (tw_w[tw_t[i]] >> 1)), ay = (INT8)(oy - tw_y[i] - (tw_w[tw_t[i]] >> 1));
                if(ax > -9 && ax < 9 && ay > -9 && ay < 9){ char lb[24]; lb[0] = 0; tw_seen |= 1 << i; town_label(lb, sv.star, i, tw_t[i]); note(lb); break; }
            }
        }
        if(ox == door_mx && oy == door_my){ if(!was_door){ int r;
                was_door = 1; sfx(SFX_STAIRS);
                r = dungeon_run();                                 /* down you go */
                if(r == DG_TITLE){ leave(); return PL_TITLE; }
                if(r == DG_FAINT){ bx = ((UINT16)ship_mx << 4) + 8; by = ((UINT16)ship_my << 4) + 40; was_door = 0; }
                else { bx = ((UINT16)door_mx << 4) + 8; by = ((UINT16)door_my << 4) + 30; was_door = 0; }   /* out of the door, one step below it */
                if(r == DG_WIN && opt.autosave) slot_write(cur_slot);
                camera(); enter(); continue; } }
        else was_door = 0;
        wait_vbl_done();
        SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y;
        bugspr_show(BUG_SX - bugspr_w() / 2, BUG_SY - BUGSPR_UP, flip, frame);
        ship_show(); crit_draw(tick); life_draw(cam_x, cam_y); { UINT8 po[3]; po[0] = 0; po[1] = 1; po[2] = 2; pet_draw(po, cam_x, cam_y, 1); }
        if(!(tick & (wv ? 7 : 63))) apply_pal();
        hud_update();
        if(HIT(J_A) && near_ship()){
            sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level); if(sv.el.fuel < 6) sv.el.fuel = 6; slot_write(cur_slot); sfx(SFX_FANFARE);
            dlg_say("BOLT", "HOME SWEET SHIP. YOU ARE HEALED, AND THE GAME IS SAVED.");
            if(ship_menu()){ leave(); return PL_LAUNCH; }
        }
        else if(HIT(J_A) && (k = life_pat(bx, by))){ note(k == 1 ? "A HAPPY PAT! +GOLD" : k == 2 ? "NUZZLE! +GOLD" : k == 3 ? "SHY! IT RAN AWAY." : "IT LIKES YOU ALREADY"); }
        else if(HIT(J_A) && water_near()){ fish_run(wk); }
        else if(!(tick & 31) && !fish_hint && water_near()){ fish_hint = 1; note("PRESS A AT THE WATER TO FISH"); }
        if(HIT(J_START)){
            { int sr = sysmenu_run(redraw);
              if(sr == SYS_TITLE){ leave(); return PL_TITLE; }
              if(sr == SYS_HELL || sr == SYS_MOON){                                  /* EXTRAS > HELL DEPTHS */
                  int r = dungeon_hell(sr == SYS_MOON);
                  if(r == DG_TITLE){ leave(); return PL_TITLE; }
                  if(r == DG_FAINT){ bx = ((UINT16)ship_mx << 4) + 8; by = ((UINT16)ship_my << 4) + 40; }
                  was_door = 1; camera(); enter(); continue; } }
            bugspr_load(&sv.look);
        }
    }
}
