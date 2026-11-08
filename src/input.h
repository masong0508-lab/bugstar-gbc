#ifndef INPUT_H
#define INPUT_H
#include <gb/gb.h>
extern UINT8 keys, keys_old, keys_rep;
void input_poll(void);
#define HELD(k) (keys & (k))
#define HIT(k)  ((keys & (k)) && !(keys_old & (k)))
/* HITR: like HIT, but a held direction repeats (after 20 frames, then every 4): use it for menu cursors and value changes. Buttons never repeat. */
#define HITR(k) (keys_rep & (k))
#endif
