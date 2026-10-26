#pragma bank 5
/* BUGSTAR GBC - step 7: the dungeon, top-down.
   Every floor is 32x32 metatiles of 16x16 px, grown from the save's seed and the floor number: up to 8 rooms joined by corridors,
   the way up in the first room, the way down (or, on the last floor, the queen and a caged friend) in the last one, chests,
   potions, and monsters that come for you when you are close. A bites in the way you face, B runs, SELECT drinks a potion,
   START is the SYSTEM menu. A dungeon has 3 floors, plus one for every world already freed (up to 6). Beat the queen and the
   world is free. If your hearts run out you faint, and Bolt drags you back to the ship (you lose a quarter of your gold). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
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
#include "lab.h"
#include "menus.h"
#include "bug.h"
#include "bugspr.h"
#include "dungeon_art.h"
#include "dungeon.h"
#include "feat.h"

#define DW        32               /* the floor is DW x DW metatiles */
#define NROOM     8
#define NMON      10
#define VIEW_W    11
#define VIEW_H    9
#define BUG_SX    80               /* the screen spot the camera keeps the bug's feet near */
#define BUG_SY    72
#define MON_TILE0 64               /* monster sprite tiles live in VRAM bank 1 from here */
#define OAM_BITE  0                /* 0,1: the bite flash. 4..14: the bug. 16..39: monsters */
#define OAM_MON0  16

enum { K_SPIDER, K_BEETLE, K_WASP, K_SLIME, K_QUEEN };
typedef struct { UINT16 x, y; UINT8 kind, hp, cool, flash, frame, alive; INT8 kx, ky; UINT8 st, t; INT8 lx, ly; } Mon;
/* st/t: spiders crouch (st 1, they blink) then lunge (st 2); a slime with st 9 is a small one that will not split again;
   the queen counts t up to calling her spiderlings */
typedef struct { UINT8 x, y, w, h; } Room;

#define dmap WRAM_AT(WRAM_DMAP, UINT8)                  /* DW x DW (wram.h) */
static Room   rooms[NROOM]; static UINT8 nrooms;
static Mon    mon[NMON];
typedef struct { UINT16 x, y; UINT8 t, n, pal; } Fx;              /* a burst (n = 255) or a damage number rising, t frames left */
#define NFX 4
static Fx     fxs[NFX];
static UINT8  depth, floors, kind, inv, atk, face, frame, step;
static UINT8  hell;
static UINT8  pit, pit_wave, pit_tier;                          /* the city pit: one big room, waves of monsters */
static const UINT8 qbrood[6] = { K_SPIDER, K_BEETLE, K_WASP, K_SLIME, K_SPIDER, K_WASP };   /* what each kind of queen calls */
static UINT8  evt, lab;                                         /* this floor's event (spell.h); lab: this floor is a maze */
static UINT8  stop;                                             /* hit-stop: frames the action holds still after a good bite */
static UINT16 bx, by, cam_x, cam_y, rng;
static UINT8  left_mt, top_mt;

static UINT8 rnd8(void){ rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; return (UINT8)rng; }
static UINT8 rndn(UINT8 n){ return n ? (UINT8)(((UINT16)rnd8() * n) >> 8) : 0; }
#define DM(x, y) dmap[((y) & 31) * DW + ((x) & 31)]
static UINT8 solid_mt(UINT8 t){ return t == D_WALL || t == D_FACE || t == D_CAGE || t == D_CHEST || t == D_OPEN || t == D_FREED; }
static UINT8 ground_at(UINT16 x, UINT16 y){ if(x >= DW * 16 || y >= DW * 16) return D_WALL; return DM(x >> 4, y >> 4); }

/* ------------------------------------------------------------ making a floor */
static void carve(UINT8 x, UINT8 y){ if(x > 0 && y > 0 && x < DW - 1 && y < DW - 1 && (DM(x, y) == D_WALL)) DM(x, y) = (rnd8() & 3) ? D_FLOOR : D_FLOOR2; }
static void make_floor(void){
    UINT8 i, t, x, y, a, b;
    rng = (UINT16)world_seed(sv.star) ^ ((UINT16)depth * 0x3C1Bu) ^ ((UINT16)sv.zone << 11) ^ 0x2B7Du; if(!rng) rng = 1;
    memset(dmap, D_WALL, DW * DW);
    nrooms = 0;
    lab = !pit && depth > 1 && depth < floors && !(rnd8() & 3);
#ifdef TEST_LAB
    lab = !pit;
#endif
    if(lab){ lab_make(dmap, (UINT8 *)rooms, rng, D_WALL, D_FLOOR, D_FLOOR2); nrooms = 5; }
    if(pit){ Room *r = &rooms[0]; r->x = 8; r->y = 9; r->w = 16; r->h = 13; nrooms = 1; for(y = r->y; y < r->y + r->h; y++) for(x = r->x; x < r->x + r->w; x++) carve(x, y); }
    for(t = 0; !pit && !lab && t < 60 && nrooms < NROOM; t++){                    /* rooms: place them where they do not touch */
        Room r; UINT8 ok = 1;
        r.w = 4 + rndn(5); r.h = 4 + rndn(4); r.x = 1 + rndn(DW - 2 - r.w); r.y = 2 + rndn(DW - 3 - r.h);
        for(i = 0; i < nrooms && ok; i++){ Room *o = &rooms[i];
            if(r.x < o->x + o->w + 2 && o->x < r.x + r.w + 2 && r.y < o->y + o->h + 2 && o->y < r.y + r.h + 2) ok = 0; }
        if(!ok) continue;
        rooms[nrooms++] = r;
        for(y = r.y; y < r.y + r.h; y++) for(x = r.x; x < r.x + r.w; x++) carve(x, y);
    }
    for(i = 1; !pit && !lab && i < nrooms; i++){                         /* corridors: each room to the one before, an L shape */
        UINT8 x0 = rooms[i - 1].x + rooms[i - 1].w / 2, y0 = rooms[i - 1].y + rooms[i - 1].h / 2, x1 = rooms[i].x + rooms[i].w / 2, y1 = rooms[i].y + rooms[i].h / 2;
        a = x0 < x1 ? x0 : x1; b = x0 < x1 ? x1 : x0; for(x = a; x <= b; x++) carve(x, y0);
        a = y0 < y1 ? y0 : y1; b = y0 < y1 ? y1 : y0; for(y = a; y <= b; y++) carve(x1, y);
    }
    for(y = DW - 2; y > 0; y--) for(x = 0; x < DW; x++)           /* a wall with floor below shows its face */
        if(DM(x, y) == D_WALL && (DM(x, y + 1) == D_FLOOR || DM(x, y + 1) == D_FLOOR2)) DM(x, y) = D_FACE;
    { Room *r = &rooms[0]; DM(r->x + r->w / 2, r->y + 1) = D_UP; bx = ((UINT16)(r->x + r->w / 2) << 4) + 8; by = ((UINT16)(r->y + 3) << 4) - 2; }
    evt = (pit || depth == floors) ? 0 : lab ? 7 : ev_roll(depth);
    if(pit){ memset(mon, 0, sizeof mon); return; }
    { Room *r = &rooms[nrooms - 1];
      if(depth < floors) DM(r->x + r->w / 2, r->y + r->h / 2) = D_DOWN;
      else DM(r->x + r->w / 2, r->y + 1) = D_CAGE; }               /* the last floor: a caged friend, guarded */
    for(i = 1; i < nrooms; i++){                                  /* chests and potions */
        Room *r = &rooms[i];
        if(evt == 1 || rndn(3) == 0){ x = r->x + 1 + rndn(r->w - 2); y = r->y + 1 + rndn(r->h - 2); if(DM(x, y) == D_FLOOR || DM(x, y) == D_FLOOR2) DM(x, y) = D_CHEST; }
        if(rndn(5) == 0){ x = r->x + rndn(r->w); y = r->y + rndn(r->h); if(DM(x, y) == D_FLOOR || DM(x, y) == D_FLOOR2) DM(x, y) = D_POTION; }
    }
    memset(mon, 0, sizeof mon);                                    /* monsters: in every room but the first */
    for(i = 0, t = 0; t < 40 && i < NMON; t++){
        Room *r = &rooms[1 + rndn(nrooms - 1)]; Mon *m = &mon[i];
        UINT8 k = rndn(depth + 1 > 4 ? 4 : depth + 1);
        x = r->x + rndn(r->w); y = r->y + rndn(r->h);
        if(DM(x, y) != D_FLOOR && DM(x, y) != D_FLOOR2) continue;
        m->x = ((UINT16)x << 4) + 8; m->y = ((UINT16)y << 4) + 12; m->kind = k; m->alive = 1;
        m->hp = (k == K_BEETLE ? 8 : k == K_WASP ? 3 : k == K_SLIME ? 5 : 4) + depth * 2 + sv.zone * 3;
        if(evt == 5) m->hp += m->hp / 2;
        if(hell) m->hp = hell_hp(m->hp);
        i++;
        if(i >= (UINT8)(4 + depth * 2 + (evt == 3 ? 3 : 0) - (evt == 6 ? 3 : 0))) break;
    }
    if(depth == floors && nrooms > 1){                             /* the queen waits by the cage */
        Room *r = &rooms[nrooms - 1]; Mon *m = &mon[NMON - 1];
        memset(m, 0, sizeof *m); m->kind = K_QUEEN; m->alive = 1; m->x = ((UINT16)(r->x + r->w / 2) << 4) + 8; m->y = ((UINT16)(r->y + r->h / 2 + 1) << 4) + 8;
        m->hp = sv.zone >= 8 ? 250 : 40 + sv.zone * 25;
    }
}

/* ------------------------------------------------------------ drawing */
/* Walls and floors are blended like the planet's ground (dungeon_art.h): a quarter whose cell and three neighbours are not all
   the same (wall or floor) is a blend tile, flipped to face its corner, so rooms get rounded corners with a shadow along the wall. */
static UINT8 mt_at(UINT8 x, UINT8 y){ return (x < DW && y < DW) ? DM(x, y) : D_WALL; }
static void draw_mt(UINT8 x, UINT8 y){
    UINT8 mt = mt_at(x, y), t[4], a[4], bx2 = (x & 15) << 1, by2 = (y & 15) << 1, q, l0, l1, l2, l3, sx, sy;
    if(dg_mtlvl[mt] >= 4){
        t[0] = mt * 4; t[1] = mt * 4 + 1; t[2] = mt * 4 + 2; t[3] = mt * 4 + 3; a[0] = a[1] = a[2] = a[3] = 0x08 | (4 + dg_mtpal[mt]);
    } else {
        l0 = dg_mtlvl[mt] & 1;
        for(q = 0; q < 4; q++){
            sx = (q & 1) ? x + 1 : x - 1; sy = (q & 2) ? y + 1 : y - 1;
            l1 = dg_mtlvl[mt_at(sx, y)] & 1; l2 = dg_mtlvl[mt_at(x, sy)] & 1; l3 = dg_mtlvl[mt_at(sx, sy)] & 1;
            if(l0 == l1 && l0 == l2 && l0 == l3){ t[q] = mt * 4 + q; a[q] = 0x08 | (4 + dg_mtpal[mt]); }
            else { t[q] = DG_NMT * 4 + (l0 | (l1 << 1) | (l2 << 2) | (l3 << 3)) - 1; a[q] = 0x08 | 4 | ((q & 1) ? 0x20 : 0) | ((q & 2) ? 0x40 : 0); }
        }
    }
    VBK_REG = VBK_BANK_1; set_bkg_tiles(bx2, by2, 2, 2, a);
    VBK_REG = VBK_BANK_0; set_bkg_tiles(bx2, by2, 2, 2, t);
}
static void camera(void){
    INT16 cx = (INT16)bx - BUG_SX, cy = (INT16)by - BUG_SY;
    if(cx < 0) cx = 0; if(cx > DW * 16 - 160) cx = DW * 16 - 160;
    if(cy < 0) cy = 0; if(cy > DW * 16 - 128) cy = DW * 16 - 128;
    cam_x = (UINT16)cx; cam_y = (UINT16)cy;
}
static void draw_all(void){
    UINT8 i, j; left_mt = (UINT8)(cam_x >> 4); top_mt = (UINT8)(cam_y >> 4);
    for(j = 0; j < VIEW_H + 1; j++) for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt + j);
}
static void scroll_fill(void){
    UINT8 l = (UINT8)(cam_x >> 4), t = (UINT8)(cam_y >> 4), i;
    while(l > left_mt){ left_mt++; for(i = 0; i < VIEW_H + 1; i++) draw_mt(left_mt + VIEW_W - 1, top_mt + i); }
    while(l < left_mt){ left_mt--; for(i = 0; i < VIEW_H + 1; i++) draw_mt(left_mt, top_mt + i); }
    while(t > top_mt){ top_mt++; for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt + VIEW_H); }
    while(t < top_mt){ top_mt--; for(i = 0; i < VIEW_W; i++) draw_mt(left_mt + i, top_mt); }
}
static void set_mt(UINT8 x, UINT8 y, UINT8 mt){ DM(x, y) = mt; draw_mt(x, y); }
static UINT8 show_fx(UINT8 o);
static void show_mons(void){                                       /* every monster near the screen, as sprites */
    UINT8 i, o = OAM_MON0, k;
    for(i = 0; i < NMON; i++){
        Mon *m = &mon[i]; INT16 sx, sy; UINT8 big, cols, rows, base, pal;
        if(!m->alive || (m->flash & 2)) continue;
        big = m->kind == K_QUEEN; cols = big ? 4 : 2; rows = big ? 2 : 1;
        sx = (INT16)m->x - (INT16)cam_x - (big ? 16 : 8); sy = (INT16)m->y - (INT16)cam_y - (big ? 30 : 14);
        if(sx < -32 || sx > 168 || sy < -32 || sy > 136) continue;
        if(o + cols * rows > 36) break;                              /* (keep a few for bursts and numbers) */
        base = MON_TILE0 + (big ? MON_T_QUEEN(m->frame) : MON_T_SMALL(m->kind, m->frame)); pal = big ? 0 : m->kind;
        for(k = 0; k < cols * rows; k++){
            set_sprite_tile(o, base + k * 2); set_sprite_prop(o, pal | 0x08);
            move_sprite(o, (UINT8)(sx + (k % cols) * 8 + 8), (UINT8)(sy + (k / cols) * 16 + 16)); o++;
        }
    }
    o = show_fx(o);
    while(o < 40) move_sprite(o++, 0, 0);
}
static void fx_add(UINT16 x, UINT16 y, UINT8 n, UINT8 pal){
    UINT8 i, best = 0;
    for(i = 1; i < NFX; i++) if(fxs[i].t < fxs[best].t) best = i;      /* a free one, or the oldest */
    fxs[best].x = x; fxs[best].y = y; fxs[best].n = n; fxs[best].pal = pal; fxs[best].t = n == 255 ? 16 : 32;
}
static UINT8 show_fx(UINT8 o){                                     /* bursts and numbers, in the sprites left over; returns the next free one */
    UINT8 i;
    for(i = 0; i < NFX; i++){
        Fx *f = &fxs[i]; INT16 sx, sy;
        if(!f->t) continue;
        f->t--;
        sx = (INT16)f->x - (INT16)cam_x; sy = (INT16)f->y - (INT16)cam_y;
        if(sx < -16 || sx > 176 || sy < -16 || sy > 140) continue;
        if(f->n == 255){                                           /* the burst: 16x16, two frames */
            UINT8 t = MON_TILE0 + MON_T_POOF(f->t < 8);
            if(o > 38) break;
            set_sprite_tile(o, t); set_sprite_tile(o + 1, t + 2); set_sprite_prop(o, f->pal | 0x08); set_sprite_prop(o + 1, f->pal | 0x08);
            move_sprite(o, (UINT8)sx, (UINT8)(sy + 8)); move_sprite(o + 1, (UINT8)(sx + 8), (UINT8)(sy + 8)); o += 2;
        } else {                                                   /* the number floats up */
            UINT8 d[3], nd = 0, v = f->n, k; INT16 x;
            do { d[nd++] = v % 10; v /= 10; } while(v && nd < 3);
            sy -= (32 - f->t) >> 1;
            if(o + nd > 40) break;
            x = sx - nd * 4 + 8;
            for(k = nd; k--;){
                set_sprite_tile(o, MON_TILE0 + MON_T_DIGIT(d[k])); set_sprite_prop(o, 2 | 0x08);
                move_sprite(o, (UINT8)x, (UINT8)(sy + 16)); o++; x += 7;
            }
        }
    }
    return o;
}
static void show_bite(void){
    if(!atk || atk < 4){ move_sprite(OAM_BITE, 0, 0); move_sprite(OAM_BITE + 1, 0, 0); return; }
    { INT16 sx = (INT16)(bx - cam_x) - 8, sy = (INT16)(by - cam_y) - 22;     /* 16x16, centred in front of the bug's head */
      if(face == 0) sx += 20; else if(face == 1) sx -= 20; else if(face == 2) sy -= 14; else sy += 18;
      set_sprite_tile(OAM_BITE, MON_TILE0 + MON_T_BITE); set_sprite_tile(OAM_BITE + 1, MON_TILE0 + MON_T_BITE + 2);
      set_sprite_prop(OAM_BITE, 3 | 0x08); set_sprite_prop(OAM_BITE + 1, 3 | 0x08);
      move_sprite(OAM_BITE, (UINT8)(sx + 8), (UINT8)(sy + 16)); move_sprite(OAM_BITE + 1, (UINT8)(sx + 16), (UINT8)(sy + 16)); }
}
static void show_bug(void){
    INT16 sx = (INT16)bx - (INT16)cam_x - bugspr_w() / 2, sy = (INT16)by - (INT16)cam_y - 36;
    if(inv & 4) bugspr_show(0, 200, face == 1, frame);           /* blink while you can't be hurt */
    else bugspr_show((UINT8)sx, (UINT8)sy, face == 1, frame);
}
static void enter_gfx(void){                                       /* everything onto the screen (LCD off) */
    UINT8 i;
    for(i = 0; i < 40; i++) move_sprite(i, 0, 0);
    VBK_REG = VBK_BANK_1;
    dungeon_tiles_load(MON_TILE0);
    VBK_REG = VBK_BANK_0;
    pet_load();
    txt_pal_reset();
    set_bkg_palette(4, 4, &dg_pals[kind * 16]);
    set_sprite_palette(0, 4, mon_pals);
    camera(); SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; draw_all();
}
static void redraw(void){ if(sys_gfx_lost){ enter_gfx(); return; } SCX_REG = (UINT8)cam_x; SCY_REG = (UINT8)cam_y; txt_pal_reset(); set_bkg_palette(4, 4, &dg_pals[kind * 16]); draw_all(); }

/* ------------------------------------------------------------ moving and fighting */
static UINT8 blocked(UINT16 x, UINT16 y, UINT8 r){                 /* a box 2r wide and 4 tall at the feet */
    return solid_mt(ground_at(x - r, y - 3)) || solid_mt(ground_at(x + r - 1, y - 3)) || solid_mt(ground_at(x - r, y)) || solid_mt(ground_at(x + r - 1, y));
}
static void hurt(UINT8 n, INT8 kx, INT8 ky){
    UINT8 k;
    if(inv) return;
    { UINT8 a = UPG(UP_SHELL) + UPG(SK_GUARD) / 3 + (TRICK(3) ? 1 : 0); n = n > a ? n - a : 1; n = diff_dmg(n); }
    sv.hp = sv.hp > n ? sv.hp - n : 0; inv = TRICK(5) ? 75 : 50; sfx(SFX_HURT); shake(10);
    for(k = 0; k < 10; k++){ if(!blocked(bx + kx, by + ky, 5)){ bx += kx; by += ky; } }   /* knocked back */
}
#ifdef TEST_BOSS
static UINT8 bite_power(void){ return 30; }
#else
static UINT8 bite_power(void){ return 2 + sv.level + (sv.friends > 6 ? 6 : sv.friends) + UPG(SK_MIGHT) + 2 * UPG(UP_FANG) + (TRICK(2) ? 2 : 0); }
#endif
static void mon_spawn(UINT8 kind, UINT16 x, UINT16 y, UINT8 hp, UINT8 st, UINT16 px, UINT16 py);
static UINT8 gmul(void){ return evt == 2 ? 3 : evt == 5 ? 2 : 1; }
static void spell_hit(Mon *m, UINT8 p){                            /* a spell hurts a monster (it never finishes the queen) */
    if(m->kind == K_QUEEN && p >= m->hp) p = m->hp - 1;
    if(p > m->hp) p = (UINT8)m->hp;
    m->hp -= p; m->flash = 12; fx_add(m->x, m->y - 18, p, 2);
    if(!m->hp){ m->alive = 0; sfx(SFX_DIE); fx_add(m->x - 8, m->y - 10, 255, m->kind); sv.gold += LUCKY(3 + m->kind * 2 + depth) * gmul(); gain_xp(3 + m->kind + depth); EX_KILLS++; }
}
static void cast(void){
    UINT8 s = EX_SPELL & 3, i;
    if(!spell_pay(s)) return;
    if(s > 1){ if(s == 3) inv = 250; return; }
    sfx(SFX_FIRE); shake(6);
    for(i = 0; i < NMON; i++){
        Mon *m = &mon[i]; UINT8 d;
        if(!m->alive) continue;
        d = spell_dmg(s, face, (INT16)m->x - (INT16)bx, (INT16)m->y - 6 - (INT16)by);
        if(d) spell_hit(m, d);
    }
}
static void do_bite(void){
    UINT8 i; INT16 hx = (INT16)bx + (face == 0 ? 16 : face == 1 ? -16 : 0), hy = (INT16)by - 6 + (face == 2 ? -16 : face == 3 ? 12 : 0);
    for(i = 0; i < NMON; i++){
        Mon *m = &mon[i]; INT16 dx, dy; UINT8 reach;
        if(!m->alive) continue;
        reach = m->kind == K_QUEEN ? 22 : 14;
        dx = (INT16)m->x - hx; dy = (INT16)m->y - 6 - hy; if(dx < 0) dx = -dx; if(dy < 0) dy = -dy;
        if(dx > reach || dy > reach) continue;
        { UINT8 p = bite_power(); if(p > m->hp) p = (UINT8)m->hp; m->hp -= p; fx_add(m->x, m->y - 18, p, 2); } sfx(SFX_HIT); stop = 3;
        m->flash = 12; m->kx = face == 0 ? 2 : face == 1 ? -2 : 0; m->ky = face == 3 ? 2 : face == 2 ? -2 : 0;
        if(!m->hp){
            m->alive = 0; sfx(m->kind == K_QUEEN ? SFX_FIRE : SFX_DIE); shake(m->kind == K_QUEEN ? 40 : 5); stop = 6;
            fx_add(m->x - 8, m->y - 10, 255, m->kind == K_QUEEN ? 0 : m->kind);
            if(m->kind == K_QUEEN){ fx_add(m->x - 24, m->y - 20, 255, 0); fx_add(m->x + 8, m->y - 22, 255, 3); fx_add(m->x - 8, m->y + 4, 255, 2); }   /* she goes up big */
            if(m->kind == K_SLIME && m->st != 9){ UINT8 h = 2 + depth; mon_spawn(K_SLIME, m->x - 10, m->y, h, 9, m->x, m->y); mon_spawn(K_SLIME, m->x + 10, m->y, h, 9, m->x, m->y); }   /* and slimes split */
            if(m->kind == K_QUEEN){ sv.gold += LUCKY(200 + sv.zone * 100); gain_xp(40 + sv.zone * 20); }
            else { sv.gold += LUCKY(3 + m->kind * 2 + depth) * gmul(); gain_xp(3 + m->kind + depth); EX_KILLS++; }
        }
    }
    { UINT8 tx = (UINT8)((UINT16)hx >> 4), ty = (UINT8)((UINT16)(hy + 6) >> 4);    /* biting a chest or a cage opens it */
      if(tx < DW && ty < DW){
          if(DM(tx, ty) == D_CHEST){ UINT16 g = 20 + depth * 10 + rndn(20); char b[32], *d; set_mt(tx, ty, D_OPEN); g = LUCKY(g); sv.gold += g; sfx(SFX_COIN);
              if(rndn(3) == 0 && sv.potions < 9){ sv.potions++; dlg_say(0, "THE CHEST HOLDS GOLD AND A HEALTH POTION. (B+SELECT DRINKS IT)"); }
              else { d = scat(b, "THE CHEST HOLDS "); d = u16s(d, g); scat(d, " GOLD"); dlg_say(0, b); } }
          else if(DM(tx, ty) == D_CAGE){
              UINT8 q; for(q = 0; q < NMON; q++) if(mon[q].alive && mon[q].kind == K_QUEEN) break;
              if(q < NMON) dlg_say("BOLT", "THE QUEEN IS STILL HERE. BEAT HER FIRST!");
              else { set_mt(tx, ty, D_FREED); sv.friends++; atk = 0; sfx(SFX_FANFARE); } } } }
}
static void mon_spawn(UINT8 kind, UINT16 x, UINT16 y, UINT8 hp, UINT8 st, UINT16 px, UINT16 py){   /* a new monster in a free slot (not the queen's); (px, py): where to put it if (x, y) is in a wall */
    UINT8 i;
    if(blocked(x, y, 5)){ x = px; y = py; }
    for(i = 0; i < NMON - 1; i++) if(!mon[i].alive){
        Mon *m = &mon[i]; memset(m, 0, sizeof *m);
        m->kind = kind; m->x = x; m->y = y; m->hp = hp; m->st = st; m->alive = 1; m->flash = 8;
        fx_add(x - 8, y - 10, 255, kind);
        return;
    }
}
static void mon_step(void){
    UINT8 i;
    for(i = 0; i < NMON; i++){
        Mon *m = &mon[i]; INT16 dx, dy; UINT8 sp, n, r;
        if(!m->alive) continue;
        if(m->flash) m->flash--;
        if(m->kx || m->ky){ if(!blocked(m->x + m->kx * 2, m->y + m->ky * 2, 5)){ m->x += m->kx * 2; m->y += m->ky * 2; } if(!m->flash || m->flash < 6){ m->kx = m->ky = 0; } continue; }
        dx = (INT16)bx - (INT16)m->x; dy = (INT16)by - (INT16)m->y;
        if(dx > 112 || dx < -112 || dy > 100 || dy < -100) continue;       /* asleep until you come near */
        r = m->kind == K_QUEEN ? 12 : 5;
        if(m->kind == K_SPIDER){                                    /* crouch, blink, then lunge */
            if(m->st == 1){ m->flash = 2 + (m->t & 2); if(!--m->t){ m->st = 2; m->t = 12; m->lx = dx > 4 ? 3 : dx < -4 ? -3 : 0; m->ly = dy > 4 ? 3 : dy < -4 ? -3 : 0; } continue; }
            if(m->st == 2){
                if(!blocked(m->x + m->lx, m->y, r)) m->x += m->lx;
                if(!blocked(m->x, m->y + m->ly, r)) m->y += m->ly;
                if(!--m->t){ m->st = 0; m->t = 80; }
                goto contact;
            }
            if(m->t) m->t--;
            else if(dx > -56 && dx < 56 && dy > -48 && dy < 48){ m->st = 1; m->t = 18; sfx(SFX_HISS); continue; }
        }
        if(m->kind == K_QUEEN && ++m->t >= 230 - kind * 12){        /* she calls her brood: each kind of queen calls something else, some also stamp */
            UINT8 bk = qbrood[kind];
            m->t = 0; sfx(SFX_HISS); shake(8);
            mon_spawn(bk, m->x - 20, m->y + 4, 3 + depth, 0, m->x, m->y + 16); mon_spawn(bk, m->x + 20, m->y + 4, 3 + depth, 0, m->x, m->y + 16);
            if(kind == 1 || kind == 3 || kind == 5) mon_spawn(bk, m->x, m->y + 22, 3 + depth, 0, m->x, m->y + 16);
            if(kind == 2 || kind == 5){ shake(14); sfx(SFX_BOOM); if(dx < 52 && dx > -52 && dy < 44 && dy > -44) hurt(2 + sv.zone, dx > 0 ? 2 : -2, dy > 0 ? 2 : -2); }   /* she stamps */
        }
        sp = m->kind == K_WASP ? 3 : m->kind == K_SLIME ? 0 : m->kind == K_QUEEN ? 1 : 1;   /* steps per 2 frames */
        if(m->kind == K_SLIME) sp = (step & 1); else if(m->kind == K_QUEEN) sp = (step & 1) ? 1 : 0; else if(m->kind == K_BEETLE) sp = (step & 3) ? 1 : 0; else if(m->kind == K_SPIDER) sp = (step & 1) ? 2 : 1;
        for(n = 0; n < sp; n++){
            if(dx > 3 && !blocked(m->x + 1, m->y, r)) m->x++; else if(dx < -3 && !blocked(m->x - 1, m->y, r)) m->x--;
            if(dy > 3 && !blocked(m->x, m->y + 1, r)) m->y++; else if(dy < -3 && !blocked(m->x, m->y - 1, r)) m->y--;
        }
        if(!(step & 7)) m->frame ^= 1;
contact:
        { INT16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, reach = m->kind == K_QUEEN ? 22 : 11;
          if(ax < reach && ay < reach - 2 && !inv){ UINT8 dmg = 1 + depth / 2 + sv.zone + (m->kind == K_BEETLE) + (m->kind == K_QUEEN) * (2 + sv.zone);
              hurt(dmg, dx > 0 ? 2 : dx < 0 ? -2 : 0, dy > 0 ? 2 : dy < 0 ? -2 : 0); } }
    }
}


/* ------------------------------------------------------------ pets, and the city pit */
static void pets_fight(void){                                      /* your friends chip at monsters near them (they never finish one off: the kill is yours) */
    UINT8 i, j; UINT16 px, py;
    if(step & 31) return;
    for(i = 0; i < 3; i++){
        if(!pet_get(i, &px, &py)) break;
        for(j = 0; j < NMON; j++){
            Mon *m = &mon[j]; INT16 dx, dy;
            if(!m->alive || m->hp < 2) continue;
            dx = (INT16)m->x - (INT16)px; dy = (INT16)m->y - (INT16)py; if(dx < 0) dx = -dx; if(dy < 0) dy = -dy;
            if(dx < 24 && dy < 20){ UINT8 p = 1 + sv.level / 6; if(p >= m->hp) p = m->hp - 1; m->hp -= p; m->flash = 6; fx_add(m->x, m->y - 18, p, 2); break; }
        }
    }
}
static UINT8 pit_tick(void){                                        /* the next wave when the last one is down; DG_WIN-worthy after the last */
    UINT8 i, n = 0;
    for(i = 0; i < NMON; i++) if(mon[i].alive) n++;
    if(n) return 0;
    if(pit_wave >= 10) return 1;
    pit_wave++;
    n = 2 + pit_wave / 2; if(n > 8) n = 8;
    for(i = 0; i < n; i++){
        UINT8 k = rndn(1 + pit_wave / 3 > 4 ? 4 : 1 + pit_wave / 3), x = (i & 1) ? 22 : 9, y = 11 + (i >> 1) * 2;
        UINT16 hp = (k == K_BEETLE ? 8 : k == K_WASP ? 3 : k == K_SLIME ? 5 : 4) + pit_wave + pit_tier + sv.level / 2;
        mon_spawn(k, ((UINT16)x << 4) + 8, ((UINT16)y << 4) + 12, hp > 250 ? 250 : (UINT8)hp, 0, ((UINT16)16 << 4) + 8, ((UINT16)16 << 4));
    }
    sfx(SFX_HISS); shake(6); return 0;
}
/* ------------------------------------------------------------ the floors */
static int play_floor(void){                                       /* returns DG_* when this floor is left */
    UINT8 k, ox, oy, was_on = 1;
    { UINT8 i; for(i = 0; i < NFX; i++) fxs[i].t = 0; }
    fade_out(); DISPLAY_OFF; make_floor(); enter_gfx(); hud_init(); music(depth == floors || pit ? MUS_BOSS : MUS_DUNGEON_K(kind)); bugspr_load(&sv.look); SHOW_BKG; SHOW_SPRITES;
    show_bug(); fade_in();
    if(depth == floors && !pit) banner(BN_QUEEN);
    if(depth > sv.deepest) sv.deepest = depth;
#ifdef TEST_BOSS
    { Room *r = &rooms[nrooms - 1]; bx = ((UINT16)(r->x + r->w / 2) << 4) + 8; by = ((UINT16)(r->y + r->h - 1) << 4) + 12; }
#endif
    if(!pit){ char b[48], *d = scat(b, "FLOOR B"); d = u16s(d, depth); d = scat(d, " OF "); d = u16s(d, floors);
      if(depth == floors) scat(d, ". SOMETHING BIG IS BREATHING DOWN HERE."); dlg_say(0, b); }
    if(!pit) floor_intro(evt);
    pet_reset(bx, by);
    for(;;){
        input_poll(); k = keys; step++;
        { INT8 vx = (k & J_LEFT) ? -1 : (k & J_RIGHT) ? 1 : 0, vy = (k & J_UP) ? -1 : (k & J_DOWN) ? 1 : 0; UINT8 sp = (k & J_B) ? (TRICK(1) ? 3 : 2) : 1, n;
          if(vx > 0) face = 0; else if(vx < 0) face = 1; else if(vy < 0) face = 2; else if(vy > 0) face = 3;
          for(n = 0; n < sp; n++){ if(vx && !blocked(bx + vx, by, 5)) bx += vx; if(vy && !blocked(bx, by + vy, 5)) by += vy; }
          if((vx || vy) && !(step & 7)) frame ^= 1; }
        if(HIT(J_A) && !atk){ atk = 12; sfx(SFX_BITE); do_bite(); }
        if(atk) atk--;
        if(inv) inv--;
        if(!(step & 127) && sv.mp < hud_mpmax(sv.level)) sv.mp++;
        mon_step(); pets_fight(); if(TRICK(4) && !step && sv.hp && sv.hp < hud_hpmax(sv.level)) sv.hp++;
        if(pit && pit_tick()) return DG_WIN;
        camera(); scroll_fill();
        ox = (UINT8)(bx >> 4); oy = (UINT8)((by - 2) >> 4);
        if(DM(ox, oy) == D_POTION){ set_mt(ox, oy, (rnd8() & 1) ? D_FLOOR : D_FLOOR2); if(sv.potions < 9){ sv.potions++; sfx(SFX_PICK); dlg_say(0, "A HEALTH POTION! B+SELECT DRINKS IT."); } }
        if(DM(ox, oy) == D_DOWN){ if(!was_on){ sfx(SFX_STAIRS); return DG_DOWN; } } else if(DM(ox, oy) == D_UP){ if(!was_on){ sfx(SFX_STAIRS); return DG_EXIT; } } else was_on = 0;
        if(HIT(J_SELECT) && (k & J_A)){ EX_SPELL = (EX_SPELL & 0x80) | spell_next(EX_SPELL & 3); { char b[24]; b[0] = 0; spell_label(EX_SPELL & 3, b); dlg_say(0, b); } }
        else if(HIT(J_SELECT) && !(k & J_B)) cast();
        else if(HIT(J_SELECT)){
            if(sv.potions && sv.hp < hud_hpmax(sv.level)){ UINT16 mx = hud_hpmax(sv.level); sv.potions--; sfx(SFX_SPARK); { UINT16 h = mx / 2 + mx / 20 * UPG(SK_VIGOR) + (TRICK(0) ? mx / 4 : 0); sv.hp = sv.hp + h > mx ? mx : sv.hp + h; } dlg_say(0, "GLUG GLUG. YOU FEEL BETTER."); }
            else if(!sv.potions) dlg_say(0, "NO POTIONS LEFT.");
        }
        wait_vbl_done();
        shake_tick(); SCX_REG = (UINT8)cam_x + shake_x; SCY_REG = (UINT8)cam_y + shake_y;
        show_bug(); show_mons(); show_bite(); pet_push(bx, by); { UINT8 po[3]; po[0] = 2; po[1] = 3; po[2] = 15; pet_draw(po, cam_x, cam_y, 0); }
        hud_update();
        while(stop){ stop--; wait_vbl_done(); }
#ifdef TEST_FAINT
        { static UINT16 tf; if(++tf == 400) sv.hp = 0; }
#endif
        if(!sv.hp) return DG_FAINT;
        if(depth == floors){ UINT8 q; for(q = 0; q < NMON; q++) if(mon[q].alive && mon[q].kind == K_QUEEN) break;
            if(q == NMON && mon[NMON - 1].kind == K_QUEEN && ground_at(bx, by) != D_CAGE){
                UINT8 cx, cy, f = 0; for(cy = 0; cy < DW && !f; cy++) for(cx = 0; cx < DW; cx++) if(DM(cx, cy) == D_FREED){ f = 1; break; }
                if(f) return DG_WIN; } }
        if(HIT(J_START)){
            if(sysmenu_run(redraw) == SYS_TITLE) return DG_TITLE;
            bugspr_load(&sv.look);
        }
    }
}
int dungeon_run(void) BANKED {
    int r;
    kind = world_kind(world_seed(sv.star));
    floors = 3 + (sv.zone > 3 ? 3 : sv.zone);
    depth = 1; inv = 0; atk = 0; face = 3;
#ifdef TEST_LAB
    depth = 2;
#endif
#ifdef TEST_BOSS
    depth = floors; ex.tricks = 63; sv.friends = 3;              /* (test builds: straight to the queen) */
#endif
    for(;;){
        r = play_floor();
        if(r == DG_DOWN){ depth++; continue; }
        break;
    }
    { UINT8 i; for(i = 0; i < 40; i++) move_sprite(i, 0, 0); }
    if(r == DG_FAINT){ if(opt.diff) sv.gold -= sv.gold / 4; sv.hp = hud_hpmax(sv.level) / 2; if(!sv.hp) sv.hp = 1; dlg_say("BOLT", "YOU FAINTED. I DRAGGED YOU BACK TO THE SHIP. SOME GOLD FELL OUT OF YOUR POCKETS."); }
    else if(r == DG_WIN) dungeon_won(kind);
    else if(r == DG_EXIT) dlg_say(0, "YOU CLIMB BACK OUT INTO THE DAYLIGHT.");
    feats_check();
    return r;
}
int dungeon_hell(UINT8 moon) BANKED {
    int r;
    kind = world_kind(world_seed(sv.star)); if(moon) kind = (kind + 3) % 6; floors = 99; depth = 1; inv = 0; atk = 0; face = 3; hell = 1 + moon;
    for(;;){ hell_set(depth, moon); r = play_floor(); if(r == DG_DOWN){ depth++; continue; } break; }
    hell = 0; { UINT8 i; for(i = 0; i < 40; i++) move_sprite(i, 0, 0); }
    if(r == DG_EXIT || r == DG_FAINT) hell_end(r, depth, moon); else hell_t = 0;
    return r;
}
int dungeon_pit(UINT8 tier) BANKED {                                 /* the city pit: returns (waves cleared << 4) | how it ended (DG_WIN: all ten, DG_EXIT: gave up, DG_FAINT) */
    int r; UINT8 w;
    kind = world_kind(world_seed(sv.star));
    floors = 2; depth = 1; inv = 0; atk = 0; face = 3; pit = 1; pit_wave = 0; pit_tier = tier;
    r = play_floor();
    pit = 0;
    { UINT8 i; for(i = 0; i < 40; i++) move_sprite(i, 0, 0); }
    w = r == DG_WIN ? pit_wave : (pit_wave ? pit_wave - 1 : 0);
    if(r == DG_FAINT){ sv.hp = hud_hpmax(sv.level) / 2; if(!sv.hp) sv.hp = 1; }
    return (int)((w << 4) | (UINT8)r);
}
