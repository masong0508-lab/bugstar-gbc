#ifndef MENUS_H
#define MENUS_H
#include <gb/gb.h>
/* thin helpers on top of the text engine (text.h) */
void  put_line(UINT8 x, UINT8 y, const char *s, UINT8 w);   /* print s at x,y, space-padded to exactly w columns */
char *u16s(char *d, UINT16 v);            /* decimal; returns pointer to the end NUL */
char *u32s(char *d, UINT32 v);           /* decimal 32-bit; returns pointer to the end NUL */
char *scat(char *d, const char *s);       /* copy; returns pointer to the end NUL */
char *fmt_time(char *d, UINT32 secs);     /* H:MM:SS */
char *fmt_hex32(char *d, UINT32 v);       /* 8 hex digits */
extern const char *const kind_name[6];    /* GARDEN DESERT ICE JUNGLE OCEAN DUSK: the kinds of world (same order as the planets) */
UINT8 world_kind(UINT32 wseed);           /* the kind of the world with that seed (world_seed() in save.h) */
char *world_name(char *d, UINT32 wseed);  /* its name, two or three made-up syllables, e.g. ZORBAX; returns the end NUL */
void  options_screen(void) BANKED;              /* bank 2 */
/* mode 0 = LOAD (used slots only), 1 = NEW (asks before overwriting). Returns slot 0..2, or -1 for back. */
int   slot_picker(UINT8 mode) BANKED;   /* bank 2 */
#endif
