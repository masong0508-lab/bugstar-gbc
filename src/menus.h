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
void  options_screen(void);
/* mode 0 = LOAD (used slots only), 1 = NEW (asks before overwriting). Returns slot 0..2, or -1 for back. */
int   slot_picker(UINT8 mode);
#endif
