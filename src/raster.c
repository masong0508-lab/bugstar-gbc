/* BUGSTAR GBC - the title's wavy logo: an HBlank interrupt that slides each scanline of the logo sideways a little (bank 0:
   an interrupt can come while any bank is mapped). See raster.h. */
#include <gb/gb.h>
#include "raster.h"

static UINT8 phase;
static const UINT8 wave[32] = { 0,0,1,1,1,1,1,2,2,2,1,1,1,1,1,0,0,0,255,255,255,255,255,254,254,254,255,255,255,255,255,0 };   /* a slow sine, -2..2 as SCX values */
/* The old version took an HBlank interrupt on all 144 lines of every frame (about half the CPU) to wave 24 of them. Now the LY=LYC
   interrupt (STAT bit 6) wakes it on the line before the logo, it switches to HBlank interrupts (bit 3) for the logo's lines only, and
   after the last one it goes back to waiting for LYC: 26 interrupts a frame instead of 144. Bank 0 is full, so STAT itself says which. */
static void lcd_isr(void){
    if(!(STAT_REG & 8)){ STAT_REG = 8; return; }             /* LYC hit on line WAVE_Y0 - 1: HBlank interrupts from here */
    if(LY_REG >= WAVE_Y1 - 1){ SCX_REG = 0; STAT_REG = 0x40; return; }
    SCX_REG = wave[(LY_REG + 1 + phase) & 31];        /* (what we set now shows from the next line) */
}
void wave_start(void){ LYC_REG = WAVE_Y0 - 1; STAT_REG = 0x40; add_LCD(lcd_isr); IE_REG |= LCD_IFLAG; }   /* (the IE bit goes on after add_LCD and off before remove_LCD, so no LCD interrupt can arrive meanwhile) */
void wave_stop(void){ IE_REG &= ~LCD_IFLAG; remove_LCD(lcd_isr); STAT_REG = 0; SCX_REG = 0; }
void wave_tick(void){ phase++; }

UINT8 road_scx[144];
static void road_isr(void){ UINT8 l = LY_REG + 1; SCX_REG = l < 144 ? road_scx[l] : 0; }
void road_start(void){ STAT_REG = 0x08; add_LCD(road_isr); IE_REG |= LCD_IFLAG; }
void road_stop(void){ IE_REG &= ~LCD_IFLAG; remove_LCD(road_isr); STAT_REG = 0; SCX_REG = 0; }
