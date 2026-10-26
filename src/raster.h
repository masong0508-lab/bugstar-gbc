#ifndef RASTER_H
#define RASTER_H
#include <gb/gb.h>
#define WAVE_Y0 8                /* the scanlines of the title logo */
#define WAVE_Y1 32
void wave_start(void);           /* the logo starts to wave (title screen) */
void wave_stop(void);            /* ...and stops: call before leaving the title */
void wave_tick(void);            /* once every few frames: moves the wave along */
/* KART WORLD's road: an HBlank interrupt sets each scanline's sideways scroll from road_scx[line] (the road bends in perspective) */
extern UINT8 road_scx[144];
void road_start(void);
void road_stop(void);
#endif
