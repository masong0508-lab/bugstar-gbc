#pragma bank 15
/* BUGSTAR GBC - the Elite layer's numbers (see econ.h). */
#include <gb/gb.h>
#include "save.h"
#include "econ.h"
#include "menus.h"

static const char *const good_name[G_N] = { "FOOD", "ORE", "CRYSTAL", "SILK", "AMBER", "MEDS", "TECH", "NECTAR" };
static const UINT8 base[G_N] = { 12, 18, 34, 28, 24, 46, 70, 55 };
/* percent of the middle price, by the kind of world (GARDEN DESERT ICE JUNGLE OCEAN DUSK): cheap where it is made, dear where it is missed */
static const UINT8 mod[6][G_N] = {
    /* FOOD ORE CRY SILK AMB MEDS TECH NEC */
    {   60, 120, 130, 100,  90, 110, 140,  70 },     /* GARDEN: farms */
    {  150,  60, 100, 130,  70, 120, 110, 140 },     /* DESERT: mines */
    {  130,  90,  60, 140, 120,  90, 100, 125 },     /* ICE: crystal caves */
    {   80, 140, 120,  60,  65,  70, 150,  90 },     /* JUNGLE: silk and amber */
    {   70, 130,  90, 120, 130,  60, 125,  80 },     /* OCEAN: healers and nectar */
    {  140, 100,  75,  90, 140, 130,  60, 150 },     /* DUSK: factories */
};
void good_label(UINT8 g, char *out) BANKED { scat(out, good_name[g]); }
UINT16 econ_base(UINT8 g) BANKED { return base[g]; }
static UINT8 hash8(UINT8 star, UINT8 g, UINT8 epoch){
    UINT16 h = (UINT16)star * 53 + (UINT16)g * 29 + (UINT16)epoch * 91 + 7;
    h ^= h >> 5; h *= 37; h ^= h >> 7;
    return (UINT8)h;
}
UINT16 econ_price(UINT8 star, UINT8 kind, UINT8 g) BANKED {
    UINT32 p = (UINT32)base[g] * mod[kind > 5 ? 0 : kind][g] / 100;
    p = p * (85 + hash8(star, g, 0) % 31) / 100;                     /* this star: 85..115% */
    p = p * (90 + hash8(star, g, 1 + sv.el.jumps / 3) % 21) / 100;   /* this season: 90..110%, changes every 3 jumps */
    return p ? (UINT16)p : 1;
}
UINT16 econ_sell(UINT8 star, UINT8 kind, UINT8 g) BANKED { UINT16 p = econ_price(star, kind, g) * 88 / 100; return p ? p : 1; }
UINT8 econ_load(void) BANKED { UINT8 i, n = 0; for(i = 0; i < G_N; i++) n += sv.el.cargo[i]; return n; }
UINT8 econ_buy(UINT8 star, UINT8 kind, UINT8 g, UINT8 n) BANKED {
    UINT16 p = econ_price(star, kind, g); UINT8 room = HOLD_CAP() > econ_load() ? HOLD_CAP() - econ_load() : 0, afford;
    afford = (UINT8)(sv.gold / p > 99 ? 99 : sv.gold / p);
    if(n > room) n = room;
    if(n > afford) n = afford;
    sv.el.cargo[g] += n; sv.gold -= (UINT32)p * n;
    return n;
}
UINT8 econ_sell_n(UINT8 star, UINT8 kind, UINT8 g, UINT8 n) BANKED {
    UINT16 p = econ_sell(star, kind, g); UINT32 old = sv.el.sold;
    if(n > sv.el.cargo[g]) n = sv.el.cargo[g];
    sv.el.cargo[g] -= n; sv.gold += (UINT32)p * n; sv.el.sold += (UINT32)p * n;
    if(sv.el.sold / 100 > old / 100) fame_add((UINT16)(sv.el.sold / 100 - old / 100));
    return n;
}

/* ---------------------------------------------------------------- fame */
static const char *const rank_name[NRANK] = { "HARMLESS", "MOSTLY HARMLESS", "POOR", "AVERAGE", "ABOVE AVERAGE", "COMPETENT", "DANGEROUS", "DEADLY", "ELITE" };
static const UINT16 rank_at[NRANK] = { 0, 10, 30, 70, 150, 300, 600, 1200, 2500 };
void rank_label(UINT8 r, char *out) BANKED { scat(out, rank_name[r]); }
UINT8 fame_rank(void) BANKED { UINT8 r = 0; while(r < NRANK - 1 && sv.el.fame >= rank_at[r + 1]) r++; return r; }
UINT8 fame_disc(void) BANKED { return fame_rank() * 2; }
void fame_add(UINT16 n) BANKED { UINT16 f = sv.el.fame; sv.el.fame = n > (UINT16)(65535U - f) ? 65535U : (UINT16)(f + n); }
UINT8 elite_kill(UINT8 kind) BANKED {
    if(sv.el.kills < 65535) sv.el.kills++;
    fame_add(kind == 2 ? 2 : 1);
    if(sv.el.jk == J_HUNT && sv.el.jp < sv.el.jn){ sv.el.jp++; return sv.el.jp == sv.el.jn; }
    return 0;
}

/* ---------------------------------------------------------------- jobs */
UINT8 cells_between(UINT8 a, UINT8 b) BANKED {
    UINT8 dx = (a & 15) > (b & 15) ? (a & 15) - (b & 15) : (b & 15) - (a & 15), dy = (a >> 4) > (b >> 4) ? (a >> 4) - (b >> 4) : (b >> 4) - (a >> 4);
    if(dx > 8) dx = 16 - dx;
    if(dy > 8) dy = 16 - dy;
    return dx > dy ? dx : dy;
}
UINT8 jump_fuel(UINT8 a, UINT8 b) BANKED { UINT8 d = cells_between(a, b); return d ? d : 1; }
void job_offer(UINT8 star, UINT8 *kind, UINT8 *n, UINT8 *dest, UINT16 *reward) BANKED {
    UINT16 h = (UINT16)star * 31 + (UINT16)sv.el.jobs * 17 + 5; UINT8 z = sv.zone;
    h ^= h >> 4; h *= 25; h ^= h >> 6;
    if(h & 1){
        *kind = J_HUNT; *n = 3 + (UINT8)((h >> 1) % 4) + z; *dest = 0; *reward = (UINT16)*n * (20 + z * 6);
    } else {
        UINT8 cx = (star & 15) + (UINT8)((h >> 2) % 9) + 12, cy = (star >> 4) + (UINT8)((h >> 5) % 9) + 12, d;
        d = (UINT8)(((cy & 15) << 4) | (cx & 15));
        if(d == star) d ^= 1;
        *kind = J_SEND; *n = 0; *dest = d; *reward = 50 + (UINT16)cells_between(star, d) * 35 + (UINT16)z * 25;
    }
}
void job_take(UINT8 kind, UINT8 n, UINT8 dest, UINT16 reward) BANKED {
    sv.el.jk = kind; sv.el.jn = n; sv.el.jp = 0; sv.el.jd = dest; sv.el.jrew = reward;
}
UINT8 job_done(UINT8 here) BANKED {
    if(sv.el.jk == J_HUNT) return sv.el.jp >= sv.el.jn;
    if(sv.el.jk == J_SEND) return here == sv.el.jd;
    return 0;
}
UINT16 job_pay(void) BANKED {
    UINT16 g = sv.el.jrew + (UINT16)((UINT32)sv.el.jrew * fame_rank() * 3 / 100);
    sv.gold += g; sv.el.jobs++; fame_add(sv.el.jk == J_HUNT ? 6 : 8);
    sv.el.jk = 0; sv.el.jn = sv.el.jp = 0; sv.el.jrew = 0;
    return g;
}
