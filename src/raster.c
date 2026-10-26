/* BUGSTAR GBC - the title's wavy logo: an HBlank interrupt that slides each scanline of the logo sideways a little (bank 0:
   an interrupt can come while any bank is mapped). See raster.h. */
#include <gb/gb.h>
#include "raster.h"

static UINT8 phase;
static const UINT8 wave[32] = { 0,0,1,1,1,1,1,2,2,2,1,1,1,1,1,0,0,0,255,255,255,255,255,254,254,254,255,255,255,255,255,0 };   /* a slow sine, -2..2 as SCX values */
static void lcd_isr(void){
    UINT8 ly = LY_REG + 1;                                   /* (what we set now shows from the next line) */
    SCX_REG = (ly >= WAVE_Y0 && ly < WAVE_Y1) ? wave[(UINT8)(ly + phase) & 31] : 0;
}
void wave_start(void){ CRITICAL { STAT_REG = 0x08; add_LCD(lcd_isr); } set_interrupts(IE_REG | LCD_IFLAG); }
void wave_stop(void){ set_interrupts(IE_REG & ~LCD_IFLAG); CRITICAL { remove_LCD(lcd_isr); STAT_REG = 0; } SCX_REG = 0; }
void wave_tick(void){ phase++; }

UINT8 road_scx[144];
static void road_isr(void){ UINT8 l = LY_REG + 1; SCX_REG = l < 144 ? road_scx[l] : 0; }
void road_start(void){ CRITICAL { STAT_REG = 0x08; add_LCD(road_isr); } set_interrupts(IE_REG | LCD_IFLAG); }
void road_stop(void){ set_interrupts(IE_REG & ~LCD_IFLAG); CRITICAL { remove_LCD(road_isr); STAT_REG = 0; } SCX_REG = 0; }
