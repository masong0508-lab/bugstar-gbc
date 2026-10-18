#ifndef HUD_H
#define HUD_H
#include <gb/gb.h>
/* Window-layer HUD (step 4b): two tile rows pinned to the bottom of the screen. The window does not scroll,
   so the map can scroll freely above it. Row 0: heart, HP bar + number, level. Row 1: coin, gold, star, MP bar + number.
   The bar turns red below 25% HP. The HUD reads sv (save.h), so just change sv.* and call hud_update(). */
#define HUD_ROWS 2
#define HUD_Y    (144 - HUD_ROWS * 8)      /* screen pixel row where the window starts */
UINT16 hud_hpmax(UINT16 level);            /* placeholder stat curve until the bug stats step: 10 at level 1 */
UINT16 hud_mpmax(UINT16 level);            /* 5 at level 1 */
void hud_init(void);                       /* draw everything and show the window (LCD may be on, drawing is VBL-safe) */
void hud_redraw(void);                     /* draw everything into the window map without moving or showing it */
UINT8 hud_is_on(void);                     /* 1 between hud_show/hud_init and hud_hide */
void hud_update(void);                     /* redraw only what changed since last call: cheap, call every frame */
void hud_pos(UINT8 y);                     /* slide the window to pixel row y (HUD_Y = docked, 144 = off screen) */
void hud_show(void);
void gain_xp(UINT16 n);                    /* add XP; levels up (12 x level each) with a message, refilling HP and MP */
void hud_hide(void);
#endif
