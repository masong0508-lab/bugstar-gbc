#ifndef INPUT_H
#define INPUT_H
#include <gb/gb.h>
extern UINT8 keys, keys_old;
void input_poll(void);
#define HELD(k) (keys & (k))
#define HIT(k)  ((keys & (k)) && !(keys_old & (k)))
#endif
