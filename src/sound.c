/* BUGSTAR GBC - step 9: the bank-0 side of the sound player (see sound.h and sound_play.c) */
#include <gb/gb.h>
#include "save.h"
#include "sound.h"

volatile UINT8 snd_song = MUS_NONE, snd_restart = 1, snd_req = 255;
void snd_tick(void);                                  /* sound_play.c, bank 7: called with bank 7 mapped */

static void snd_isr(void){ UINT8 b = _current_bank; SWITCH_ROM(7); snd_tick(); SWITCH_ROM(b); }
void sound_init(void){ NR52_REG = 0x80; NR50_REG = 0x77; NR51_REG = 0xEF; add_VBL(snd_isr); }
void music(UINT8 n){ if(n != snd_song){ snd_song = n; snd_restart = 1; } }
void sfx(UINT8 n){ if(opt.sfx) snd_req = n; }
void sfx_type(void){ if(opt.sfx && snd_req == 255) snd_req = SFX_TYPE; }
