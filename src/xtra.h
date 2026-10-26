#ifndef XTRA_H
#define XTRA_H
#include <gb/gb.h>
/* the small EXTRAS (bank 26): MINING, the station LOUNGE, RELICS, DEBUG. Block saved at SRAM offset 432 of the slot (own seed + sum, so a new game starts empty). */
UINT8 xtra_relics(void) BANKED;            /* unlock what is earned, return how many relics you hold */
void  xtra_relic_screen(void) BANKED;
void  xtra_mine(void) BANKED;
void  xtra_lounge(void) BANKED;
UINT8 xtra_dbg_open(UINT8 unlock) BANKED;  /* 1 when the DEBUG menu is open (unlock: 1 opens it now) */
void  xtra_debug(void) BANKED;
#endif
