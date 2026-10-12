#ifndef STUB_GB
#define STUB_GB
#include <stdint.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32; typedef int8_t INT8; typedef int16_t INT16;
#define J_UP 4
#define J_DOWN 8
#define J_LEFT 2
#define J_RIGHT 1
#define J_A 16
#define J_B 32
#define J_START 128
#define J_SELECT 64
#define VBL_IFLAG 1
#define CRITICAL
#define DIV_REG (*(volatile UINT8*)&stub_div)
extern UINT8 stub_div; extern int stub_bank;


UINT8 joypad(void); void wait_vbl_done(void); void gotoxy(UINT8,UINT8); void cls(void);
void add_VBL(void (*)(void)); void set_interrupts(UINT8); void cpu_fast(void);
void waitpad(UINT8); void waitpadup(void);
void move_sprite(UINT8,UINT8,UINT8); void set_sprite_tile(UINT8,UINT8); void set_sprite_data(UINT8,UINT8,const UINT8*);
void set_bkg_data(UINT8,UINT8,const UINT8*); void set_bkg_tiles(UINT8,UINT8,UINT8,UINT8,const UINT8*);
void fill_bkg_rect(UINT8,UINT8,UINT8,UINT8,UINT8);
void set_win_tiles(UINT8,UINT8,UINT8,UINT8,const UINT8*); void fill_win_rect(UINT8,UINT8,UINT8,UINT8,UINT8); void move_win(UINT8,UINT8);
extern UINT8 stub_win;
#define SHOW_WIN (stub_win=1)
#define HIDE_WIN (stub_win=0)
#define DISPLAY_ON
#define DISPLAY_OFF
#define SHOW_BKG
#define SHOW_SPRITES
#define HIDE_SPRITES
extern UINT8 stub_vbk;
#define VBK_REG stub_vbk
#define VBK_BANK_0 0
#define VBK_BANK_1 1
extern UINT8 _cpu;
#endif
