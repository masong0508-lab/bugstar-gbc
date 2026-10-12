#ifndef STUB_CGB
#define STUB_CGB
#include <gb/gb.h>
typedef UINT16 palette_color_t;
#define RGB(r,g,b) ((((UINT16)(b)&0x1f)<<10)|(((UINT16)(g)&0x1f)<<5)|((UINT16)(r)&0x1f))
#define CGB_TYPE 0x11
void set_bkg_palette(UINT8,UINT8,const palette_color_t*); void set_sprite_palette(UINT8,UINT8,const palette_color_t*);
#endif
