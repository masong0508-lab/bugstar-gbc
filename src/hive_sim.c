#pragma bank 47
/* BUGSTAR GBC - BUG HIVE, the rules (hive_int.h). The colony state lives in WRAM bank 6 (HV, MAP). Time: a tick is 6 seconds, 20 ticks a day, 6 days a season.
   Workers walk the tunnels (a breadth-first search to the nearest place that suits their job) and do real work; while the player is away the same colony is
   run by formula (hs_away). */
#include <gb/gb.h>
#include <string.h>
#include "save.h"
#include "sound.h"
#include "menus.h"
#include "traits.h"
#include "hive_int.h"

enum { GC_ENTRY, GC_STORE, GC_NURS, GC_BARR, GC_QUEEN, GC_STAT, GC_MARK, GC_SITE };
static const UINT8 prof[12][5] = {            /* what each kind of world gives: food leaf ore crystal honey (0..4) */
    { 3, 3, 1, 0, 3 }, { 1, 0, 3, 1, 1 }, { 1, 0, 2, 3, 0 }, { 3, 4, 0, 0, 2 }, { 3, 1, 0, 2, 1 }, { 1, 1, 2, 2, 2 },
    { 1, 1, 4, 1, 0 }, { 3, 2, 1, 1, 1 }, { 0, 0, 1, 4, 0 }, { 0, 0, 4, 3, 0 }, { 2, 3, 1, 0, 1 }, { 2, 1, 0, 1, 4 } };
static const UINT8 bc[NBLD][4] = { { 12, 0, 0, 0 }, { 10, 2, 0, 0 }, { 14, 4, 0, 0 }, { 10, 10, 0, 0 }, { 12, 12, 2, 0 }, { 10, 16, 0, 0 },
                                   { 6, 8, 0, 0 }, { 8, 6, 4, 0 }, { 12, 10, 6, 1 }, { 10, 6, 0, 3 }, { 16, 6, 2, 1 } };
static const UINT8 dxs[4] = { 0, 1, 0, 255 };
static const UINT8 dys[4] = { 255, 0, 1, 0 };

UINT8 hs_rnd(void) BANKED {
    UINT16 r = HV->rng;
    r ^= r << 7; r ^= r >> 9; r ^= r << 8; if(!r) r = 0xACE1;
    HV->rng = r;
    return (UINT8)(r >> 3) ^ (UINT8)r;
}
UINT8 hs_prof_at(UINT8 kind, UINT8 r) BANKED { return prof[kind % 12][r > 4 ? 4 : r]; }
void hs_note(UINT8 id) BANKED {
    HV->note = id; HV->notet = 150;
    if(HV->away) return;
    switch(id){
        case M_HATCH: sfx(SFX_HATCH); break;
        case M_BUILT: case M_UPGR: sfx(SFX_BUILT); break;
        case M_CARAVAN: case M_TRADERS: sfx(SFX_BELL); break;
        case M_RAID: case M_BOSS: case M_TWOFOLD: sfx(SFX_HORN); break;
        case M_QUESTDONE: case M_ACH: case M_ROYAL: sfx(SFX_QUEST); break;
        case M_TECHDONE: case M_WON: case M_OPWON: sfx(SFX_POWER); break;
        case M_WARN: case M_LOST: case M_OPLOST: case M_STARVE: sfx(SFX_ALARM); break;
        default: break;
    }
}
void hs_dirty(UINT16 idx) BANKED {
    UINT8 n = HV->stat[S_DQN];
    if(n >= DQN){ HV->stat[S_FULL] = 1; return; }
    DQ[n] = idx; HV->stat[S_DQN] = n + 1;
}
static UINT8 season(void){ return (UINT8)((HV->day / 6) & 3); }
UINT16 hs_stock(void) BANKED { UINT16 s = 60 + 80 * (UINT16)HV->bsum[C_STORE]; return s > 999 ? 999 : s; }
UINT8 hs_popcap(void) BANKED { UINT16 c = 4 + 3 * (UINT16)HV->bsum[C_NURS] + 2 * HV->royal; return c > NWK ? NWK : (UINT8)c; }
UINT8 hs_guardcap(void) BANKED { UINT16 c = 2 + 2 * (UINT16)HV->bsum[C_BARR]; return c > 12 ? 12 : (UINT8)c; }
UINT8 hs_nidle(void) BANKED { UINT8 i, n = 0; for(i = 0; i < HV->pop; i++) if(HV->wk[i].job == J_IDLE) n++; return n; }
static UINT8 has_tech(UINT8 t){ return (HV->tech >> t) & 1; }
static void add_res(UINT8 r, UINT16 v){ UINT32 n = (UINT32)HV->res[r] + v; UINT16 c = hs_stock(); HV->res[r] = n > c ? c : (UINT16)n; }
static UINT8 passable(UINT8 c){ c = CT(c); return c == C_TUNNEL || c == C_ENTRY || c == C_QUEEN || c == C_SITE || (c >= C_FIRSTB && c != C_WALL); }

void hs_recount(void) BANKED {
    UINT16 i; UINT8 c;
    memset(HV->bsum, 0, sizeof HV->bsum); memset(HV->bcnt, 0, sizeof HV->bcnt);
    for(i = 0; i < MW * MH; i++){
        c = MAP[i]; if(CT(c) >= C_QUEEN){ UINT8 t = CT(c); HV->bcnt[t]++; HV->bsum[t] += (t == C_SITE ? 1 : CLV(c) + 1); }
    }
}
/* ------------------------------------------------------------------ the map */
void hs_new(UINT8 kind, UINT8 diff) BANKED {
    UINT8 x, y, r; UINT16 i; UINT8 od, cd;
    memset(HV, 0, sizeof(HS));
    HV->magic = 0x32564842UL; HV->seed = sv.seed; HV->ver = HV_VER; HV->kind = kind; HV->diff = diff;
    HV->rng = (UINT16)(sv.seed ^ (sv.seed >> 16)) | 1; HV->rng ^= (UINT16)kind * 977;
    HV->res[R_FOOD] = 45; HV->res[R_LEAF] = 40; HV->res[R_ORE] = 12; HV->shells = 10; HV->pop = 4; HV->hap = 60; HV->rs = 255;
    HV->quota[0] = 2; HV->quota[1] = 1; HV->quota[2] = 0; HV->quota[3] = 0; HV->quota[4] = 1;
    HV->cvn = 4; HV->orday = 12; HV->day = 0; HV->tod = 4; HV->wx = W_CLEAR; HV->rday = diff == 0 ? 9 : diff == 1 ? 7 : 5;
    od = 8 + 5 * prof[kind][2]; cd = 1 + 2 * prof[kind][3];            /* vein chance in 256ths: the world's kind makes the ground */
    for(y = 0; y < MH; y++) for(x = 0; x < MW; x++){
        UINT8 c;
        if(y < 2) c = C_SKY;
        else if(y == 2){ c = C_GRASS; r = hs_rnd(); if(r < 40) c = C_FLOWER; }
        else if(x == 0 || x == MW - 1 || y == MH - 1) c = C_ROCK;
        else {
            r = hs_rnd(); c = C_SOIL;
            if(y > 4 && r < od + (y >> 2)) c = C_ORE;
            else if(y > 12 && r > 255 - cd - (y >> 3)) c = C_CRYS;
            else if(r > 244 && y > 6) c = C_ROCK;
            else if(r > 240 && y > 9) c = C_WATER;
        }
        MAP[CI(x, y)] = c;
    }
    for(y = 5; y < MH - 2; y++) for(x = 1; x < MW - 2; x++) if(MAP[CI(x, y)] == C_ORE){        /* veins: ore likes company */
        if(MAP[CI(x + 1, y)] == C_SOIL && (hs_rnd() & 1)) MAP[CI(x + 1, y)] = C_ORE;
        if(MAP[CI(x, y + 1)] == C_SOIL && !(hs_rnd() & 3)) MAP[CI(x, y + 1)] = C_ORE;
    }
    MAP[CI(ENTX, ENTY)] = C_ENTRY;
    for(y = 3; y <= 7; y++) MAP[CI(ENTX, y)] = C_TUNNEL;
    for(x = 13; x <= 19; x++) MAP[CI(x, 7)] = C_TUNNEL;
    MAP[CI(ENTX, 8)] = C_TUNNEL; MAP[CI(ENTX, 9)] = C_TUNNEL;
    MAP[CI(ENTX, 7)] = C_QUEEN;
    for(y = 8; y < 11; y++) for(x = 12; x < 21; x++) if(MAP[CI(x, y)] == C_WATER || MAP[CI(x, y)] == C_ROCK) MAP[CI(x, y)] = C_SOIL;
    for(x = 13; x <= 19; x++){ MAP[CI(x, 6)] = MAP[CI(x, 6)] == C_TUNNEL ? C_TUNNEL : C_SOIL; }
    for(i = 0; i < 4; i++){ HV->wk[i].x = ENTX - 1 + (UINT8)i; HV->wk[i].y = 7; HV->wk[i].job = J_IDLE; }
    hs_recount(); hs_assign(); HV->last = play_secs();
    hs_quest_new(0); hs_quest_new(1); hs_quest_new(2);
}
/* ------------------------------------------------------------------ search */
static UINT8 goal(UINT16 i, UINT8 gc){
    UINT8 c = CT(MAP[i]), x = (UINT8)(i & 31), y = (UINT8)(i >> 5), d, m;
    switch(gc){
        case GC_ENTRY: return c == C_ENTRY;
        case GC_STORE: return c == C_STORE;
        case GC_NURS:  return c == C_NURS;
        case GC_BARR:  return c == C_BARR;
        case GC_QUEEN: return c == C_QUEEN;
        case GC_STAT:  return c == C_FARM || c == C_SMELT || c == C_WORK || c == C_SHRINE || c == C_MARKET;
        default:
            for(d = 0; d < 4; d++){
                if((d == 0 && y == 0) || (d == 1 && x == MW - 1) || (d == 2 && y == MH - 1) || (d == 3 && x == 0)) continue;
                m = MAP[i + (d == 0 ? -32 : d == 2 ? 32 : d == 1 ? 1 : -1)];
                if(gc == GC_MARK ? ((m & MARKD) && CT(m) <= C_CRYS && CT(m) != C_ROCK && CT(m) != C_TUNNEL) : CT(m) == C_SITE) return 1;
            }
            return 0;
    }
}
/* 0..3 the first step to the nearest place for gc, 4 already there, 255 nowhere */
static UINT8 bfs(UINT8 sx, UINT8 sy, UINT8 gc){
    UINT16 s = CI(sx, sy), cur, nb, nexp = 0; UINT8 head = 0, tail = 0, d, fd, gen, x, y;
    if(goal(s, gc)) return 4;
    gen = HV->stat[S_VGEN] + 1; if(gen > 63){ memset(VIS, 0, 1024); gen = 1; } HV->stat[S_VGEN] = gen;
    VIS[s] = gen << 2; QUE[tail++ & 127] = s;
    while(head != tail){
        if(++nexp > 420) break;
        cur = QUE[head++ & 127]; fd = VIS[cur] & 3; x = (UINT8)(cur & 31); y = (UINT8)(cur >> 5);
        for(d = 0; d < 4; d++){
            if((d == 0 && y == 0) || (d == 1 && x == MW - 1) || (d == 2 && y == MH - 1) || (d == 3 && x == 0)) continue;
            nb = cur + (d == 0 ? -32 : d == 2 ? 32 : d == 1 ? 1 : -1);
            if((VIS[nb] >> 2) == gen || !passable(MAP[nb])) continue;
            VIS[nb] = (gen << 2) | (cur == s ? d : fd);
            if(goal(nb, gc)) return VIS[nb] & 3;
            if((UINT8)(tail - head) < 126) QUE[tail++ & 127] = nb;
        }
    }
    return 255;
}
UINT8 hs_connected(void) BANKED { return bfs(ENTX, ENTY, GC_QUEEN) != 255; }
/* ------------------------------------------------------------------ jobs */
void hs_assign(void) BANKED {
    UINT8 i, j, n = 0, left = HV->pop, want[6], k = 0, cnt = 0, job;
    static const UINT8 order[5] = { 0, 1, 2, 3, 4 };               /* quota index: forage build nurse guard mine */
    static const UINT8 jobof[5] = { J_FORAGE, J_BUILD, J_NURSE, J_GUARD, J_MINE };
    (void)n;
    for(j = 0; j < 6; j++) want[j] = 0;
    for(j = 0; j < 5; j++){
        UINT8 q = HV->quota[order[j]];
        if(order[j] == 3 && q > hs_guardcap()) q = hs_guardcap();
        if(order[j] == 2 && !HV->bcnt[C_NURS]) q = 0;
        if(q > left) q = left;
        want[jobof[j]] = q; left -= q;
    }
    want[J_IDLE] = left;
    for(i = 0; i < HV->pop && i < NWK; i++){
        while(k < 6 && cnt >= want[k == 0 ? J_FORAGE : k == 1 ? J_BUILD : k == 2 ? J_NURSE : k == 3 ? J_GUARD : k == 4 ? J_MINE : J_IDLE]){ k++; cnt = 0; }
        job = k == 0 ? J_FORAGE : k == 1 ? J_BUILD : k == 2 ? J_NURSE : k == 3 ? J_GUARD : k == 4 ? J_MINE : J_IDLE;
        cnt++;
        if(HV->wk[i].job != job){ Wk *w = &HV->wk[i]; w->job = job; if(w->st != 2){ w->st = 0; w->tm = 0; } w->carry = 0; }
    }
}
static void spawn(void){
    Wk *w; UINT8 i = HV->pop;
    if(i >= NWK) return;
    w = &HV->wk[i]; memset(w, 0, sizeof *w);
    w->x = ENTX; w->y = 7; w->job = J_IDLE; HV->pop++;
    hs_assign();
}
static void kill_one(void){ if(HV->pop > 1){ HV->pop--; HV->dead++; hs_assign(); } }
static UINT8 pickres(void){
    UINT8 k = HV->kind, a = prof[k][0] + 2, b = prof[k][1] + 1, h = prof[k][4] + 1 + (season() == 2 ? 2 : 0), r;
    if(HV->evt == E_BLOOM) h += 2;
    if(HV->res[R_FOOD] >= hs_stock() * 3 / 4) a = 0;                /* foragers bring what is short, not what already fills the larder */
    if(HV->res[R_HONEY] >= hs_stock() / 2) h = 0;
    if(HV->res[R_LEAF] >= hs_stock() * 3 / 4) b = 0;
    if(!(a + b + h)) a = 1;
    r = hs_rnd() % (a + b + h);
    return r < a ? R_FOOD : r < a + b ? R_LEAF : R_HONEY;
}
static UINT16 pct_forage(void){
    static const UINT8 sp[4] = { 110, 100, 140, 45 };
    static const UINT8 wp[W_N] = { 100, 80, 0, 45, 70 };
    UINT16 p = (UINT16)sp[season()] * wp[HV->wx] / 100;
    if(has_tech(0)) p += p / 4;
    if(HV->evt == E_DROUGHT) p /= 2;
    if(HV->evt == E_BLOOM) p += p / 2;
    if(HV->hap < 25) p -= p / 3;
    return p;
}
static UINT8 night(void){ return HV->tod >= 14 && HV->tod < 18; }
static void deliver(Wk *w){
    UINT8 r = (w->carry & 7), a = w->carry >> 3;
    if(r){ add_res(r - 1, a); }
    w->carry = 0;
}
static UINT16 find_nb(UINT16 i, UINT8 gc){
    UINT8 d, x = (UINT8)(i & 31), y = (UINT8)(i >> 5); UINT16 n; UINT8 m;
    for(d = 0; d < 4; d++){
        if((d == 0 && y == 0) || (d == 1 && x == MW - 1) || (d == 2 && y == MH - 1) || (d == 3 && x == 0)) continue;
        n = i + (d == 0 ? -32 : d == 2 ? 32 : d == 1 ? 1 : -1); m = MAP[n];
        if(gc == GC_MARK ? ((m & MARKD) && CT(m) <= C_CRYS && CT(m) != C_ROCK && CT(m) != C_TUNNEL) : CT(m) == C_SITE) return n;
    }
    return 0xFFFF;
}
static UINT8 site_need(UINT8 type){ (void)type; return 4 - (HV->gear >= 20 ? 1 : 0) - (HV->gear >= 60 ? 1 : 0); }
static void finish_site(Site *s){
    UINT16 i = CI(s->x, s->y);
    MAP[i] = s->type; s->type = 0; if(HV->nsite) HV->nsite--;
    hs_dirty(i); hs_recount(); hs_note(M_BUILT);
}
static void dig(UINT16 n, Wk *w){
    UINT8 t = CT(MAP[n]);
    if(t == C_ORE){ w->carry = (R_ORE + 1) | ((3 + (HV->gear >= 40 ? 1 : 0)) << 3); }
    else if(t == C_CRYS){ w->carry = (R_CRYS + 1) | (1 << 3); }
    else if(t == C_SOIL && !(hs_rnd() & 7)) w->carry = (R_LEAF + 1) | (1 << 3);
    MAP[n] = C_TUNNEL; hs_dirty(n);
}
static void wk_step(Wk *w, UINT8 d){ w->x += dxs[d]; w->y += dys[d]; w->dir = d; w->sub = 8; }
static void wander(Wk *w){
    UINT8 d = hs_rnd() & 3, k; UINT16 n;
    for(k = 0; k < 4; k++, d = (d + 1) & 3){
        if((d == 0 && w->y == 0) || (d == 1 && w->x == MW - 1) || (d == 2 && w->y == MH - 1) || (d == 3 && w->x == 0)) continue;
        n = CI((UINT8)(w->x + dxs[d]), (UINT8)(w->y + dys[d]));
        if(passable(MAP[n])){ wk_step(w, d); return; }
    }
    w->st = 3; w->tm = 20;
}
/* one worker, at rest on a cell: choose */
void hs_think(UINT8 i) BANKED {
    Wk *w = &HV->wk[i]; UINT8 gc, d, here; UINT16 n, c = CI(w->x, w->y);
    if(w->st == 3 || w->st == 1){                                    /* (the map counts tm down: here it is 0) */
        if(w->st == 1){                                              /* work done */
            w->st = 0;
            if(w->job == J_BUILD || w->job == J_MINE){
                n = find_nb(c, GC_SITE);
                if(n != 0xFFFF && w->job == J_BUILD){
                    UINT8 k; for(k = 0; k < NSITE; k++) if(HV->site[k].type && CI(HV->site[k].x, HV->site[k].y) == n){ if(++HV->site[k].prog >= site_need(HV->site[k].type)) finish_site(&HV->site[k]); break; }
                } else {
                    n = find_nb(c, GC_MARK);
                    if(n != 0xFFFF){ dig(n, w); }
                }
            }
        } else w->st = 0;
        return;
    }
    if(w->st == 2){ w->st = 0;                                      /* back from outside */
        { UINT16 p = pct_forage(), a = 1 + (hs_rnd() & 1) + ((hs_rnd() & 3) == 0); a = (a * p + hs_rnd() % 100) / 100; w->carry = (pickres() + 1) | (UINT8)((a ? a : 0) << 3); if(!a) w->carry = 0; HV->stat[0]++; }
        return; }
    here = CT(MAP[c]);
    switch(w->job){
        case J_FORAGE:
            if(w->carry){ gc = HV->bcnt[C_STORE] ? GC_STORE : GC_QUEEN; }
            else if(night() || HV->wx == W_STORM){ gc = GC_QUEEN; }
            else gc = GC_ENTRY;
            break;
        case J_MINE: gc = w->carry ? (HV->bcnt[C_STORE] ? GC_STORE : GC_QUEEN) : GC_MARK; break;
        case J_BUILD: gc = w->carry ? (HV->bcnt[C_STORE] ? GC_STORE : GC_QUEEN) : (HV->nsite ? GC_SITE : GC_MARK); break;
        case J_NURSE: gc = HV->bcnt[C_NURS] ? GC_NURS : GC_QUEEN; break;
        case J_GUARD: gc = (w->carry & 1) ? GC_ENTRY : (HV->bcnt[C_BARR] ? GC_BARR : GC_QUEEN); break;
        default: gc = HV->bcnt[C_FARM] + HV->bcnt[C_SMELT] + HV->bcnt[C_WORK] + HV->bcnt[C_SHRINE] + HV->bcnt[C_MARKET] ? GC_STAT : GC_QUEEN; break;
    }
    d = bfs(w->x, w->y, gc);
    if(d < 4){ wk_step(w, d); return; }
    if(d == 255){ if(w->job == J_BUILD && gc == GC_SITE && 0) return; wander(w); if(w->st == 0 && !w->sub) { w->st = 3; w->tm = 30; } return; }
    /* arrived */
    (void)here;
    switch(gc){
        case GC_ENTRY:
            if(w->job == J_FORAGE){ w->st = 2; w->tm = 90 + (hs_rnd() & 63); }
            else { w->carry ^= 1; w->st = 3; w->tm = 50; }
            break;
        case GC_STORE: case GC_QUEEN:
            if(w->carry && w->job != J_GUARD && w->job != J_NURSE && w->job != J_IDLE){ deliver(w); }
            w->st = 3; w->tm = 24 + (hs_rnd() & 15);
            if(w->job == J_GUARD) w->tm = 80;
            break;
        case GC_MARK:
            w->st = 1; w->tm = w->job == J_MINE ? 40 : 70; break;
        case GC_SITE:
            w->st = 1; w->tm = 36; break;
        case GC_BARR:
            w->carry ^= 1; w->st = 3; w->tm = 70; break;
        default:
            w->st = 3; w->tm = 90 + (hs_rnd() & 63); break;
    }
}
/* ------------------------------------------------------------------ buildings */
void hs_cost(UINT8 type, UINT8 lv, UINT16 *c) BANKED {
    UINT8 k;
    if(type < C_FIRSTB || type >= C_N){ c[0] = c[1] = c[2] = c[3] = 0; return; }
    for(k = 0; k < 4; k++) c[k] = (UINT16)bc[type - C_FIRSTB][k] * lv;
}
static const UINT8 cr[4] = { R_LEAF, R_ORE, R_BAR, R_CRYS };
static UINT8 can_pay(const UINT16 *c){ UINT8 k; for(k = 0; k < 4; k++) if(HV->res[cr[k]] < c[k]) return 0; return 1; }
static void pay(const UINT16 *c){ UINT8 k; for(k = 0; k < 4; k++) HV->res[cr[k]] -= c[k]; }
UINT8 hs_unlocked(UINT8 t) BANKED {
    switch(t){
        case C_SMELT: return has_tech(2);
        case C_MARKET: return has_tech(3);
        case C_GATE: return has_tech(5);
        case C_TOWER: return has_tech(6);
        default: return 1;
    }
}
UINT8 hs_place(UINT8 x, UINT8 y, UINT8 type) BANKED {
    UINT16 c[4], i = CI(x, y); UINT8 k;
    if(MAP[i] != C_TUNNEL) return M_NOCELL;
    if(!hs_unlocked(type)) return M_NEEDTECH;
    if(HV->nsite >= NSITE) return M_SITEFULL;
    hs_cost(type, 1, c); if(!can_pay(c)) return M_NOPAY;
    if(type == C_WALL){ MAP[i] = C_WALL; k = hs_connected(); MAP[i] = C_TUNNEL; if(!k) return M_NOWALL; }
    for(k = 0; k < NSITE; k++) if(!HV->site[k].type) break;
    if(k >= NSITE) return M_SITEFULL;
    pay(c); HV->site[k].x = x; HV->site[k].y = y; HV->site[k].type = type; HV->site[k].prog = 0; HV->nsite++;
    MAP[i] = C_SITE; hs_dirty(i); hs_recount();
    return 0;
}
UINT8 hs_upgrade(UINT8 x, UINT8 y) BANKED {
    UINT16 c[4], i = CI(x, y); UINT8 t = CT(MAP[i]), l = CLV(MAP[i]) + 1;
    if(t < C_FIRSTB) return M_NOCELL;
    if(l >= 8) return M_MAXLV;
    hs_cost(t, l + 1, c); if(!can_pay(c)) return M_NOPAY;
    pay(c); MAP[i] += 0x20; hs_dirty(i); hs_recount(); return 0;
}
UINT8 hs_can_dig(UINT16 i) BANKED {
    UINT8 t = CT(MAP[i]);
    if((i >> 5) < 3) return 0;
    if(t == C_SOIL || t == C_ORE) return 1;
    if(t == C_CRYS) return has_tech(8) ? 1 : 2;
    return 0;
}
/* ------------------------------------------------------------------ research */
void hs_tech_cost(UINT8 t, UINT16 *c) BANKED {
    UINT8 tier = t & 3, br = t >> 2;
    c[0] = 12 * (tier + 1); c[1] = tier ? 8 * tier : 0; c[2] = tier >= 2 ? 3 * tier : 0;
    c[3] = (br == 2 && tier) || tier >= 2 ? tier : 0; c[4] = tier ? 2 * tier : 0;
    if(t == 8){ c[1] = 0; c[4] = 0; }
}
UINT8 hs_tech_ok(UINT8 t) BANKED { return (t & 3) == 0 || has_tech(t - 1); }
UINT8 hs_start_research(UINT8 t) BANKED {
    UINT16 c[5]; UINT8 k;
    if(HV->rs != 255) return M_RESEARCH;
    if(has_tech(t) || !hs_tech_ok(t)) return M_NOCELL;
    hs_tech_cost(t, c);
    for(k = 0; k < 4; k++) if(HV->res[cr[k]] < c[k]) return M_NOPAY;
    if(HV->res[R_HONEY] < c[4]) return M_NOPAY;
    for(k = 0; k < 4; k++) HV->res[cr[k]] -= c[k];
    HV->res[R_HONEY] -= c[4]; HV->rs = t; HV->rsleft = 8 + 4 * (t & 3) + (t == 8 ? 0 : 0);
    return 0;
}
/* ------------------------------------------------------------------ raids */
UINT16 hs_strength(UINT8 kind) BANKED {
    UINT16 s = 6 + 3 * (UINT16)HV->rn + HV->day / 3;
    s = s * (HV->diff == 0 ? 70 : HV->diff == 1 ? 100 : 140) / 100;
    if(kind == RK_BOSS) s *= 2; else if(kind == RK_RIVAL) s += s / 5;
    return s;
}
UINT16 hs_defence(void) BANKED {
    UINT8 i, g = 0; UINT16 d;
    for(i = 0; i < HV->pop; i++) if(HV->wk[i].job == J_GUARD) g++;
    d = (UINT16)g * (3 + HV->gear / 10 + (has_tech(4) ? 2 : 0));
    d += 3 * (UINT16)HV->bsum[C_WALL] + 3 * (UINT16)HV->bsum[C_GATE] + (UINT16)HV->bsum[C_TOWER] * (has_tech(6) ? 7 : 5);
    d += HV->hap / 20 + 1;
    return d;
}
static void schedule(void){
    HV->rday = HV->day + (HV->diff == 0 ? 8 : HV->diff == 1 ? 6 : 4) + (hs_rnd() % 3);
    HV->rpend = 0;
}
void hs_raid_result(UINT8 won) BANKED {
    UINT8 k = HV->rpend ? HV->rpend - 1 : RK_BANDIT, r, k2 = HV->rpend2;
    if(won){
        HV->rwon++; HV->shells += 4 + HV->rn + (k2 ? 3 : 0); if(HV->shells > 9999) HV->shells = 9999;
        if(k == RK_BOSS){ HV->boss++; add_res(R_CRYS, 3); HV->shells += 15; }
        else add_res(hs_rnd() & 1 ? R_FOOD : R_ORE, 5);
        if(HV->hap < 90) HV->hap += 6;
        hs_note(M_WON);
    } else {
        HV->rlost++;
        if(k != RK_RIVAL || k2) for(r = 0; r < R_N; r++) HV->res[r] -= HV->res[r] / (k == RK_BOSS ? 2 : k2 ? 2 : 3);
        if(k != RK_BANDIT || k2 == 1 + RK_RIVAL){ UINT8 n = HV->pop / (k == RK_BOSS ? 3 : 5); if(!n) n = 1; while(n-- && HV->pop > 3) kill_one(); }
        HV->shells -= HV->shells / 4; HV->grief = 8; if(HV->hap > 15) HV->hap -= 15;
        hs_note(M_LOST);
    }
    HV->rn++; HV->rpend2 = 0; schedule();
}
void hs_raid_auto(void) BANKED {
    UINT8 k = HV->rpend - 1; UINT16 d = hs_defence(), s = hs_strength(k);
    if(HV->rpend2) s += hs_strength(HV->rpend2 - 1) * 2 / 3;               /* two raids at once */
    s = s - s / 8 + hs_rnd() % (s / 4 + 1);
    hs_raid_result(d >= s);
}
/* an outpost is raided: its own level and a third of the home guards stand against a weaker band */
void hs_outpost_raid(void) BANKED {
    UINT8 k = HV->orp ? HV->orp - 1 : 0; UINT16 d, s;
    HV->stat[2] = 0;
    if(!HV->orp || !HV->outp[k]){ HV->orp = 0; return; }
    d = 7 * (UINT16)HV->outp[k] + hs_defence() / 4; s = hs_strength(RK_BANDIT) * 3 / 4;
    s = s - s / 8 + hs_rnd() % (s / 4 + 1);
    if(d >= s){ HV->orwon++; HV->shells += 3 + HV->outp[k]; if(HV->shells > 9999) HV->shells = 9999; HV->stat[2] = 1; hs_note(M_OPWON); }
    else { HV->orlost++; HV->outp[k]--; HV->shells -= HV->shells / 8; HV->stat[2] = 2; hs_note(M_OPLOST); }
    HV->orp = 0;
}
UINT8 hs_caravan_here(void) BANKED { return HV->cvs != 0 && HV->cvt == 0; }
/* ------------------------------------------------------------------ quests, score, achievements */
UINT16 hs_kmask(void) BANKED {
    UINT16 m = 0; UINT16 s;
    for(s = 0; s < 256; s++) if(ex.seen[s >> 3] & (1 << (s & 7))) m |= 1 << world_kind(world_seed((UINT8)s));
    return m;
}
UINT8 hs_seen_count(void) BANKED {
    UINT8 i, n = 0, b;
    for(i = 0; i < 32; i++) for(b = ex.seen[i]; b; b >>= 1) n += b & 1;
    return n;
}
static UINT8 ntech(void){ UINT8 i, n = 0; for(i = 0; i < NTECH; i++) n += has_tech(i); return n; }
void hs_quest_new(UINT8 q) BANKED {
    UINT8 k = (UINT8)((HV->qn + q * 3 + (hs_rnd() & 7)) & 7), r; UINT16 v = 0;
    if(HV->qn < 2 && k > 4) k &= 3;
    switch(k){
        case 0: r = hs_rnd() % 4; v = ((UINT16)r << 12) | (UINT16)(30 + 25 * HV->qn > 400 ? 400 : 30 + 25 * HV->qn); if(r == 3) v = (3U << 12) | 8 + 2 * HV->qn; break;
        case 1: r = C_FIRSTB + hs_rnd() % NBLD; v = ((UINT16)r << 8) | (1 + HV->qn / 3 > 6 ? 6 : 1 + HV->qn / 3); break;
        case 2: v = HV->pop + 2 > NWK ? NWK : HV->pop + 2; if(HV->pop >= NWK){ k = 7; v = HV->day + 6; } break;
        case 3: v = HV->rwon + 1; break;
        case 4: v = ntech() + 1; if(v > NTECH){ k = 7; v = HV->day + 6; } break;
        case 5: v = hs_seen_count() + 2; break;
        case 6: { UINT16 m = hs_kmask(); r = hs_rnd() % 12; while(m & (1 << r)) { r = (r + 1) % 12; if(r == 0 && m == 0xFFF) break; } v = r; if(m == 0xFFF){ k = 7; v = HV->day + 6; } break; }
        default: v = HV->day + 6; k = 7; break;
    }
    HV->qt[q] = k; HV->qv[q] = v;
}
UINT8 hs_quest_done(UINT8 q) BANKED {
    UINT16 v = HV->qv[q];
    switch(HV->qt[q]){
        case 0: return HV->res[v >> 12] >= (v & 0xFFF);
        case 1: return HV->bsum[v >> 8] >= (v & 255);
        case 2: return HV->pop >= v;
        case 3: return HV->rwon >= v;
        case 4: return ntech() >= v;
        case 5: return hs_seen_count() >= v;
        case 6: return (hs_kmask() >> v) & 1;
        default: return HV->day >= v;
    }
}
void hs_quest_claim(UINT8 q) BANKED {
    UINT16 sh = 8 + 4 * (UINT16)HV->qn; if(sh > 60) sh = 60;
    if(has_tech(11)) sh *= 2;
    HV->shells += sh; if(HV->shells > 9999) HV->shells = 9999;
    if(HV->qt[q] >= 5) add_res(R_CRYS, 2); else if(HV->qn % 3 == 2) add_res(R_HONEY, 4);
    HV->qn++; hs_quest_new(q); hs_note(M_QUESTDONE);
}
UINT32 hs_score(void) BANKED {
    UINT32 s; UINT8 i, a = 0; UINT16 b = 0, o = 0;
    for(i = C_FIRSTB; i < C_N; i++) b += HV->bsum[i];
    for(i = 0; i < 12; i++) o += HV->outp[i];
    for(i = 0; i < 16; i++) a += (HV->ach >> i) & 1;
    s = 8UL * HV->pop + 2UL * HV->day + 6UL * b + 40UL * ntech() + 30UL * HV->rwon + 150UL * HV->boss + 40UL * HV->qn + 60UL * a + 25UL * o;
    return s * (HV->diff == 0 ? 7 : HV->diff == 1 ? 10 : 15) / 10;
}
void hs_ach(void) BANKED {
    UINT16 a = HV->ach, i; UINT8 t, all = 1, big = 0, n = 0;
    for(t = C_FIRSTB; t < C_N; t++){ if(!HV->bcnt[t]) all = 0; if(HV->bsum[t] > HV->bcnt[t] * 4 || (HV->bcnt[t] && HV->bsum[t] / HV->bcnt[t] >= 5)) big = 1; }
    for(i = 0; i < 12; i++) n += HV->outp[i] ? 1 : 0;
    if(HV->pop >= 5) a |= 1; if(HV->pop >= 10) a |= 2; if(HV->pop >= 16) a |= 4;
    if(HV->rwon) a |= 8; if(HV->boss) a |= 16;
    if(HV->res[R_BAR]) a |= 32; if(HV->res[R_CRYS]) a |= 64; if(HV->tech) a |= 128; if(ntech() >= 12) a |= 256;
    if(HV->day >= 10) a |= 512; if(HV->day >= 24) a |= 1024; if(big) a |= 2048; if(all) a |= 4096;
    if(HV->qn >= 5) a |= 8192; if(n) a |= 16384; if(HV->royal) a |= 32768U;
    if(a != HV->ach){ HV->ach = a; hs_note(M_ACH); }
}
/* ------------------------------------------------------------------ the tick */
static void daily(void){
    UINT8 s = season(), r, q;
    HV->day++; HV->tod = 0;
    if(HV->day % 6 == 0) hs_note(M_SEASON0 + ((HV->day / 6) & 3));
    /* weather */
    if(HV->wleft) HV->wleft--;
    if(!HV->wleft){
        r = hs_rnd() % 100;
        if(r < 22) HV->wx = (s == 3 || HV->kind == 2) ? W_SNOW : W_RAIN;
        else if(r < 30) HV->wx = W_STORM;
        else if(r < 38 && (s == 1 || HV->kind == 1)) HV->wx = W_DRY;
        else HV->wx = W_CLEAR;
        HV->wleft = 1 + (hs_rnd() & 1);
        if(!HV->away) hs_note(M_CLEAR + (HV->wx == W_RAIN ? M_RAIN - M_CLEAR : HV->wx == W_STORM ? M_STORM - M_CLEAR : HV->wx == W_SNOW ? M_SNOW - M_CLEAR : HV->wx == W_DRY ? M_DRY - M_CLEAR : 0));
    }
    /* events */
    if(HV->evleft){ HV->evleft--; if(!HV->evleft) HV->evt = E_NONE; }
    else if(HV->day > 3 && hs_rnd() % 100 < 22){
        r = hs_rnd() % 4;
        if(r == 0){ HV->evt = E_DROUGHT; HV->evleft = 3; hs_note(M_DROUGHT); }
        else if(r == 1){ HV->evt = E_PLAGUE; HV->evleft = 3; hs_note(M_PLAGUE); }
        else if(r == 2){ HV->evt = E_TRADERS; HV->evleft = 3; HV->trader = 3; HV->town = hs_rnd(); hs_note(M_TRADERS); }
        else { HV->evt = E_BLOOM; HV->evleft = 3; hs_note(M_BLOOM); }
    }
    if(HV->trader) HV->trader--;
    /* raids */
    if(!HV->rpend && HV->day >= HV->rday){
        HV->rpend = 1 + ((HV->rn + 1) % 5 == 0 ? RK_BOSS : (hs_rnd() & 1) ? RK_RIVAL : RK_BANDIT);
        HV->rpend2 = 0;
        if(HV->rn >= 2 && HV->rpend != 1 + RK_BOSS && (UINT8)(hs_rnd() % 100) < 10 + 12 * HV->diff) HV->rpend2 = 1 + ((hs_rnd() & 1) ? RK_RIVAL : RK_BANDIT);   /* a second band at once */
        if(HV->away){ hs_raid_auto(); }
    }
    if(!HV->orp && HV->day >= HV->orday){                          /* an outpost is raided */
        UINT8 k, n = 0, pick;
        for(k = 0; k < 12; k++) if(HV->outp[k]) n++;
        HV->orday = HV->day + (HV->diff == 0 ? 9 : HV->diff == 1 ? 7 : 5) + (hs_rnd() % 3);
        if(n){
            pick = hs_rnd() % n;
            for(k = 0; k < 12; k++) if(HV->outp[k]){ if(!pick){ HV->orp = k + 1; break; } pick--; }
            if(HV->away) hs_outpost_raid();
        }
    }
    /* caravans: one sets out now and then, takes a while to arrive, and stays two days */
    if(HV->cvn) HV->cvn--;
    else if(!HV->cvt && !HV->cvs && HV->bcnt[C_MARKET] && has_tech(3)){
        HV->cvt = 8; HV->cvn = 3 + (hs_rnd() % 3); HV->cvg = hs_rnd() % R_N; HV->cvw = (HV->cvg + 1 + hs_rnd() % (R_N - 1)) % R_N; HV->cvq = 6 + (hs_rnd() % 10); HV->cvf = 0;
    }
    if(!HV->rpend && !HV->away && ((HV->tech >> 7) & 1) && HV->day + 1 == HV->rday) hs_note(M_WARN);
    for(q = 0; q < NQ; q++) if(!HV->qt[q] && !HV->qv[q]) hs_quest_new(q);
    if(HV->grief) HV->grief--;
    hs_ach();
    { UINT32 sc = hs_score(); if(sc > HV->best) HV->best = sc; }
}
void hs_tick(void) BANKED {
    UINT8 eat, i, fed = 1; UINT16 n; INT16 tgt;
    /* the clock */
    HV->tod++;
    if(HV->tod >= 20) daily();
    if(HV->cvt){ HV->cvt--; if(!HV->cvt){ HV->cvs = 40; if(!HV->away) hs_note(M_CARAVAN); } }
    else if(HV->cvs){ HV->cvs--; if(!HV->cvs && !HV->away) hs_note(M_CARAWAY); }
    /* food */
    if(!(HV->tod & 1)){
        eat = (HV->pop + 3) / 4; if(HV->diff == 2) eat += (eat + 2) / 3;
        if(HV->res[R_FOOD] >= eat){ HV->res[R_FOOD] -= eat; HV->hunger = 0; }
        else { HV->res[R_FOOD] = 0; HV->hunger++; if(HV->hunger >= 2){ HV->hunger = 0; if(HV->pop > 2){ kill_one(); hs_note(M_STARVE); } } }
    }
    fed = HV->hunger == 0;
    /* the fungus garden: leaf in, food out (a little honey from level 4 up); staffed by the idle */
    if(HV->bsum[C_FARM]){
        UINT8 idle = hs_nidle(); UINT16 f = HV->bsum[C_FARM], lf = f * 2, mk;
        mk = 40 + 20 * (UINT16)idle; if(mk > 140) mk = 140;
        if(HV->res[R_LEAF] < lf) lf = HV->res[R_LEAF];
        { UINT16 room = hs_stock() > HV->res[R_FOOD] ? hs_stock() - HV->res[R_FOOD] : 0; if(lf * 3 / 2 > room) lf = room * 2 / 3 + (room ? 1 : 0); }      /* a full larder does not eat leaf */
        HV->res[R_LEAF] -= lf;
        n = lf * 3 / 2 * mk / 100; if(has_tech(1)) n += n / 2; if(HV->kind == 7) n += n / 4; if(HV->wx == W_RAIN) n += n / 4; if(HV->evt == E_DROUGHT) n -= n / 3;
        add_res(R_FOOD, n);
        if(HV->bcnt[C_FARM] && HV->bsum[C_FARM] >= 4 && HV->res[R_FOOD] > 40 && !(HV->tod & 1)){ HV->res[R_FOOD] -= 4; add_res(R_HONEY, 1); }
    }
    if(HV->bsum[C_SMELT]){
        UINT8 b = HV->bsum[C_SMELT], idle = hs_nidle();
        if(!idle) b = (b + 1) / 2;
        while(b-- && HV->res[R_ORE] >= 2 && HV->res[R_LEAF] >= 1){ HV->res[R_ORE] -= 2; HV->res[R_LEAF]--; add_res(R_BAR, has_tech(2) && !(hs_rnd() & 1) ? 2 : 1); }
    }
    if(HV->bsum[C_WORK]){
        UINT8 b = HV->bsum[C_WORK]; UINT8 gcap = 10 * HV->bsum[C_WORK]; if(gcap > 100) gcap = 100;
        while(b-- && HV->res[R_BAR] >= 2 && HV->gear < gcap){ HV->res[R_BAR] -= 2; HV->gear++; }
    }
    /* births */
    {
        UINT8 nn = 0; for(i = 0; i < HV->pop; i++) if(HV->wk[i].job == J_NURSE) nn++;
        n = 1 + HV->bsum[C_NURS] + (HV->bcnt[C_NURS] ? 2 * (UINT16)nn : 0);
        if(has_tech(9)) n += n / 2; if(HV->hap >= 75) n += n / 4; if(HV->hap < 30) n /= 2;
        HV->egg += (UINT8)(n > 20 ? 20 : n);
        if(HV->egg >= 16){
            if(HV->pop < hs_popcap() && HV->res[R_FOOD] >= 4 && fed){ HV->egg = 0; HV->res[R_FOOD] -= 3; spawn(); hs_note(M_HATCH); }
            else HV->egg = 16;
        }
    }
    /* happiness */
    tgt = 55;
    if(!fed) tgt -= 30;
    tgt += 3 * (INT16)(HV->bsum[C_SHRINE] > 6 ? 6 : HV->bsum[C_SHRINE]);
    { UINT8 sf = HV->bsum[C_WALL] + HV->bsum[C_TOWER] + HV->bsum[C_GATE]; tgt += sf > 8 ? 8 : sf; }
    if(HV->res[R_HONEY] && (HV->tod & 3) == 0){ HV->res[R_HONEY]--; tgt += 12; }
    else if(HV->res[R_HONEY] > 3) tgt += 6;
    if(HV->wx == W_STORM) tgt -= 6; else if(HV->wx == W_CLEAR) tgt += 3;
    if(HV->evt == E_PLAGUE) tgt -= 18; if(HV->evt == E_BLOOM) tgt += 6;
    if(HV->grief) tgt -= 12;
    if(HV->pop > hs_popcap()) tgt -= 10;
    if(HV->bcnt[C_STORE] && HV->bcnt[C_NURS]) tgt += 4;
    if(tgt < 0) tgt = 0; if(tgt > 100) tgt = 100;
    if(HV->hap < tgt) HV->hap += (tgt - HV->hap > 3 ? 2 : 1); else if(HV->hap > tgt) HV->hap -= (HV->hap - tgt > 3 ? 2 : 1);
    if(HV->hap < 12 && HV->pop > 3 && !(hs_rnd() & 7)){ kill_one(); hs_note(M_LEFT); }
    if(HV->evt == E_PLAGUE && HV->pop > 3 && (hs_rnd() % 100) < (HV->bsum[C_SHRINE] ? 1 : 3) && !(HV->tod & 1)){ kill_one(); }
    /* research */
    if(HV->rs != 255){
        UINT8 sp = 1 + HV->bsum[C_SHRINE] / 3;
        if(HV->rsleft > sp) HV->rsleft -= sp;
        else { HV->tech |= 1 << HV->rs; HV->rs = 255; HV->rsleft = 0; hs_note(M_TECHDONE); }
    }
    /* outposts on the other kinds of world */
    if(!(HV->tod & 1)){
        UINT8 k, r, bs;
        for(k = 0; k < 12; k++) if(HV->outp[k]){
            bs = 0; for(r = 1; r < 5; r++) if(prof[k][r] > prof[k][bs]) bs = r;
            add_res(bs == 0 ? R_FOOD : bs == 1 ? R_LEAF : bs == 2 ? R_ORE : bs == 3 ? R_CRYS : R_HONEY, (UINT16)HV->outp[k] * (1 + prof[k][bs] / 2));
        }
    }
    /* away: the workers are not on the screen, the formula stands in for them */
    if(HV->away){
        UINT8 f = 0, m = 0, b = 0, p = night() ? 1 : 2;
        UINT16 pc = pct_forage();
        for(i = 0; i < HV->pop; i++){ UINT8 j = HV->wk[i].job; if(j == J_FORAGE) f++; else if(j == J_MINE) m++; else if(j == J_BUILD) b++; }
        while(f--){ if(hs_rnd() % 3 < p){ UINT16 q = ((1 + (hs_rnd() & 1)) * pc + hs_rnd() % 100) / 100; if(q) add_res(pickres(), q); } }
        while(m--){ if(hs_rnd() & 1){ add_res(R_ORE, 2); if(has_tech(8) && !(hs_rnd() & 7)) add_res(R_CRYS, 1); } }
        for(i = 0; i < NSITE && b; i++) if(HV->site[i].type){ HV->site[i].prog += b > 2 ? 2 : 1; if(HV->site[i].prog >= site_need(HV->site[i].type)) finish_site(&HV->site[i]); }
    }
}
void hs_away(UINT16 ticks) BANKED {
    UINT8 w0 = HV->rwon, l0 = HV->rlost, p0 = HV->pop;
    if(ticks > 200) ticks = 200;
    HV->awayd = (UINT8)(ticks / 20); HV->away = 1;
    while(ticks--) hs_tick();
    HV->away = 0; HV->awayw = HV->rwon - w0; HV->awayl = HV->rlost - l0; (void)p0;
}

#ifdef HT_RICH
/* test builds: a grown colony in one step */
void hs_test_rich(void) BANKED {
    UINT8 x, i;
    for(x = 10; x <= 22; x++){ MAP[CI(x, 7)] = C_TUNNEL; MAP[CI(x, 9)] = C_TUNNEL; MAP[CI(x, 11)] = C_TUNNEL; }
    MAP[CI(ENTX, 7)] = C_QUEEN;
    for(i = 8; i <= 10; i++) MAP[CI(ENTX, i)] = C_TUNNEL;
    MAP[CI(ENTX, 11)] = C_TUNNEL; MAP[CI(15, 5)] = C_TUNNEL; MAP[CI(17, 5)] = C_TUNNEL; MAP[CI(15, 4)] = C_TUNNEL; MAP[CI(17, 4)] = C_TUNNEL;
    for(x = 12; x <= 20; x += 2) MAP[CI(x, 8)] = C_TUNNEL;
    MAP[CI(ENTX, 4)] = C_GATE | (2 << 5);
    MAP[CI(11, 7)] = C_SHRINE | (2 << 5); MAP[CI(13, 7)] = C_NURS | (3 << 5); MAP[CI(14, 7)] = C_NURS | (1 << 5); MAP[CI(18, 7)] = C_STORE | (3 << 5); MAP[CI(19, 7)] = C_MARKET | (1 << 5);
    MAP[CI(21, 7)] = C_TOWER | (4 << 5);
    MAP[CI(11, 9)] = C_FARM | (3 << 5); MAP[CI(13, 9)] = C_WORK | (1 << 5); MAP[CI(15, 9)] = C_SMELT | (2 << 5); MAP[CI(18, 9)] = C_BARR | (3 << 5); MAP[CI(20, 9)] = C_BARR | (0 << 5);
    MAP[CI(15, 5)] = C_TOWER | (5 << 5); MAP[CI(17, 5)] = C_TOWER | (1 << 5); MAP[CI(12, 11)] = C_WALL | (2 << 5); MAP[CI(21, 11)] = C_GATE | (1 << 5); MAP[CI(14, 11)] = C_STORE | (1 << 5);
    for(i = 0; i < R_N; i++) HV->res[i] = 150 + 20 * i;
    HV->shells = 400; HV->tech = 0x0FFF & ~0x0800; HV->day = 14; HV->pop = 0;
    for(i = 0; i < 14; i++){ HV->wk[i].x = 10 + i; HV->wk[i].y = 7; HV->wk[i].job = J_IDLE; HV->pop++; }
    HV->quota[0] = 4; HV->quota[1] = 2; HV->quota[2] = 2; HV->quota[3] = 3; HV->quota[4] = 2;
    HV->outp[3] = 2; HV->outp[8] = 1; HV->gear = 30; HV->hap = 80; HV->wx = W_RAIN; HV->wleft = 2;
    hs_recount(); hs_assign(); HV->tod = 13;
#ifdef HT_RAID
    HV->rpend = 1 + RK_BOSS; HV->rn = 4;
#endif
}
#endif
