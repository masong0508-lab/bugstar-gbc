#pragma bank 23
/* BUGSTAR GBC - labyrinth floors (from the GBA game): a real maze of one-wide passages with a chamber in each corner and one in the middle.
   dm = the 32x32 floor map (all wall on entry), rooms = 5 x {x, y, w, h}; the stairs up are in rooms[0], the stairs down in rooms[4]. */
#include <gb/gb.h>
#include <string.h>
#include "lab.h"

static UINT16 s;
static UINT8 rr(void){ s ^= s << 7; s ^= s >> 9; s ^= s << 8; return (UINT8)s; }

void lab_make(UINT8 *dm, UINT8 *rooms, UINT16 seed, UINT8 wall, UINT8 f0, UINT8 f1) BANKED {
    UINT8 stack[225], sp = 0, x, y, i; static const INT8 dx[4] = { 0, 2, 0, -2 }, dy[4] = { -2, 0, 2, 0 };
    static const UINT8 R[20] = { 1, 1, 3, 3,   27, 1, 3, 3,   1, 27, 3, 3,   13, 13, 5, 5 };     /* chambers 0..3; the 5th is made below */
    s = seed ? seed : 1;
    memset(dm, wall, 32 * 32);
    x = 1; y = 1; dm[y * 32 + x] = f0; stack[sp++] = 0;                  /* depth first: cells sit on odd squares, the walls between on even ones */
    for(;;){
        UINT8 opt[4], n = 0, d;
        for(d = 0; d < 4; d++){ INT8 nx = (INT8)x + dx[d], ny = (INT8)y + dy[d]; if(nx > 0 && ny > 0 && nx < 31 && ny < 31 && dm[ny * 32 + nx] == wall) opt[n++] = d; }
        if(n){ d = opt[rr() % n]; dm[((y + y + dy[d]) >> 1) * 32 + ((x + x + dx[d]) >> 1)] = (rr() & 3) ? f0 : f1; x += dx[d]; y += dy[d]; dm[y * 32 + x] = (rr() & 3) ? f0 : f1; stack[sp++] = d; }
        else { if(!sp) break; d = stack[--sp]; if(!sp) break; x -= dx[d]; y -= dy[d]; }
    }
    for(i = 0; i < 4; i++) memcpy(rooms + i * 4, R + i * 4, 4);
    rooms[16] = 27; rooms[17] = 27; rooms[18] = 3; rooms[19] = 3;
    for(i = 0; i < 5; i++){ UINT8 *r = rooms + i * 4, a, b; for(b = r[1]; b < r[1] + r[3]; b++) for(a = r[0]; a < r[0] + r[2]; a++) dm[b * 32 + a] = (rr() & 3) ? f0 : f1; }
    for(i = 0; i < 12; i++){ x = 2 + 2 * (rr() % 14); y = 1 + 2 * (rr() % 15); if(i & 1){ UINT8 t = x; x = y; y = t; } if(x < 31 && y < 31) dm[y * 32 + x] = f0; }    /* a few loops */
}
