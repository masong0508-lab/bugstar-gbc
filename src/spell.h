#ifndef SPELL_H
#define SPELL_H
#include <gb/gb.h>
/* Spells, dungeon floor events, feats and the difficulty (bank 21). Strings live in bank 21: callers get them copied into RAM. */
#define EX_SPELL  (ex.rsv[0])                       /* low 2 bits: the spell picked; 0x80: the SELECT hint was shown */
#define EX_FEAT   (ex.rsv[1])                       /* rsv[1], rsv[2]: a bit per feat already paid */
#define EX_FISHN  (ex.rsv[3])                       /* fish caught in all */
#define EX_CASTS  (ex.rsv[4])                       /* spells cast */
#define EX_KILLS  (*(UINT16 *)&ex.rsv[5])           /* monsters killed in dungeons */
UINT8 spell_known(UINT8 s) BANKED;
UINT8 spell_cost(UINT8 s) BANKED;
UINT8 spell_next(UINT8 s) BANKED;                   /* the next spell you know */
void  spell_label(UINT8 s, char *out) BANKED;       /* "FIREBALL  5 MP" */
UINT8 diff_dmg(UINT8 n) BANKED;                     /* damage you take after the difficulty */
UINT8 ev_roll(UINT8 depth) BANKED;                  /* floor event 0..6 for this floor of this world */
void  ev_name(UINT8 e, char *out) BANKED;
UINT8 spell_pay(UINT8 s) BANKED;                   /* pays the mana (1), or says why not (0); heals for HEAL */
UINT8 spell_dmg(UINT8 s, UINT8 face, INT16 dx, INT16 dy) BANKED;   /* damage SPARK / FIREBALL does to a monster at that offset (0: a miss) */
void  floor_intro(UINT8 e) BANKED;                  /* the floor's event message and effect, and the one-time SELECT hint */
void  feats_check(void) BANKED;                     /* pays any feat just earned (with a message) */
void  feats_run(void) BANKED;                       /* the full-screen feats page */
#endif
