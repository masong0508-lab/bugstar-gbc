#pragma bank 4
/* BUGSTAR GBC - step 6: the planet, top-down.
   A world is 256x256 metatiles of 16x16 px (4096 px each way, wrapping), grown from the save's seed. Nothing of it is stored:
   every metatile is worked out from smooth value noise when it scrolls into view (height: water, sand, grass, rocks up high;
   a second noise for where trees and flowers grow). The kind of world (GARDEN, DESERT, ICE, JUNGLE, OCEAN, DUSK) comes from the
   seed too, and only changes the colours and how much water and forest there is.
   The hardware BG map is 32x32 tiles (16x16 metatiles); as the camera moves, the row or column that comes into view is drawn.
   The ship you landed in is a sprite: A next to it saves and heals. Berry bushes heal a little. The door leads down (step 7). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"
#include "sysmenu.h"
#include "menus.h"
#include "bug.h"
#include "bugspr.h"
#include "planet_art.h"
#include "planet.h"

#define SHIP_TILE0 64            /* sprite tiles in VRAM bank 1 (the terrain uses bank-1 tiles 0..43) */
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
static UINT8 ground(UINT8 x, UINT8 y){                          /* the metatile at (x, y) */
    const WorldKind *k = &kinds[wk]; UINT8 h, v, r;
    UINT8 dx = x - ship_mx + 1, dy = y - ship_my + 1;
    if(dx < 4 && dy < 4) return MT_PAD;                           /* the landing pad around the ship (you step out onto its bottom row) */
    if(x == door_mx && y == door_my) return MT_DOOR;
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
static UINT8 solid(UINT8 mt){ return mt == MT_DEEP || mt == MT_TREE || mt == MT_ROCK; }

/* ------------------------------------------------------------ drawing */
static void draw_mt(UINT8 x, UINT8 y){                          /* metatile (x, y) into the BG map, where it falls */
    UINT8 mt = ground(x, y), t[4], a[4], bx2 = (x & 15) << 1, by2 = (y & 15) << 1, p = 0x08 | (4 + planet_mtpal[mt]);   /* 0x08: tiles from VRAM bank 1 */
    t[0] = mt * 4; t[1] = mt * 4 + 1; t[2] = mt * 4 + 2; t[3] = mt * 4 + 3;
    a[0] = a[1] = a[2] = a[3] = p;
    VBK_REG = VBK_BANK_1; set_bkg_tiles(bx2, by2, 2, 2, a);
    VBK_REG = VBK_BANK_0; set_bkg_tiles(bx2, by2, 2, 2, t);
}
static void draw_all(void){
    UINT8 i, j;
    left_mt = (UINT8)(cam_x >> 4); top_mt = (UINT8)(cam_y >> 4);
    for(j = 0; j < VIEW_H + 1; j++) for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt + j);
}
static void scroll_fill(void){                                   /* draw the column or row that has just come into view */
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
    seed_a = (UINT16)sv.seed; seed_b = (UINT16)(sv.seed >> 16) ^ 0x1234u; if(!seed_b) seed_b = 1;
    sa1 = (UINT8)seed_a; sa2 = (UINT8)(seed_a >> 8); sb1 = (UINT8)seed_b; sb2 = (UINT8)(seed_b >> 8) ^ 0x5A;
    wk = (UINT8)((sv.seed >> 8) % PL_NWORLD);
    ship_mx = 250; ship_my = 250;                                /* (out of the way while we look for land) */
    door_mx = 0; door_my = 0;
    find_land(&ship_mx, &ship_my, 128, 128, 2);                  /* a clearing big enough to land in */
    { UINT8 d = (UINT8)(seed_b & 3), dx = d == 0 ? 14 : d == 1 ? (UINT8)-14 : 3, dy = d == 2 ? 14 : d == 3 ? (UINT8)-14 : 5;
      find_land(&door_mx, &door_my, ship_mx + dx, ship_my + dy, 1); }
    bx = ((UINT16)ship_mx << 4) + 8; by = ((UINT16)ship_my << 4) + 40;   /* you step out just below the ship */
    npicked = 0;
}
static void load_gfx(void){
    UINT8 i;
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, PL_NMT * 4, planet_tiles);
    set_sprite_data(SHIP_TILE0, 16, ship_tiles);
    VBK_REG = VBK_BANK_0;
    set_bkg_palette(4, 4, &planet_pals[wk * 16]);
    set_sprite_palette(SHIP_PAL, 1, ship_pal);
    for(i = 0; i < 8; i++){ set_sprite_tile(SHIP_OAM0 + i, SHIP_TILE0 + i * 2); set_sprite_prop(SHIP_OAM0 + i, SHIP_PAL | 0x08); }
}
static void camera(void){ cam_x = (bx - BUG_SX) & 4095; cam_y = (by - BUG_SY) & 4095; }
static void redraw(void){ SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; txt_pal_reset(); set_bkg_palette(4, 4, &planet_pals[wk * 16]); draw_all(); }

/* ------------------------------------------------------------ walking */
static UINT8 at(UINT16 x, UINT16 y){ return ground((UINT8)((x & 4095) >> 4), (UINT8)((y & 4095) >> 4)); }
static UINT8 blocked(UINT16 x, UINT16 y){                       /* the bug's feet: a box 10 wide and 4 tall */
    return solid(at(x - 5, y - 3)) || solid(at(x + 4, y - 3)) || solid(at(x - 5, y)) || solid(at(x + 4, y));
}
static void eat_berry(UINT8 x, UINT8 y){
    if(npicked < 32){ picked[npicked][0] = x; picked[npicked][1] = y; npicked++; }
    else { memmove(picked[0], picked[1], 31 * 2); picked[31][0] = x; picked[31][1] = y; }
    draw_mt(x, y);
    { UINT16 mx = hud_hpmax(sv.level); sv.hp = sv.hp + 3 > mx ? mx : sv.hp + 3; }
    sv.gold += 2; sv.xp += 1;
}
static UINT8 near_ship(void){
    UINT16 dx = (bx - ((UINT16)ship_mx << 4) - 8) & 4095, dy = (by - ((UINT16)ship_my << 4)) & 4095;
    if(dx > 2048) dx = 4096 - dx; if(dy > 2048) dy = 4096 - dy;
    return dx < 28 && dy < 40;
}

int planet_run(void) BANKED {                                    /* returns PL_TITLE when the person quits to the title */
    UINT8 frame = 0, step = 0, flip = 0, k, ox, oy, was_door = 0; char msg[96], *d;
    DISPLAY_OFF;
    cpu_fast();
    for(k = 0; k < 40; k++) move_sprite(k, 0, 0);               /* nothing left over from the menus */
    world_setup();
    load_gfx();
    txt_pal_reset();
    camera(); SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; draw_all();
    hud_init();
    bugspr_load(&sv.look);
    SHOW_BKG; SHOW_SPRITES;
    DISPLAY_ON;
    play_reset(sv.secs);
    ship_show(); bugspr_show(BUG_SX - bugspr_w() / 2, BUG_SY - 36, flip, 0);
    d = msg; d = scat(d, "WE LANDED. THIS IS "); d = scat(d, planet_world_name[wk]); d = scat(d, " WORLD. FIND THE DOOR AND GO DOWN. PRESS A AT THE SHIP TO SAVE.");
    dlg_say("BOLT", msg);
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
        if(ox == door_mx && oy == door_my){ if(!was_door){ was_door = 1; dlg_say("BOLT", "THE DOOR DOWN. IT IS SEALED FOR NOW: THE DUNGEONS ARE STILL BEING DUG. COME BACK SOON."); } }
        else was_door = 0;
        wait_vbl_done();
        SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y;
        bugspr_show(BUG_SX - bugspr_w() / 2, BUG_SY - 36, flip, frame);
        ship_show();
        hud_update();
        if(HIT(J_A) && near_ship()){
            sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level); slot_write(cur_slot);
            dlg_say("BOLT", "HOME SWEET SHIP. YOU ARE HEALED, AND THE GAME IS SAVED.");
        }
        if(HIT(J_START)){
            if(sysmenu_run(redraw) == SYS_TITLE){ hud_hide(); bugspr_hide(); for(k = 0; k < 8; k++) move_sprite(SHIP_OAM0 + k, 0, 0); SCX_REG = 0; SCY_REG = 0; txt_cls(); return PL_TITLE; }
            bugspr_load(&sv.look);
        }
    }
}
