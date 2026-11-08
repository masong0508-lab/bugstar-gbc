#ifndef WEATHER_H
#define WEATHER_H
#include <gb/gb.h>
#include <gb/cgb.h>
/* version 6: WEATHER on the planets (bank 44). What falls depends on the kind of world, the world and the day (480 s of play).
   Six sprites (OAM 22..27, tiles 144.. in VRAM bank 1, sprite palette 1) fall, drift or twinkle; fog, dust and storms also bend the colours,
   and a storm flashes. The weather is only on the planet: it never touches the dungeon or space. */
enum { WX_CLEAR, WX_RAIN, WX_STORM, WX_SNOW, WX_ASH, WX_SPORES, WX_DUST, WX_SHARDS, WX_FOG, WX_CONFETTI, WX_N };
#define WX_TILE0 144
#define WX_OAM0  22
#define WX_NP    6
extern UINT8 wx_type, wx_flash;      /* today's weather, and the frames a lightning flash has left */
void weather_start(UINT8 wk, UINT8 star) BANKED;       /* choose today's weather for this world */
void weather_gfx(void) BANKED;                        /* the particle tiles into VRAM bank 1 (call with bank 1 selected) */
void weather_step(UINT8 tick) BANKED;                 /* once a frame */
void weather_draw(void) BANKED;                       /* move the sprites */
void weather_hide(void) BANKED;
void weather_pal(palette_color_t *p, UINT8 n) BANKED; /* bend the world's colours */
void weather_name(char *out) BANKED;                  /* "RAIN", "SNOWFALL" ... */
#endif
