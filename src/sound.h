#ifndef SOUND_H
#define SOUND_H
#include <gb/gb.h>
#include "sound_data.h"
/* step 9: music and sound effects. The player runs in the VBL interrupt from bank 7 (sound_play.c); these calls only leave it a note,
   so they are cheap and safe from any bank. Channel 2 plays the tune, 3 the bass, 1 an echo of the tune (right speaker), 4 the drums.
   Effects borrow channel 1 and/or 4 for a moment. opt.music (0 off, 1 low, 2 full) and opt.sfx are read as it plays. */
enum { SFX_BITE, SFX_HIT, SFX_PICK, SFX_FIRE, SFX_HURT, SFX_STAIRS, SFX_FANFARE, SFX_COIN, SFX_SPLASH, SFX_DIE, SFX_SPARK, SFX_STEP,
       SFX_THUD, SFX_CRUNCH, SFX_HISS, SFX_LASER, SFX_BOOM, SFX_MOVE, SFX_OK, SFX_BACK, SFX_LEVEL, SFX_TYPE, SFX_N };
#define MUS_NONE 255
void sound_init(void);           /* once at boot */
void music(UINT8 track);         /* MUS_TITLE.. (sound_data.h) or MUS_NONE; the same track again carries on where it is */
void sfx(UINT8 n);               /* SFX_* */
void sfx_type(void);             /* the dialogue's typing tick (for dlg_set_blip) */
#endif
