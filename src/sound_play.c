#pragma bank 7
/* BUGSTAR GBC - step 9: the sound player, once a frame from the VBL interrupt (sound.c maps this bank first).
   A track is 64 steps; every `tempo` frames one step plays: the tune on channel 2, the bass on the wave channel 3, the tune again
   three steps late and quieter on channel 1 (an echo, right speaker only, so it sounds wide), and a drum on the noise channel 4.
   An effect takes channel 1 and/or 4 for a few frames; the tune leaves them alone until it is done. */
#include <gb/gb.h>
#include "save.h"
#include "sound.h"

extern volatile UINT8 snd_song, snd_restart, snd_req;
static UINT8 step, wait, busy1, busy4, muted, waved;
static const UINT8 wave[16] = { 0x02, 0x46, 0x8A, 0xCE, 0xFF, 0xFE, 0xEC, 0xA8, 0x64, 0x20, 0x00, 0x01, 0x23, 0x45, 0x67, 0x9B };   /* a soft, round bass */

/* GBA-style register pairs (the GBA's sound chip is the GB's): cnt_h = NRx1 | NRx2 << 8, cnt_x = NRx3 | NRx4 << 8 */
static void s1(UINT8 sweep, UINT16 h, UINT16 x, UINT8 frames){
    NR10_REG = sweep; NR11_REG = (UINT8)h; NR12_REG = (UINT8)(h >> 8); NR13_REG = (UINT8)x; NR14_REG = (UINT8)(x >> 8);
    busy1 = frames; NR51_REG = 0xFF;                 /* both speakers while it plays */
}
static void s4(UINT16 l, UINT16 h, UINT8 frames){ NR41_REG = (UINT8)l; NR42_REG = (UINT8)(l >> 8); NR43_REG = (UINT8)h; NR44_REG = (UINT8)(h >> 8); busy4 = frames; }

static void play_sfx(UINT8 n){
    switch(n){
    case SFX_BITE:    s4((0xA << 12) | (1 << 8), 0x8000 | (3 << 4) | 1, 10); break;
    case SFX_HIT:     s1((2 << 4) | (1 << 3) | 3, (0xC << 12) | (1 << 8) | (2 << 6), 0x8000 | 1750, 14); break;
    case SFX_PICK:    s1((1 << 4) | 2, (0xB << 12) | (2 << 8) | (2 << 6), 0x8000 | 1800, 16); break;
    case SFX_FIRE:    s4((0xF << 12) | (3 << 8), 0x8000 | (5 << 4) | 3, 24); s1((3 << 4) | (1 << 3) | 2, (0x9 << 12) | (3 << 8) | (1 << 6), 0x8000 | 1400, 24); break;
    case SFX_HURT:    s1((3 << 4) | (1 << 3) | 2, (0xF << 12) | (2 << 8) | (3 << 6), 0x8000 | 1100, 20); break;
    case SFX_STAIRS:  s1((7 << 4) | (1 << 3) | 1, (0xC << 12) | (5 << 8) | (2 << 6), 0x8000 | 1700, 36); break;
    case SFX_FANFARE: s1((6 << 4) | 1, (0xD << 12) | (6 << 8) | (2 << 6), 0x8000 | 1550, 44); break;
    case SFX_COIN:    s1(0, (0xA << 12) | (1 << 8) | (2 << 6), 0x8000 | 1930, 12); break;
    case SFX_SPLASH:  s4((0xB << 12) | (4 << 8), 0x8000 | (4 << 4) | 2, 30); break;
    case SFX_DIE:     s4((0xE << 12) | (2 << 8), 0x8000 | (6 << 4) | 4, 20); break;
    case SFX_SPARK:   s4((0xF << 12) | (1 << 8), 0x8000 | (1 << 4), 10); s1((1 << 4) | (1 << 3) | 1, (0xE << 12) | (1 << 8), 0x8000 | 1980, 14); break;
    case SFX_STEP:    s4((0x3 << 12) | (1 << 8), 0x8000 | (6 << 4) | 5, 4); break;
    case SFX_THUD:    s4((0xD << 12) | (2 << 8), 0x8000 | (7 << 4) | 6, 16); s1((2 << 4) | (1 << 3) | 4, (0xA << 12) | (2 << 8) | (2 << 6), 0x8000 | 900, 16); break;
    case SFX_CRUNCH:  s4((0xF << 12) | (3 << 8), 0x8000 | (4 << 4) | (1 << 3) | 2, 22); break;
    case SFX_HISS:    s4((0x6 << 12) | (2 << 8), 0x8000 | (2 << 4) | (1 << 3) | 1, 14); break;
    case SFX_LASER:   s1((1 << 4) | (1 << 3) | 2, (0xB << 12) | (1 << 8) | (2 << 6), 0x8000 | 1950, 12); break;
    case SFX_BOOM:    s4((0xF << 12) | (5 << 8), 0x8000 | (7 << 4) | 3, 44); break;
    case SFX_MOVE:    s1(0, (0x6 << 12) | (1 << 8) | (2 << 6), 0x8000 | 1880, 6); break;
    case SFX_OK:      s1((1 << 4) | 3, (0xA << 12) | (1 << 8) | (2 << 6), 0x8000 | 1800, 12); break;
    case SFX_BACK:    s1((1 << 4) | (1 << 3) | 3, (0x9 << 12) | (1 << 8) | (2 << 6), 0x8000 | 1800, 12); break;
    case SFX_LEVEL:   s1((5 << 4) | 2, (0xD << 12) | (4 << 8) | (2 << 6), 0x8000 | 1600, 50); break;
    case SFX_TYPE:    s1(0, (0x4 << 12) | (1 << 8) | (1 << 6), 0x8000 | 1990, 3); break;
    }
}

static void quiet(void){ NR22_REG = 0; NR30_REG = 0; if(!busy1) NR12_REG = 0; if(!busy4) NR42_REG = 0; }

void snd_tick(void){
    const SndTrack *t; UINT8 m, v; UINT16 f;
    if(busy1 && !--busy1) NR51_REG = 0xEF;           /* the echo goes back to the right speaker */
    if(busy4) busy4--;
    if(snd_req != 255){ play_sfx(snd_req); snd_req = 255; }
    if(!waved){ UINT8 i; NR30_REG = 0; for(i = 0; i < 16; i++) ((volatile UINT8 *)0xFF30)[i] = wave[i]; waved = 1; }
    if(snd_restart){ snd_restart = 0; step = 0; wait = 1; quiet(); }
    if(snd_song >= MUS_N || !opt.music){ if(!muted){ quiet(); muted = 1; } return; }
    muted = 0;
    if(--wait) return;
    t = &snd_tracks[snd_song]; wait = t->tempo;
    v = opt.music == 1 ? (t->vol >> 1) : t->vol;
    m = t->mel[step];                                  /* the tune */
    if(m == 0) NR22_REG = 0;
    else if(m > 1){ f = snd_sq[m]; NR21_REG = 0x80; NR22_REG = (v << 4) | 2; NR23_REG = (UINT8)f; NR24_REG = 0x80 | (UINT8)(f >> 8); }
    m = t->bas[step];                                  /* the bass */
    if(m == 0) NR32_REG = 0;
    else if(m > 1){ f = snd_wv[m]; NR30_REG = 0x80; NR32_REG = opt.music == 1 ? 0x40 : 0x20; NR33_REG = (UINT8)f; NR34_REG = 0x80 | (UINT8)(f >> 8); }
    if(!busy1){                                        /* the echo */
        m = t->mel[(UINT8)(step - 3) & 63];
        if(m == 0) NR12_REG = 0;
        else if(m > 1){ f = snd_sq[m]; NR10_REG = 0; NR11_REG = 0x40; NR12_REG = ((v / 3 + 1) << 4) | 3; NR13_REG = (UINT8)f; NR14_REG = 0x80 | (UINT8)(f >> 8); }
    }
    if(!busy4){                                        /* the drums */
        m = t->drm[step & 15];
        if(m == 1){ NR41_REG = 0; NR42_REG = opt.music == 1 ? 0x61 : 0xB1; NR43_REG = 0x71; NR44_REG = 0x80; }
        else if(m == 2){ NR41_REG = 0; NR42_REG = opt.music == 1 ? 0x52 : 0x92; NR43_REG = 0x42; NR44_REG = 0x80; }
        else if(m == 3){ NR41_REG = 0; NR42_REG = opt.music == 1 ? 0x21 : 0x41; NR43_REG = 0x10; NR44_REG = 0x80; }
    }
    step = (step + 1) & 63;
}
