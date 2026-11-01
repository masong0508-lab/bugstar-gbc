#pragma bank 37
/* BUGSTAR GBC - the dungeon's floor maker, split out of dungeon.c so bank 5 has room (it was over by 591 bytes).
   Same code as before; it shares the dungeon's state through dungeon_int.h. */
#include <gb/gb.h>
#include <string.h>
#include "wram.h"
#include "save.h"
#include "spell.h"
#include "lab.h"
#include "wild.h"
#include "dungeon_int.h"

static UINT8 rnd8(void){ rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; return (UINT8)rng; }
static UINT8 rndn(UINT8 n){ return n ? (UINT8)(((UINT16)rnd8() * n) >> 8) : 0; }
/* ------------------------------------------------------------ making a floor */
static void carve(UINT8 x, UINT8 y){ if(x > 0 && y > 0 && x < DW - 1 && y < DW - 1 && (DM(x, y) == D_WALL)) DM(x, y) = (rnd8() & 3) ? D_FLOOR : D_FLOOR2; }
void dg_make_floor(void) BANKED {
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
    dn_x = 255;
    evt = (pit || depth == floors) ? 0 : lab ? 7 : ev_roll(depth);
    if(pit){ memset(mon, 0, sizeof mon); return; }
    { Room *r = &rooms[nrooms - 1];
      if(depth < floors){ dn_x = r->x + r->w / 2; dn_y = r->y + r->h / 2; DM(dn_x, dn_y) = D_DOWN; }
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
        m->hp = bst_hp(m->hp, k, kind);
        i++;
        if(i >= (UINT8)(4 + depth * 2 + (evt == 3 ? 3 : 0) - (evt == 6 ? 3 : 0))) break;
    }
    if(depth == floors && nrooms > 1){                             /* the queen waits by the cage */
        Room *r = &rooms[nrooms - 1]; Mon *m = &mon[NMON - 1];
        memset(m, 0, sizeof *m); m->kind = K_QUEEN; m->alive = 1; m->x = ((UINT16)(r->x + r->w / 2) << 4) + 8; m->y = ((UINT16)(r->y + r->h / 2 + 1) << 4) + 8;
        m->hp = sv.zone >= 8 ? 250 : 40 + sv.zone * 25;
    }
}

