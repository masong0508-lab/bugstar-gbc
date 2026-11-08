#include "input.h"
#include "text.h"
UINT8 keys, keys_old, keys_rep;
static UINT8 rep_t;
void input_poll(void){
    keys_old = keys; keys = joypad();
    keys_rep = keys & ~keys_old;
    if(keys & 0x0F){                                  /* a held direction repeats: after 20 frames, every 4th */
        if(keys == keys_old){ if(rep_t < 255) rep_t++; if(rep_t >= 20 && !((rep_t - 20) & 3)) keys_rep |= keys & 0x0F; }
        else rep_t = 0;
    } else rep_t = 0;
    if(txt_soft_on) txt_soft_end();                   /* a screen that started a soft redraw is shown now, whatever else the caller forgot */
}
