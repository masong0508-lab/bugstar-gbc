#ifndef WARSHIP_H
#define WARSHIP_H
#include <gb/gb.h>
/* WARSHIP RAID (bank 31), the GBC's simplified version of the GBA's colossal warships: a warship hangs over a world and its planet-buster is counting down.
   Fly under it, shoot every gun, then the core, before the clock runs out. Three sizes (2, 4 and 6 guns); each needs the one before it beaten first.
   Your ship's parts count: LASER (damage), GUNS (fire rate), ENGINE (speed), HULL and SHIELD (hits you can take). Pays gold, XP and fame, and keeps the best times
   in its own SRAM block (offset 3584 of the slot's bank, own magic and checksum, so older saves just start empty). Called from EXTRAS; the LCD is off on entry and exit. */
#define WAR_OPEN (sv.level >= 6)
void   warship_run(void) BANKED;                  /* the size menu, then the raids */
UINT8  warship_info(char *out) BANKED;            /* a line for the EXTRAS card ("RAIDS 1/3"); returns how many sizes are beaten */
void   warship_reset(void) BANKED;                /* forget the records (the EXTRAS reset) */
#endif
