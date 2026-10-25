#ifndef WARP_H
#define WARP_H
#include <gb/gb.h>
/* The warp (bank 9): a 3D rail shooter down a hyperspace tunnel. Call with the LCD off; it fades itself in, plays for
   `frames` frames and ends on a white flash with the LCD on (the caller fades out and rebuilds its screen).
   You steer the ship around the screen and shoot into it; meteors and hot star shards fly at you out of the depths
   (they are aimed where you are, so keep moving), and now and then (not every warp) pirates jump in for a hyperspace attack.
   The 3D: every thing is a point (x, y, z), drawn at 80 + x * 64 / z (the divide is a multiply by a 4096 / z table),
   bigger as it comes near; your shots fly away from you in z and hit what they meet at the same depth. */
typedef struct {
    UINT16 gold;                 /* earned on the way */
    UINT8  rocks, pirates;       /* meteors and pirates shot */
    UINT8  hits;                 /* times you were hit */
    UINT8  attacked;             /* 1 if pirates came */
    UINT8  wrecked;              /* 1 if the HP ran out (you still arrive, battered) */
} WarpResult;
void warp_run(UINT16 frames, WarpResult *res) BANKED;
void warp_report(WarpResult *r) BANKED;   /* Bolt tells how the warp went (and XP for what you shot) */
enum { SAY_HOWTO, SAY_BEAST, SAY_DAWN, SAY_HOW3, SAY_BEAST3 };
void space_say(UINT8 id) BANKED;          /* space's longer talk, kept here in bank 9 (bank 6 is full) */
#endif
