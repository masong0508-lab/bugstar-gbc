#include "input.h"
UINT8 keys, keys_old;
void input_poll(void){ keys_old = keys; keys = joypad(); }
