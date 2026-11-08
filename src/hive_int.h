#ifndef HIVE_INT_H
#define HIVE_INT_H
#include <gb/gb.h>
#include "hive.h"
/* BUG HIVE, the parts (hive.c bank 45: entry + save; hive_map.c 46: map, loop, sprites; hive_sim.c 47: the colony's rules and the workers; hive_ui.c 48: pages and texts;
   hive_def.c 49: the defend phase; hive_art.c 50: tiles). While the hive runs WRAM bank 6 is selected for good: the colony lives there (WRAM bank 6, D370..DFFF):
   the text engine's shadow (D000..D36F) is bank 6 too and its functions put the bank back as they found it. Nothing here may use the bank 1 buffers (fades, dialogs). */
#ifndef HT_KIND
#define HT_KIND 0
#define HT_DIFF 1
#endif
#define MW 32
#define MH 32
typedef struct { UINT8 x, y, job, st, tm, carry, dir, sub; } Wk;           /* a worker: st 0 think/walk, 1 work, 2 outside, 3 wait; carry = res+1 | amount<<3 */
typedef struct { UINT8 x, y, type, prog; } Site;                          /* a building going up */
enum { R_FOOD, R_LEAF, R_ORE, R_BAR, R_CRYS, R_HONEY, R_N };
enum { J_IDLE, J_FORAGE, J_BUILD, J_NURSE, J_GUARD, J_MINE, J_N };
enum { W_CLEAR, W_RAIN, W_STORM, W_SNOW, W_DRY, W_N };
enum { E_NONE, E_DROUGHT, E_PLAGUE, E_TRADERS, E_BLOOM, E_N };
enum { RK_BANDIT, RK_RIVAL, RK_BOSS };
/* map cell: low 5 bits = type; buildings: bits 5..7 = level - 1; soil/ore/crystal: bit 5 = marked to dig */
enum { C_SOIL, C_TUNNEL, C_ROCK, C_ORE, C_CRYS, C_SKY, C_GRASS, C_FLOWER, C_ENTRY, C_WATER, C_SITE, C_QUEEN,
       C_NURS, C_FARM, C_STORE, C_BARR, C_WORK, C_SMELT, C_WALL, C_GATE, C_TOWER, C_SHRINE, C_MARKET, C_N };
#define C_FIRSTB C_NURS
#define NBLD (C_N - C_FIRSTB)                    /* 11 buildings you can place */
#define CT(c)  ((c) & 31)
#define CLV(c) ((c) >> 5)                       /* level - 1 */
#define MARKD  0x20
#define NWK 24
#define NSITE 8
#define ENTX 16
#define ENTY 2
#define NTECH 12
#define NQ 3
typedef struct {
    UINT32 magic, seed;
    UINT8  ver, kind, diff, tod;                 /* diff 0 CALM 1 NORMAL 2 HARSH */
    UINT16 day;
    UINT16 res[R_N];
    UINT16 shells;
    UINT8  pop, hap, egg, gear;
    UINT8  quota[5];                             /* forage build nurse guard mine */
    UINT16 tech; UINT8 rs, rsleft;               /* research: tech being worked on (255 none), ticks left */
    UINT8  wx, wleft, evt, evleft;
    UINT16 rday; UINT8 rn, rwon, rlost, rpend;   /* next raid day, raids so far, won, lost, pending (1 + kind) */
    UINT8  boss;                                  /* bosses beaten */
    UINT8  trader, town;
    UINT8  qt[NQ]; UINT16 qv[NQ]; UINT8  qn;     /* quests: kind, goal value; quests done */
    UINT16 ach; UINT32 best;
    UINT8  outp[12];
    UINT16 claim; UINT8 royal, hunger, grief, links;
    UINT32 last; UINT16 rng; UINT8 away, nsite;
    UINT16 dead;                                  /* bugs lost */
    UINT8  note, notet;                           /* the message on the HUD (M_*) and how long it stays */
    UINT8  rpend2;                                /* a second raider coming with the first: 1 + kind, 0 none */
    UINT8  orp, orwon, orlost;                    /* an outpost under attack (index + 1), outpost raids won and lost */
    UINT16 orday;                                 /* day of the next outpost raid */
    UINT8  cvt, cvs, cvg, cvq, cvw, cvf, cvn;     /* caravan: ticks to arrive, ticks it stays, good it sells, how many, good it buys, done flags (1 bought 2 sold), days to the next */
    UINT8  awayd, awayw, awayl;                   /* what the last catch-up did: days, raids won, raids lost */
    UINT8  stat[8];                               /* S_* below */
    UINT8  bsum[C_N], bcnt[C_N];                  /* worked out from the map, not saved as truth */
    Wk     wk[NWK];
    Site   site[NSITE];
    UINT32 sum;
} HS;
extern __at(0xD370) HS hvs;                                  /* all of these sit in WRAM bank 6 at fixed places (defined in hive_sim.c with __at) */
extern __at(0xD600) UINT8 hv_map[1024];
extern __at(0xDA00) UINT8 hv_vis[1024];
extern __at(0xDE00) UINT16 hv_que[128];
extern __at(0xDF00) UINT16 hv_dq[32];                        /* cells to redraw, filled by the rules, emptied by the map */
typedef struct { UINT8 px[10], py[10], gx[10], gy[10], ghp[10], gflash[10], ox[12], oy[12], ohp[12], oflash[12], tx[8], ty[8], tl[8], gn, gmax, on_, oboss, brk_hp, brk_at_x, brk_at_y, tn; } HD;   /* the defend phase (WRAM bank 6, DF40) */
extern __at(0xDF40) HD hvd;
#define HV (&hvs)
#define MAP hv_map
#define VIS hv_vis
#define QUE hv_que
#define DQ  hv_dq
#define DQN 32
#define HV_VER 8
#define S_VGEN 1
#define S_DQN 4
#define S_FULL 5
#define S_SPEED 6                              /* 1 = fast time */
#define S_DEF 7                                /* 1 while the defend phase runs */
typedef char hv_size_check[(sizeof(HS) <= 640) ? 1 : -1];
#define CI(x, y) ((UINT16)(y) * MW + (x))
#define HVRAND() hs_rnd()
UINT8  hs_rnd(void) BANKED;
/* ---- hive_sim.c (bank 47) ---- */
void   hs_new(UINT8 kind, UINT8 diff) BANKED;              /* a fresh colony and its map */
void   hs_recount(void) BANKED;
void   hs_test_rich(void) BANKED;                           /* (test builds only) */
void   hs_tick(void) BANKED;                                /* one tick: 6 seconds of colony time, 20 ticks = a day */
void   hs_away(UINT16 ticks) BANKED;                        /* the colony alone for that many ticks (at most 200 = 10 days) */
void   hs_assign(void) BANKED;                              /* workers take the jobs of the quotas */
void   hs_think(UINT8 i) BANKED;                            /* one worker decides and starts a step or a job */
UINT16 hs_stock(void) BANKED;                               /* the most a store holds */
void   hs_dirty(UINT16 idx) BANKED;
UINT8  hs_can_dig(UINT16 idx) BANKED;                       /* 0 no, 1 yes, 2 needs DEEP DIG */
UINT8  hs_place(UINT8 x, UINT8 y, UINT8 type) BANKED;       /* queue a building (a site) on a tunnel cell: 0 ok, else a message id */
UINT8  hs_upgrade(UINT8 x, UINT8 y) BANKED;                 /* 0 ok, else a message id */
void   hs_cost(UINT8 type, UINT8 lv, UINT16 *c) BANKED;     /* c[0..3] = leaf, ore, bar, crystal for building that level */
UINT8  hs_unlocked(UINT8 type) BANKED;                      /* the building can be placed */
UINT8  hs_popcap(void) BANKED;
UINT8  hs_guardcap(void) BANKED;
UINT16 hs_defence(void) BANKED;                             /* the number the auto-resolved raid is measured against */
UINT16 hs_strength(UINT8 kind) BANKED;
void   hs_raid_auto(void) BANKED;                           /* the pending raid is fought without the player */
void   hs_outpost_raid(void) BANKED;                        /* the attacked outpost is fought by numbers: HV->orp is cleared; returns through HV->stat[2] = 1 won / 2 lost */
UINT8  hs_caravan_here(void) BANKED;                        /* 1 while the caravan stands in the colony */
void   hs_raid_result(UINT8 won) BANKED;                    /* ...or after the defend phase */
UINT32 hs_score(void) BANKED;
void   hs_ach(void) BANKED;                                 /* check the achievements */
UINT8  hs_quest_done(UINT8 q) BANKED;                       /* 1 if quest q is met now */
void   hs_quest_new(UINT8 q) BANKED;
void   hs_quest_claim(UINT8 q) BANKED;
UINT8  hs_start_research(UINT8 t) BANKED;                   /* 0 ok else a message id */
void   hs_tech_cost(UINT8 t, UINT16 *c) BANKED;             /* leaf, ore, bar, crystal, honey */
UINT8  hs_tech_ok(UINT8 t) BANKED;                          /* its branch is open to it */
UINT8  hs_nidle(void) BANKED;
UINT8  hs_connected(void) BANKED;                           /* the queen can be reached from the entrance */
UINT8  hs_prof_at(UINT8 kind, UINT8 r) BANKED;
UINT16 hs_kmask(void) BANKED;                               /* a bit per kind of world visited */
UINT8  hs_seen_count(void) BANKED;                          /* worlds visited */
void   hs_note(UINT8 id) BANKED;
/* ---- hive_map.c (bank 46) ---- */
void   hm_run(void) BANKED;
void   hv_art_load(void) BANKED;
void   hive_save(void) BANKED;                              /* hive.c (bank 45) */                            /* hive_art.c (bank 50) */
void   hm_cell(UINT8 x, UINT8 y) BANKED;                    /* redraw a cell */
void   hm_pals(void) BANKED;                                /* colours: season, kind, time of day */
void   hm_fullmap(void) BANKED;
void   hm_look(UINT8 x, UINT8 y) BANKED;                    /* scroll so that the cell shows */
void   hm_sprites_off(void) BANKED;
void   hm_put_sprite(UINT8 oam, INT16 px, INT16 py, UINT8 tile, UINT8 pal, UINT8 flip) BANKED;   /* px,py in map pixels */
void   hm_cursor_to(UINT8 x, UINT8 y) BANKED;
UINT8  hm_cx(void) BANKED;
UINT8  hm_cy(void) BANKED;
/* ---- hive_ui.c (bank 48): pages. Each runs its own loop with the window as a full page and returns when the player leaves ---- */
void   hu_text(UINT8 id, char *out) BANKED;                 /* message id -> text */
void   hu_hud(UINT8 sel) BANKED;                            /* the three HUD rows on the window (sel: 0 normal) */
void   hu_build_menu(UINT8 x, UINT8 y) BANKED;              /* place a building on this cell */
void   hu_cell_menu(UINT8 x, UINT8 y) BANKED;               /* a building: level, upgrade */
UINT8  hu_main_menu(void) BANKED;                           /* START: pages; returns 1 to leave the hive */
UINT8  hu_intro(void) BANKED;                               /* first time: pick the world and difficulty: kind | diff << 4 */
void   hu_raid_alert(void) BANKED;
void   hu_report(UINT8 won) BANKED;
void   hu_opreport(UINT8 kind, UINT8 won) BANKED;           /* an outpost was raided: its kind of world, held or not */
void   hu2_help(void) BANKED;                               /* hive_ui2.c: the tutorial pages */
void   hu2_awayreport(void) BANKED;                         /* what the colony did while the player was away */                         /* after a raid: what happened */
void   hu_cellinfo(UINT8 x, UINT8 y, char *out) BANKED;     /* the line under the HUD while the cursor sits on a cell */
void   hu_bname(UINT8 type, char *out) BANKED;
void   hu2_trade(void) BANKED;                              /* hive_ui2.c (bank 51) */
void   hu2_outposts(void) BANKED;
void   hu2_link(void) BANKED;
void   hu2_status(void) BANKED;
UINT8  hu2_options(void) BANKED;                            /* 1: start a new colony */
/* ---- hive_def.c (bank 49) ---- */
UINT8  hd_run(void) BANKED;                                 /* the defend phase: 1 won, 0 lost */
/* message ids (hive_ui.c holds the text) */
enum { M_NONE, M_BUILT, M_DUG, M_NOLEAF, M_NOPAY, M_NEEDTECH, M_BLOCKED, M_MAXLV, M_NOCELL, M_SITEFULL, M_NEEDSTORE, M_RAIN, M_STORM, M_SNOW, M_DRY, M_CLEAR,
       M_DROUGHT, M_PLAGUE, M_TRADERS, M_BLOOM, M_RAID, M_WON, M_LOST, M_HATCH, M_STARVE, M_DEEP, M_SEASON0, M_SEASON1, M_SEASON2, M_SEASON3, M_TECHDONE, M_QUESTDONE,
       M_ACH, M_DAY, M_NIGHT, M_FAST, M_NORMAL, M_BOSS, M_LEFT, M_MARKED, M_UNMARK, M_NOWALL, M_UPGR, M_RESEARCH, M_NOTHING, M_GUARDS, M_ROYAL, M_WARN, M_HELP, M_CARAVAN, M_CARAWAY, M_OPWON, M_OPLOST, M_TWOFOLD, M_N };
#endif
