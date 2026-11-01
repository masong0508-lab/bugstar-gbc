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
/* ------------------------------------------------------------ stairs both ways, shortcuts, remembered floors */
UINT8 dg_arrive;
static UINT8 dg_seen;                                          /* a bit per floor left by stairs this trip */
static UINT8 dg_chest[8], dg_pot[8];                           /* per floor: a bit per room that has an opened chest, a potion still lying there */
static UINT16 dg_dead[8];                                      /* per floor: a bit per monster slot that was not alive when you left */
static UINT8 su_x = 255, su_y, sh_x, sh_y;                     /* the shortcut stair up and down on this floor (255: none) */
void dg_trip_reset(void) BANKED { UINT8 i; dg_seen = 0; dg_arrive = 0; for(i = 0; i < 8; i++){ dg_chest[i] = dg_pot[i] = 0; dg_dead[i] = 0; } }
static UINT8 has_short(UINT8 d){                               /* floor d has a shortcut stair down to floor d + 2 (B2 on, about 1 floor in 3; the same every visit) */
    UINT16 h;
    if(hell || pit || d < 2 || d + 2 > floors) return 0;
#ifdef TEST_SHORT
    return 1;
#endif
    h = (UINT16)world_seed(sv.star) ^ ((UINT16)d * 0x5A3Du) ^ ((UINT16)sv.zone << 9) ^ 0x71C3u; h ^= h >> 7; h *= 0x2D3Bu; h ^= h >> 5;
    return (h % 3) == 0;
}
static UINT8 in_room(const Room *r, UINT8 t){                   /* is there a tile of this kind inside the room? */
    UINT8 x, y;
    for(y = r->y; y < r->y + r->h; y++) for(x = r->x; x < r->x + r->w; x++) if(DM(x, y) == t) return 1;
    return 0;
}
static void swap_in_room(const Room *r, UINT8 from, UINT8 to){
    UINT8 x, y;
    for(y = r->y; y < r->y + r->h; y++) for(x = r->x; x < r->x + r->w; x++) if(DM(x, y) == from) DM(x, y) = to;
}
void dg_floor_left(void) BANKED {
    UINT8 i, c = 0, p = 0; UINT16 m = 0;
    if(hell || pit || depth >= 8) return;
    for(i = 0; i < nrooms; i++){ if(in_room(&rooms[i], D_OPEN)) c |= 1 << i; if(in_room(&rooms[i], D_POTION)) p |= 1 << i; }
    for(i = 0; i < NMON; i++) if(!mon[i].alive) m |= 1u << i;
    dg_seen |= 1 << depth; dg_chest[depth] = c; dg_pot[depth] = p; dg_dead[depth] = m;
}
static void floor_memory(void){                                  /* a floor you have been to is as you left it (monsters that were alive are back where they started) */
    UINT8 i, d = depth;
    if(hell || pit || d >= 8 || !((dg_seen >> d) & 1)) return;
    for(i = 0; i < nrooms; i++){
        if((dg_chest[d] >> i) & 1) swap_in_room(&rooms[i], D_CHEST, D_OPEN);
        if(!((dg_pot[d] >> i) & 1)) swap_in_room(&rooms[i], D_POTION, D_FLOOR);
    }
    for(i = 0; i < NMON; i++) if((dg_dead[d] >> i) & 1) mon[i].alive = 0;
}
static void stairs_extra(void){                                  /* the shortcut stairs, and where you step out */
    UINT8 x, y;
    su_x = sh_x = 255;
    if(!hell && !pit && nrooms >= 3){
        if(has_short(depth)){ Room *r = &rooms[nrooms - 2]; x = r->x + r->w / 2; y = r->y + r->h / 2; if(DM(x, y) == D_FLOOR || DM(x, y) == D_FLOOR2){ DM(x, y) = D_SHORT; sh_x = x; sh_y = y; } }
        if(depth > 3 && has_short(depth - 2)){ Room *r = &rooms[1]; x = r->x + 1; y = r->y + 1; if(DM(x, y) == D_FLOOR || DM(x, y) == D_FLOOR2){ DM(x, y) = D_SHUP; su_x = x; su_y = y; } }
    }
}
static void arrive(void){
    UINT8 x = 255, y = 0;
    if(hell || pit) return;
    if(dg_arrive == 1 && dn_x != 255){ x = dn_x; y = dn_y; }
    else if(dg_arrive == 2 && su_x != 255){ x = su_x; y = su_y; }
    else if(dg_arrive == 3 && sh_x != 255){ x = sh_x; y = sh_y; }
#ifdef TEST_STAIRS
    if(dg_arrive == 0){ if(sh_x != 255){ bx = ((UINT16)sh_x << 4) + 8; by = ((UINT16)(sh_y - 2) << 4) + 14; } else if(dn_x != 255){ bx = ((UINT16)dn_x << 4) + 8; by = ((UINT16)(dn_y - 2) << 4) + 14; } }
#endif
    if(x != 255){ bx = ((UINT16)x << 4) + 8; by = ((UINT16)(y + 1) << 4) + 14; }       /* just below the stair you came by */
}
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
    stairs_extra();
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
    floor_memory(); arrive();
}

