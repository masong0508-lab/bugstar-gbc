/* BUGSTAR GBC - the title's wavy logo: an HBlank interrupt that slides each scanline of the logo sideways a little (bank 0:
   an interrupt can come while any bank is mapped). See raster.h. */
#include <gb/gb.h>
#include "raster.h"

#include <gb/isr.h>
/* Both effects (the title's wavy logo and KART WORLD's road) are raw STAT interrupt handlers (ISR_VECTOR), not add_LCD ones. GBDK's own LCD
   dispatcher ends with a loop that waits for the next HBlank when the handler is not done by the end of this one: on a busy road line that
   was a whole scanline lost per interrupt, and the game got a quarter of the CPU. A raw handler returns the moment it has written SCX. */
static UINT8 phase, rmode;                 /* rmode: 0 nothing, 1 the logo's wave, 2 the road */
static const UINT8 wave[32] = { 0,0,1,1,1,1,1,2,2,2,1,1,1,1,1,0,0,0,255,255,255,255,255,254,254,254,255,255,255,255,255,0 };   /* a slow sine, -2..2 as SCX values */
/* The title: the LY=LYC interrupt (STAT bit 6) wakes it on the line before the logo, it switches to HBlank interrupts (bit 3) for the logo's lines only, and
   after the last one it goes back to waiting for LYC: 26 interrupts a frame instead of 144. */
UINT8 road_scx[144];
UINT16 road_cnt; static UINT8 rph;
/* The road: the sky is one scroll value, so a LYC interrupt on line 14 switches to HBlank interrupts for lines 14 and 15 (the HUD rows end, the sky starts),
   a LYC interrupt on line 62 switches them on again for the road (lines 62..143), and the rest of the frame has none. */
static void stat_isr(void) __critical __interrupt {
    UINT8 l;
    if(rmode == 1){
        if(!(STAT_REG & 8)){ STAT_REG = 8; return; }             /* LYC hit on line WAVE_Y0 - 1: HBlank interrupts from here */
        if(LY_REG >= WAVE_Y1 - 1){ SCX_REG = 0; STAT_REG = 0x40; return; }
        SCX_REG = wave[(LY_REG + 1 + phase) & 31];        /* (what we set now shows from the next line) */
        return;
    }
    switch(rph){
    case 0: STAT_REG = 8; rph = 1; return;
    case 1: SCX_REG = road_scx[15]; rph = 2; return;
    case 2: SCX_REG = road_scx[16]; STAT_REG = 0x40; LYC_REG = 62; rph = 3; return;
    case 3: STAT_REG = 8; rph = 4; return;
    default: l = LY_REG + 1; if(l < 144) SCX_REG = road_scx[l]; else { SCX_REG = 0; STAT_REG = 0x40; LYC_REG = 14; rph = 0; }
    }
}
ISR_VECTOR(VECTOR_STAT, stat_isr)
void wave_start(void){ rmode = 1; LYC_REG = WAVE_Y0 - 1; STAT_REG = 0x40; IE_REG |= LCD_IFLAG; }
void wave_stop(void){ IE_REG &= ~LCD_IFLAG; rmode = 0; STAT_REG = 0; SCX_REG = 0; }
void wave_tick(void){ phase++; }
void road_start(void){ rmode = 2; rph = 0; LYC_REG = 14; STAT_REG = 0x40; IE_REG |= LCD_IFLAG; }
void road_stop(void){ IE_REG &= ~LCD_IFLAG; rmode = 0; STAT_REG = 0; SCX_REG = 0; rph = 0; }
