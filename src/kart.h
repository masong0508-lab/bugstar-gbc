#ifndef KART_H
#define KART_H
#include <gb/gb.h>
/* KART WORLD (bank 24): a 3D kart race in EXTRAS. 3 laps, 3 rivals. The road is a picture in perspective whose lines slide sideways
   (an HBlank interrupt, src/raster.c) to make the bends; karts and trees are sprites that grow as they come near.
   Pays gold for the place you finish in; the best lap is kept in ex.rsv (EX_KART in spell.h). The LCD is off on entry and exit. */
void kart_run(void) BANKED;
#endif
