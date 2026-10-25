#pragma bank 18
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
#include "menus.h"
#include "space_art.h"
#include "space.h"
#include "warp.h"
#include "econ.h"
#include "elite.h"
#include "space_int.h"
#include "space3d_art.h"
#include "space3d.h"
#include "space3d_int.h"


INT16  sz;                           /* the ship's height, 1/16 unit */
UINT16 yaw;                          /* 8.8 */
INT16  pit16;                        /* 8.8 */
INT16  vel;                          /* 1/16 unit a frame */
INT16  vfx, vfy, vfz, vrx, vry, vux, vuy, vuz;   /* the ship's axes, 64 = 1.0 */
P3     pir[NP3];
B3     bolt[NB3], orb[NO3];
Pod3   pod[NPOD3];
INT8   dust_x[NDUST], dust_y[NDUST]; UINT8 dust_z[NDUST];
UINT8  dn, prev_dn;
static UINT8 dl_t[40], dl_x[40], dl_y[40], dl_p[40];
UINT8  pk3[4];                       /* the kind each planet palette (4..7) holds */
static UINT8 pal_next;
UINT8  arena_on, edge_on; INT16 ed_x, ed_y, ed_z;
static Cand cand[16]; static UINT8 ncand;
UINT8 near_pl, near_st, tgt;
INT16 tgt_d;

/* ---------------------------------------------------------------- the maths */
void s3_basis(void) BANKED {
    UINT8 a = (UINT8)(yaw >> 8), p = (UINT8)(pit16 >> 8);
    INT16 sp = s3_sin[a], cp = s3_sin[(UINT8)(a + 64)], st = s3_sin[p], ct = s3_sin[(UINT8)(p + 64)];
    vfx = (ct * sp) >> 6; vfy = (ct * cp) >> 6; vfz = st;
    vrx = cp; vry = -sp;
    vux = -((st * sp) >> 6); vuy = -((st * cp) >> 6); vuz = ct;
}
/* camera coordinates of an offset (units, |each| <= 500): returns 1 and the screen place and depth when it is in front of the ship */
static UINT8 proj(INT16 dx, INT16 dy, INT16 dz, INT16 *sx, INT16 *sy, INT16 *depth){
    INT16 cz = ((dx * vfx) >> 6) + ((dy * vfy) >> 6) + ((dz * vfz) >> 6), cx, cy;
    UINT16 a, b;
    if(cz < 6 || cz > 600) return 0;
    cx = ((dx * vrx) >> 6) + ((dy * vry) >> 6);
    cy = ((dx * vux) >> 6) + ((dy * vuy) >> 6) + ((dz * vuz) >> 6);
    a = cx < 0 ? -cx : cx; b = cy < 0 ? -cy : cy;
    if(a > 700 || b > 700) return 0;
    a = (a * 80) / (UINT16)cz; b = (b * 80) / (UINT16)cz;
    if(a > 300 || b > 300) return 0;
    *sx = cx < 0 ? 80 - (INT16)a : 80 + (INT16)a;
    *sy = cy < 0 ? 60 + (INT16)b : 60 - (INT16)b;
    *depth = cz;
    return 1;
}

/* ---------------------------------------------------------------- the sprite list */
static void spr(UINT8 tile, INT16 x, INT16 y, UINT8 pal){                  /* x, y: the sprite's top left on the screen */
    if(dn >= 40 || x < -7 || x > 159 || y < -15 || y > 119) return;
    dl_t[dn] = tile; dl_x[dn] = (UINT8)(x + 8); dl_y[dn] = (UINT8)(y + 16); dl_p[dn] = pal | 0x08; dn++;
}
static UINT8 blit(UINT8 tile, UINT8 cols, UINT8 rows, INT16 cx, INT16 cy, UINT8 pal){     /* cols x rows of 8x16 sprites, middle at (cx, cy) */
    UINT8 r, c;
    INT16 x0 = cx - cols * 4, y0 = cy - rows * 8;
    if(dn + cols * rows > 40) return 0;
    for(r = 0; r < rows; r++) for(c = 0; c < cols; c++) spr(tile + 2 * (r * cols + c), x0 + c * 8, y0 + r * 16, pal);
    return 1;
}
static void dot(UINT8 tile, INT16 cx, INT16 cy, UINT8 pal){ spr(tile, cx - 4, cy - 4, pal); }   /* the picture is in the top 8x8 of its sprite */

/* ---------------------------------------------------------------- worlds */
static UINT8 pal_for(UINT8 kind){                                           /* a planet palette slot (4..7) that holds this kind */
    UINT8 i;
    for(i = 0; i < 4; i++) if(pk3[i] == kind) return 4 + i;
    i = pal_next; pal_next = (pal_next + 1) & 3;
    pk3[i] = kind; space_kind_pal(4 + i, kind);
    return 4 + i;
}

void s3_scan(void) BANKED {                                                     /* every star near the ship: what to draw, what is in reach, the nearest */
    UINT8 cx = (UINT8)(((shx >> 4) >> 8) & 15), cy = (UINT8)(((shy >> 4) >> 8) & 15), i, j, nearest = 0;
    INT16 best = 32000, sx, sy, cz;
    ncand = 0; near_pl = near_st = 255;
    for(j = 0; j < 5; j++) for(i = 0; i < 5; i++){
        UINT8 s = (UINT8)((((cy + j - 2) & 15) << 4) | ((cx + i - 2) & 15));
        INT16 dx = unit(star_px(s) << 4, shx), dy = unit(star_py(s) << 4, shy), dz = star_z(s) - (sz >> 4), ad, ex, ez;
        if(!inbox(dx) || !inbox(dy)) continue;
        ad = aabs(dx) + aabs(dy) + aabs(dz);
        if(ad < best){ best = ad; nearest = s; }
        if(aabs(dx) < LAND_R && aabs(dy) < LAND_R && aabs(dz) < LAND_R) near_pl = s;
        ex = dx + ST_DX; ez = dz + 8;
        if(aabs(ex) < DOCK_R && aabs(dy) < DOCK_R && aabs(ez) < DOCK_R) near_st = s;
        if(ncand < 15 && proj(dx, dy, dz, &sx, &sy, &cz)){ cand[ncand].s = s; cand[ncand].st = 0; cand[ncand].sx = sx; cand[ncand].sy = sy; cand[ncand].cz = cz; ncand++; }
        if(ncand < 15 && ad < 330 && proj(ex, dy, ez, &sx, &sy, &cz)){ cand[ncand].s = s; cand[ncand].st = 1; cand[ncand].sx = sx; cand[ncand].sy = sy; cand[ncand].cz = cz; ncand++; }
    }
    tgt = sv.el.jk == J_SEND ? sv.el.jd : nearest;
}

/* where the target is: the world itself, an arrow at the edge when it is off the screen */
static void edge_mark(INT16 dx, INT16 dy, INT16 dz, UINT8 above){        /* where an offset (units) is: a marker above it on the screen, or an arrow at the edge */
    INT16 sx = 80, sy = 60, cz = 0, ox, oy;
    UINT8 on = 0;
    if(inbox(dx) && inbox(dy)) on = proj(dx, dy, dz, &sx, &sy, &cz);
    if(on && sx > 8 && sx < 152 && sy > 12 && sy < 110){
        if(above){ INT16 r = cz < 140 ? 28 : cz < 230 ? 20 : 12; spr(TS(ARW_D), sx - 4, sy - r - 12, PAL_MISC); }
        return;
    }
    while(!inbox(dx) || !inbox(dy)){ dx >>= 1; dy >>= 1; dz >>= 1; }          /* (far away: the same direction, smaller) */
    ox = ((dx * vrx) >> 6) + ((dy * vry) >> 6);
    oy = ((dx * vux) >> 6) + ((dy * vuy) >> 6) + ((dz * vuz) >> 6);
    if(aabs(ox) >= aabs(oy)) spr(ox > 0 ? TS(ARW_R) : TS(ARW_L), ox > 0 ? 148 : 4, 52, PAL_MISC);
    else spr(oy > 0 ? TS(ARW_U) : TS(ARW_D), 76, oy > 0 ? 6 : 100, PAL_MISC);
}
static void target_mark(void){
    UINT8 s = tgt;
    INT16 dx = unit(star_px(s) << 4, shx), dy = unit(star_py(s) << 4, shy), dz = star_z(s) - (sz >> 4);
    tgt_d = aabs(dx) > aabs(dy) ? aabs(dx) + aabs(dy) / 2 : aabs(dy) + aabs(dx) / 2;
    edge_mark(dx, dy, dz, 1);
}

/* ---------------------------------------------------------------- scene */
static void tiles3(void){
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, 4, s3_bg);
    set_sprite_data(S0, S3_NTILES, s3_tiles);
    space_spr_copy(T_PIR, SP_T_PIRATE(0), 8); space_spr_copy(T_BOOM, SP_T_BOOM(0), 8);
    space_spr_copy(T_ORB, SP_T_ORB, 2); space_spr_copy(T_POD, SP_T_POD, 2);
    VBK_REG = VBK_BANK_0;
    space3_pals();
    pk3[0] = pk3[1] = pk3[2] = pk3[3] = 255;
}
static void sky3(void){                                                     /* the star background: 32x32 tiles, bank 1, palette 3 */
    UINT8 x, y, row[32], at[32];
    UINT16 h = 0xACE1;
    for(x = 0; x < 32; x++) at[x] = 0x08 | 3;
    for(y = 0; y < 32; y++){
        for(x = 0; x < 32; x++){
            UINT8 r;
            h ^= h << 7; h ^= h >> 9; h ^= h << 8; r = (UINT8)h;
            row[x] = r < 6 ? 3 : r < 30 ? 2 : r < 70 ? 1 : 0;
        }
        VBK_REG = VBK_BANK_1; set_bkg_tiles(0, y, 32, 1, at);
        VBK_REG = VBK_BANK_0; set_bkg_tiles(0, y, 32, 1, row);
    }
}
void s3_dust_born(UINT8 i, UINT8 far) BANKED {
    do { dust_x[i] = (INT8)rnd(); dust_y[i] = (INT8)rnd(); } while(dust_x[i] > -14 && dust_x[i] < 14 && dust_y[i] > -14 && dust_y[i] < 14);
    dust_z[i] = far ? 255 - (rnd() & 31) : 30 + rnd() % 220;
}
static void list_things(UINT8 frame){
    UINT8 i;
    INT16 sx, sy, cz, dx, dy;
    for(i = 0; i < NB3; i++){ B3 *b = &bolt[i];
        dx = unit(b->x, shx); dy = unit(b->y, shy);
        if(b->life && inbox(dx) && inbox(dy) && proj(dx, dy, (b->z - sz) >> 4, &sx, &sy, &cz)) dot(cz < 70 ? TS(DOT2) : TS(DOT1), sx, sy, PAL_LAS);
    }
    for(i = 0; i < NO3; i++){ B3 *o = &orb[i];
        dx = unit(o->x, shx); dy = unit(o->y, shy);
        if(o->life && inbox(dx) && inbox(dy) && proj(dx, dy, (o->z - sz) >> 4, &sx, &sy, &cz)) spr(T_ORB, sx - 4, sy - 4, PAL_ORB);
    }
    for(i = 0; i < NP3; i++){ P3 *p = &pir[i];
        dx = unit(p->x, shx); dy = unit(p->y, shy);
        if(!p->st || !inbox(dx) || !inbox(dy) || !proj(dx, dy, (p->z - sz) >> 4, &sx, &sy, &cz)) continue;
        if(p->st >= 2){ if(cz < 200) blit(T_BOOM + (p->st > 12 ? 4 : 0), 2, 1, sx, sy, PAL_ORB); else dot(TS(DOT3), sx, sy, PAL_ORB); }
        else if(cz < 170) blit(T_PIR + ((frame >> 3) & 1) * 4, 2, 1, sx, sy, p->kind ? PAL_BOSS : PAL_PIR);
        else dot(TS(DOT2), sx, sy, p->kind ? PAL_BOSS : PAL_PIR);
    }
    for(i = 0; i < NPOD3; i++){ Pod3 *q = &pod[i];
        dx = unit(q->x, shx); dy = unit(q->y, shy);
        if(q->life && inbox(dx) && inbox(dy) && proj(dx, dy, (q->z - sz) >> 4, &sx, &sy, &cz)){
            if(cz < 160) spr(T_POD, sx - 4, sy - 8, PAL_LAS); else dot(TS(DOT2), sx, sy, PAL_LAS);
        }
    }
    for(i = 0; i < NDUST; i++){
        sx = 80 + ((INT16)dust_x[i] * 64) / dust_z[i]; sy = 60 + ((INT16)dust_y[i] * 64) / dust_z[i];
        if(sx < 2 || sx > 156 || sy < 2 || sy > 116){ s3_dust_born(i, 1); continue; }
        dot(TS(DOT1), sx, sy, PAL_MISC);
    }
}
static void draw_cand(Cand *c){
    INT16 cz = c->cz;
    if(c->st){
        if(cz < 110){ if(!blit(TS(ST16), 2, 1, c->sx, c->sy, PAL_MISC)) dot(TS(ST8), c->sx, c->sy, PAL_MISC); }
        else dot(TS(ST8), c->sx, c->sy, PAL_MISC);
    } else {
        UINT8 pal = pal_for(star_k[c->s]), v = c->s & 1, ok = 0;
        if(cz < 150) ok = blit(S0 + (v ? S3_T_D48_1 : S3_T_D48_0), 6, 3, c->sx, c->sy, pal);
        if(!ok && cz < 240) ok = blit(S0 + (v ? S3_T_D32_1 : S3_T_D32_0), 4, 2, c->sx, c->sy, pal);
        if(!ok && cz < 500) ok = blit(S0 + (v ? S3_T_D16_1 : S3_T_D16_0), 2, 1, c->sx, c->sy, pal);
        if(!ok) dot(S0 + (v ? S3_T_DOT_PL1 : S3_T_DOT_PL0), c->sx, c->sy, pal);
    }
}
static void list_worlds(void){
    UINT8 done[16], n, a, best; INT16 bz;
    for(n = 0; n < ncand; n++) done[n] = 0;
    for(n = 0; n < ncand; n++){
        best = 255; bz = 32000;
        for(a = 0; a < ncand; a++) if(!done[a] && cand[a].cz < bz){ bz = cand[a].cz; best = a; }
        if(best == 255) break;
        done[best] = 1;
        draw_cand(&cand[best]);
    }
}
void s3_flush(UINT8 frame) BANKED {                                             /* (in VBlank) the sprites, the background's turn, the twinkle */
    UINT8 i;
    shake_tick();
    SCX_REG = (UINT8)((((yaw >> 5) * 5) >> 4) + shake_x); SCY_REG = (UINT8)(-(((INT16)(pit16 >> 8) * 5) >> 1) + shake_y);
    for(i = 0; i < dn; i++){ set_sprite_tile(i, dl_t[i]); set_sprite_prop(i, dl_p[i]); move_sprite(i, dl_x[i], dl_y[i]); }
    for(; i < prev_dn; i++) move_sprite(i, 0, 0);
    prev_dn = dn;
    if(!(frame & 31)) set_bkg_palette_entry(3, 3, (frame & 32) ? RGB(20, 24, 31) : RGB(31, 31, 31));
}

void s3_render(UINT8 frame) BANKED {
    dn = 0;
    spr(TS(CROSS), 76, 52, PAL_MISC);
    if(arena_on){ if(edge_on) edge_mark(ed_x, ed_y, ed_z, 0); } else target_mark();
    list_things(frame);
    if(!arena_on) list_worlds();
}
void s3_load(void) BANKED {
    UINT8 i;
    tiles3(); sky3();
    SPRITES_8x16;
    txt_pal_reset(); space3_pals();
    for(i = 0; i < 40; i++) move_sprite(i, 0, 0);
    SCX_REG = 0; SCY_REG = 0;
    prev_dn = 40;
}
