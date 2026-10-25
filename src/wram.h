#ifndef WRAM_H
#define WRAM_H
/* The GBC's work RAM is 32 KB: C000-CFFF is always there, and D000-DFFF shows one of 7 banks (SVBK, 1..7).
   The game's variables and the stack live in C000-CFFF (the stack starts at D000 and grows down: see the Makefile's
   -Wl-g.STACK=0xD000; tools/check_banks.py checks the variables leave it room). The big buffers sit at fixed places in
   WRAM bank 1, which is always the one selected, so they cost the always-there RAM nothing:
     D000  the bug canvas (bug.c)            1824
     D720  the fades' palette copies (fx.c)   256
     D820  a dungeon floor map (dungeon.c)   1024
     DC20  the stars of space (space.c)       512
     DE20  the bug creator's second scratch   480  (WRAM_MAKE2)
   WRAM banks 2..7 (24 KB) are free for later: switch with wram_bank(n), and switch back to 1 before anything above is used.
   Nothing in an interrupt touches these. */
#ifdef __SDCC
#include <gb/gb.h>
#define WRAM_AT(addr, type) ((type *)(addr))
#define wram_bank(n) (SVBK_REG = (n))
#else
static unsigned char wram_host[4096];                    /* (the PC host tests: a plain array instead) */
#define WRAM_AT(addr, type) ((type *)(wram_host + ((addr) - 0xD000)))
#define wram_bank(n)
#endif
#define WRAM_BPX   0xD000
#define WRAM_MAKE  0xD720   /* the bug creator's scratch (bigbug.c, bugmake.c): shares the fades' copies, which are never in use while it runs */
#define WRAM_FADE  0xD720
#define WRAM_DMAP  0xD820
#define WRAM_STARS 0xDC20
#define WRAM_MAKE2 0xDE20
#endif
