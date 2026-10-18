#ifndef FX_H
#define FX_H
#include <gb/gb.h>
/* Style (the polish pass): fades between scenes, a WARNING cut-in, screen shake. fx.c is in bank 7; shake lives in hud.c (bank 0). */
void fade_in(void) BANKED;       /* call with the LCD off and the new scene's palettes set: turns the LCD on and brings them up from black */
void fade_out(void) BANKED;      /* the screen goes to black (nothing if the LCD is off); then turn the LCD off and build the next scene */
enum { BN_BEAST, BN_QUEEN, BN_ATTACK };
void banner(UINT8 id) BANKED;    /* a flashing WARNING band across the middle of the screen, ~1.5 s; the HUD comes back after */
extern INT8 shake_x, shake_y;    /* add these to SCX / SCY each frame */
void shake(UINT8 frames);        /* start a shake (OPTIONS: SHAKE off makes it do nothing) */
void shake_tick(void);           /* once a frame: moves shake_x / shake_y */
#endif
