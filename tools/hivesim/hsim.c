/* Host balance simulator for BUG HIVE: compiles src/hive_sim.c on the PC and plays many in-game days with a simple bot.
   build/run: see tools/hivesim/run.sh.  usage: hsim DAYS DIFF KIND SEED [verbose] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __at(x)
#define BANKED
#define NONBANKED
#include "gb/gb.h"
#include "save.h"
#include "menus.h"
#include "traits.h"
#include "hive_int.h"
Save sv; Extra ex;
HS hvs; UINT8 hv_map[1024], hv_vis[1024]; UINT16 hv_que[128], hv_dq[32]; HD hvd;
void sfx(UINT8 n){ (void)n; }
static UINT32 secs; static long inc[6]; static UINT16 prevr[6];
UINT32 play_secs(void){ return secs; }
UINT8 world_kind(UINT32 w){ return (UINT8)(w % 12); }
UINT32 world_seed(UINT8 s){ return s * 2654435761u; }
const char *const kind_name[12] = {0};
#include "../../src/hive_sim.c"
static int pick_mark(void){ /* mark one soil/ore cell touching a tunnel, preferring ore, with a limit on open marks */
    int open = 0, i, best = -1, bs = -9999;
    for(i = 0; i < 1024; i++) if((MAP[i] & MARKD) && CT(MAP[i]) <= C_CRYS) open++;
    if(open >= 6) return 0;
    for(i = 3 * 32; i < 1024; i++){
        UINT8 t = CT(MAP[i]); int x = i & 31, y = i >> 5, nb = 0, sc;
        if(t != C_SOIL && t != C_ORE) continue; if(MAP[i] & MARKD) continue;
        if(x < 2 || x > 29 || y > 29) continue;
        if(CT(MAP[i - 32]) == C_TUNNEL || CT(MAP[i + 32]) == C_TUNNEL || CT(MAP[i - 1]) == C_TUNNEL || CT(MAP[i + 1]) == C_TUNNEL) nb = 1;
        if(!nb) continue;
        { int dx, dy, bd = 9; for(dy = -6; dy <= 6; dy++) for(dx = -6; dx <= 6; dx++){ int xx = x + dx, yy = y + dy; if(xx < 1 || xx > 30 || yy < 3 || yy > 30) continue; if(CT(MAP[yy * 32 + xx]) == C_ORE && abs(dx) + abs(dy) < bd) bd = abs(dx) + abs(dy); } sc = (t == C_ORE ? 60 : 0) + (bd < 9 ? 40 - 5 * bd : 0) + 20 - y - abs(x - 16); }
        if(sc > bs){ bs = sc; best = i; }
    }
    if(best >= 0){ MAP[best] |= MARKD; return 1; }
    return 0;
}
static int try_place(int type){
    int i, c;
    for(i = 3 * 32; i < 1024; i++) if(MAP[i] == C_TUNNEL){
        int x = i & 31, y = i >> 5; if(x == ENTX && y < 9) continue;
        /* keep the shaft free; accept any other tunnel cell */
        c = hs_place((UINT8)x, (UINT8)y, (UINT8)type); if(!c) return 1; if(c == M_NOPAY || c == M_NEEDTECH || c == M_SITEFULL) return 0;
    }
    return 0;
}
static void bot_day(void){
    static const int want[] = { C_NURS, C_FARM, C_STORE, C_BARR, C_NURS, C_BARR, C_WORK, C_SMELT, C_SHRINE, C_TOWER, C_WALL, C_NURS, C_STORE, C_FARM, C_MARKET, C_GATE, C_TOWER, C_BARR, C_NURS };
    static const int lim[]  = { 1,      1,      1,       1,      2,      2,      1,      1,       1,        1,       1,      3,      2,       2,      1,        1,      2,       3,      4 };
    int i, t, n = sizeof want / sizeof *want, p = HV->pop, soon = HV->day + 2 >= HV->rday;
    UINT16 need;
    if(HV->rs == 255 && HV->res[R_LEAF] > 40){ static const int ro[] = { 2, 0, 1, 4, 5, 6, 3, 9, 7, 8, 10, 11 }; for(i = 0; i < 12; i++) if(!(HV->tech >> ro[i] & 1) && hs_tech_ok(ro[i]) && !hs_start_research(ro[i])) break; }
    for(i = 0; i < 6; i++) pick_mark();
    for(i = 0; i < n; i++){
        t = want[i]; if(HV->nsite >= 2) break;
        if(HV->bcnt[t] < lim[i] && hs_unlocked(t)){ if(try_place(t)) break; if(i < 4) break; }
    }
    if(HV->res[R_LEAF] > 50) for(i = 3 * 32; i < 1024; i++) if(CT(MAP[i]) >= C_FIRSTB && CT(MAP[i]) != C_SITE && CT(MAP[i]) != C_QUEEN && CLV(MAP[i]) < 3 && HV->nsite < 2){ if(!hs_upgrade((UINT8)(i & 31), (UINT8)(i >> 5))) break; }
    HV->quota[0] = p * 4 / 10 + 1; HV->quota[1] = p >= 6 ? 2 : 1; HV->quota[2] = HV->bcnt[C_NURS] ? (p >= 8 ? 2 : 1) : 0;
    HV->quota[3] = 1; HV->quota[4] = p >= 6 ? 2 : 1;
    if(HV->res[R_FOOD] < 15) HV->quota[0] += 2;
    if(soon){ /* raise guards until the defence beats the next raid */
        UINT8 k = (HV->rn + 1) % 5 == 0 ? RK_BOSS : RK_BANDIT; need = hs_strength(k) * 12 / 10;
        for(i = 1; i <= 12; i++){ HV->quota[3] = i; hs_assign(); if(hs_defence() >= need) break; }
    }
    hs_assign();
}
int main(int argc, char **argv){
    int days = argc > 1 ? atoi(argv[1]) : 40, diff = argc > 2 ? atoi(argv[2]) : 1, kind = argc > 3 ? atoi(argv[3]) : 0, seed = argc > 4 ? atoi(argv[4]) : 1, v = argc > 5;
    int f, t, d, i; static const char *dn[] = { "CALM", "NORMAL", "HARSH" };
    sv.seed = seed * 7919u + 13;
    hs_new(kind, diff);
    for(d = 0; d < days; d++){
        bot_day();
        for(t = 0; t < 20; t++){
            for(f = 0; f < 360; f++){
                UINT8 budget = HV->pop > 12 ? 2 : 3;
                for(i = 0; i < HV->pop; i++){ Wk *w = &HV->wk[i];
                    if(w->sub){ if(f & 1) w->sub--; } else if(w->tm){ w->tm--; } else if(budget){ hs_think(i); budget--; } }
                HV->stat[S_DQN] = 0; HV->stat[S_FULL] = 0;
                { int q; for(q = 0; q < 6; q++){ if(HV->res[q] > prevr[q]) inc[q] += HV->res[q] - prevr[q]; prevr[q] = HV->res[q]; } }
            }
            { int p0 = HV->pop; hs_tick(); if(v && HV->pop < p0) printf("  -1 pop day %d tod %d hap %d food %d hunger %d evt %d\n", HV->day, HV->tod, HV->hap, HV->res[0], HV->hunger, HV->evt); }
            if(HV->rpend){ if(v) printf("RAID day %d kind %d str %d def %d guards %d\n", HV->day, HV->rpend - 1, hs_strength(HV->rpend - 1), hs_defence(), HV->quota[3]); hs_raid_auto(); }
        }
        if(v && d % 5 == 4) {int k; printf("B:"); for(k=C_FIRSTB;k<C_N;k++) printf("%d ",HV->bcnt[k]); printf(" site %d q %d %d %d %d %d\n",HV->nsite,HV->quota[0],HV->quota[1],HV->quota[2],HV->quota[3],HV->quota[4]);}
        if(v && d % 5 == 4){
            int tn = 0, q, mk = 0; for(q = 0; q < 1024; q++){ tn += MAP[q] == C_TUNNEL; mk += (MAP[q] & MARKD) && CT(MAP[q]) <= C_CRYS; }
            printf("tun %d mk %d trips %d d%3d pop %2d hap %3d food %3d leaf %3d ore %3d bar %3d crys %3d hon %3d shells %4d gear %2d def %3d rn %d won %d lost %d tech %d\n", tn, mk, HV->stat[0], d + 1, HV->pop, HV->hap, HV->res[0], HV->res[1], HV->res[2], HV->res[3], HV->res[4], HV->res[5], HV->shells, HV->gear, hs_defence(), HV->rn, HV->rwon, HV->rlost, __builtin_popcount(HV->tech));
        }
    }
    printf("%s kind %d seed %d: day %d pop %d hap %d food %d shells %d raids %d/%d dead %d score %u\n", dn[diff], kind, seed, HV->day, HV->pop, HV->hap, HV->res[0], HV->shells, HV->rwon, HV->rlost, HV->dead, hs_score());
    printf("   income/day food %ld leaf %ld ore %ld bar %ld crys %ld hon %ld\n", inc[0] / days, inc[1] / days, inc[2] / days, inc[3] / days, inc[4] / days, inc[5] / days);
    {int k; printf("   B:"); for(k=C_FIRSTB;k<C_N;k++) printf("%d ",HV->bcnt[k]); {int tn=0,q,orn=0,mk=0; for(q=0;q<1024;q++){ tn+=MAP[q]==C_TUNNEL; orn+=CT(MAP[q])==C_ORE; mk+=(MAP[q]&MARKD)&&CT(MAP[q])<=C_CRYS;} printf(" ore cells %d marks %d ", orn, mk); printf(" tunnels %d tech %d gear %d\n", tn, __builtin_popcount(HV->tech), HV->gear);}}
    return 0;
}
