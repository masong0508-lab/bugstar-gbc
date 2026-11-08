#pragma bank 43
/* BUGSTAR GBC - version 6: the bug's TRAITS. Every bug is born with a GIFT and a QUIRK (sv.gift, sv.quirk, 1..15): small, real effects on health, mana,
   bite, gold, XP, potions and spells. The queens' tricks (feat.c) that work on kills (RED DUST, SPORE CLOUD, PRISM EYE, HOT BLOOD, BOG LEECH, SUGAR RUSH)
   are here too. Everything is a percent; a trait can also cost a little. */
#include <gb/gb.h>
#include <string.h>
#include "save.h"
#include "feat.h"
#include "hud.h"
#include "traits.h"

typedef struct { const char *nm, *ds; INT8 p[TP_N]; } Trait;        /* p: hp mp bite gold xp potion spell */
static const Trait tt[16] = {
    { "NONE",       "NO TRAIT YET",           {  0,  0,  0,  0,  0,  0,  0 } },
    { "HARDY",      "+15% HEALTH",            { 15,  0,  0,  0,  0,  0,  0 } },
    { "BRAINY",     "+30% MANA",              {  0, 30,  0,  0,  0,  0,  0 } },
    { "STRONG",     "+15% BITE",              {  0,  0, 15,  0,  0,  0,  0 } },
    { "THRIFTY",    "+12% GOLD",              {  0,  0,  0, 12,  0,  0,  0 } },
    { "CLEVER",     "+12% XP",                {  0,  0,  0,  0, 12,  0,  0 } },
    { "HEALTHY",    "+25% POTIONS",           {  0,  0,  0,  0,  0, 25,  0 } },
    { "ARCANE",     "+20% SPELLS",            {  0,  0,  0,  0,  0,  0, 20 } },
    { "GRITTY",     "+8% HEALTH, +8% BITE",   {  8,  0,  8,  0,  0,  0,  0 } },
    { "SHARP",      "+10% BITE, +6% GOLD",    {  0,  0, 10,  6,  0,  0,  0 } },
    { "SCHOLAR",    "+8% XP, +15% MANA",      {  0, 15,  0,  0,  8,  0,  0 } },
    { "GREEDY",     "+20% GOLD, -5% HEALTH",  { -5,  0,  0, 20,  0,  0,  0 } },
    { "BRAVE",      "+20% BITE, -8% HEALTH",  { -8,  0, 20,  0,  0,  0,  0 } },
    { "FRAGILE",    "+25% GOLD, -10% HEALTH", {-10,  0,  0, 25,  0,  0,  0 } },
    { "PATIENT",    "+20% XP, -5% GOLD",      {  0,  0,  0, -5, 20,  0,  0 } },
    { "WILD",       "+30% BITE, -15% HEALTH", {-15,  0, 30,  0,  0,  0,  0 } } };

INT8 tr_pct(UINT8 what) BANKED {
    INT16 v = (INT16)tt[sv.gift & 15].p[what] + tt[sv.quirk & 15].p[what];
    return (INT8)v;
}
static UINT16 scale(UINT16 v, INT8 p){          /* v + v * p / 100, without 32-bit multiplies for the common sizes */
    if(!p) return v;
    if(v < 600) return (UINT16)((INT16)v + (INT16)((INT16)v * p / 100));
    return (UINT16)((INT16)v + (INT16)((INT16)(v / 8) * p / 12));
}
UINT16 tr_hp(UINT16 base) BANKED { return scale(base, tr_pct(TP_HP)); }
UINT16 tr_mp(UINT16 base) BANKED { return scale(base, tr_pct(TP_MP)); }
UINT16 tr_gold(UINT16 g) BANKED { return scale(g, tr_pct(TP_GOLD)); }
UINT16 tr_xp(UINT16 n) BANKED { return scale(n, tr_pct(TP_XP)); }
UINT16 tr_bite(UINT16 b) BANKED { return scale(b, tr_pct(TP_BITE)); }
UINT16 tr_pot(UINT16 h) BANKED { return scale(h, tr_pct(TP_POT)); }
UINT16 tr_spell(UINT16 d) BANKED { return scale(d, tr_pct(TP_SPELL)) + (TRICK(8) ? d / 4 : 0); }
void tr_name(UINT8 t, char *out) BANKED { strcpy(out, tt[t & 15].nm); }
void tr_desc(UINT8 t, char *out) BANKED { strcpy(out, tt[t & 15].ds); }
void tr_roll(UINT32 salt) BANKED {
    UINT32 h = salt * 2654435761UL; h ^= h >> 15; h *= 2246822519UL; h ^= h >> 13;
    sv.gift = 1 + (UINT8)((h >> 4) % 15); sv.quirk = 1 + (UINT8)((h >> 12) % 15);
    if(sv.quirk == sv.gift) sv.quirk = sv.quirk % 15 + 1;
}
/* a kill: the queens' tricks that pay on it */
void kill_perks(UINT16 *gold, UINT8 *xp) BANKED {
    if(TRICK(6)) *gold += *gold / 4;                                    /* RED DUST */
    if(TRICK(11)) *xp += *xp / 5 + 1;                                   /* SUGAR RUSH */
    if(TRICK(9)){ UINT16 mx = hud_hpmax(sv.level); if(sv.hp < mx) sv.hp++; }                    /* HOT BLOOD */
    if(TRICK(10)){ UINT16 mx = hud_mpmax(sv.level); if(sv.mp < mx) sv.mp++; }                   /* BOG LEECH */
}
