/* BUGSTAR GBC - step 9: the bank-0 side of the sound player (see sound.h and sound_play.c) */
#include <gb/gb.h>
#include "save.h"
#include "sound.h"
#include "rumble.h"

volatile UINT8 snd_song = MUS_NONE, snd_restart = 1, snd_req = 255;
void snd_tick(void);                                  /* sound_play.c, bank 7: called with bank 7 mapped */

const UINT8 *snd_pm, *snd_pb, *snd_pd; UINT8 snd_bk, snd_s0, snd_s1, snd_s2, snd_o[4];
void snd_fetch(void){                                 /* reads one step of a tune out of its own bank (tunes live in banks 7 and 42; the player's code is in 7) */
    SWITCH_ROM(snd_bk); snd_o[0] = snd_pm[snd_s0]; snd_o[1] = snd_pb[snd_s0]; snd_o[2] = snd_pm[snd_s1]; snd_o[3] = snd_pd[snd_s2]; SWITCH_ROM(7);
}
static void snd_isr(void){ UINT8 b = _current_bank; SWITCH_ROM(7); snd_tick(); SWITCH_ROM(b); }
void sound_init(void){ NR52_REG = 0x80; NR50_REG = 0x77; NR51_REG = 0xEF; add_VBL(snd_isr); }
void music(UINT8 n){ if(n != snd_song){ snd_song = n; snd_restart = 1; } }
void sfx(UINT8 n){ rumble_sfx(n); if(opt.sfx) snd_req = n; }   /* (the motor does not care whether the sound is on) */
void sfx_type(void){ if(opt.sfx && snd_req == 255) snd_req = SFX_TYPE; }
