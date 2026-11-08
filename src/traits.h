#ifndef TRAITS_H
#define TRAITS_H
#include <gb/gb.h>
/* version 6: the bug's traits (bank 43). TP_*: what a trait can change, in percent. */
enum { TP_HP, TP_MP, TP_BITE, TP_GOLD, TP_XP, TP_POT, TP_SPELL, TP_N };
INT8   tr_pct(UINT8 what) BANKED;
UINT16 tr_hp(UINT16 base) BANKED;       /* max health with the traits */
UINT16 tr_mp(UINT16 base) BANKED;
UINT16 tr_gold(UINT16 g) BANKED;
UINT16 tr_xp(UINT16 n) BANKED;
UINT16 tr_bite(UINT16 b) BANKED;
UINT16 tr_pot(UINT16 h) BANKED;         /* a potion's healing */
UINT16 tr_spell(UINT16 d) BANKED;       /* a spell's damage (also PRISM EYE) */
void   tr_name(UINT8 t, char *out) BANKED;
void   tr_desc(UINT8 t, char *out) BANKED;
void   tr_roll(UINT32 salt) BANKED;     /* give sv a new gift and quirk */
void   kill_perks(UINT16 *gold, UINT8 *xp) BANKED;   /* the queens' kill tricks */
#endif
