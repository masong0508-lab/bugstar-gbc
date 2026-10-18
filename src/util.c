#include <gb/gb.h>
#include "text.h"
#include "menus.h"
/* small text helpers: in bank 0, because every bank uses them */
/* ------------------------------------------------------------ small text helpers */
void put_line(UINT8 x, UINT8 y, const char *s, UINT8 w){ txt_line(x, y, s, w); }
char *scat(char *d, const char *s){ while(*s) *d++ = *s++; *d = 0; return d; }
char *u16s(char *d, UINT16 v){
    char t[6]; UINT8 n = 0;
    if(!v) t[n++] = '0';
    while(v){ t[n++] = '0' + (UINT8)(v % 10); v /= 10; }
    while(n) *d++ = t[--n];
    *d = 0; return d;
}
char *u32s(char *d, UINT32 v){
    char t[10]; UINT8 n = 0;
    if(!v) t[n++] = '0';
    while(v){ t[n++] = '0' + (UINT8)(v % 10); v /= 10; }
    while(n) *d++ = t[--n];
    *d = 0; return d;
}
static char *two(char *d, UINT8 v){ *d++ = '0' + v / 10; *d++ = '0' + v % 10; *d = 0; return d; }
char *fmt_time(char *d, UINT32 secs){
    UINT16 h = (UINT16)(secs / 3600UL);
    d = u16s(d, h); *d++ = ':';
    d = two(d, (UINT8)((secs / 60UL) % 60UL)); *d++ = ':';
    return two(d, (UINT8)(secs % 60UL));
}
char *fmt_hex32(char *d, UINT32 v){
    UINT8 i;
    for(i = 0; i < 8; i++){ UINT8 n = (UINT8)((v >> (28 - 4 * i)) & 15); *d++ = n < 10 ? '0' + n : 'A' + n - 10; }
    *d = 0; return d;
}

const char *const kind_name[6] = { "GARDEN", "DESERT", "ICE", "JUNGLE", "OCEAN", "DUSK" };
UINT8 world_kind(UINT32 wseed){ return (UINT8)((wseed >> 8) % 6); }
static const char *const syl[16] = { "ZOR", "BA", "KI", "LU", "MOX", "TE", "VEN", "RA", "QUI", "NO", "PEL", "SA", "DRO", "MI", "GAX", "U" };
char *world_name(char *d, UINT32 wseed){
    UINT8 a = (UINT8)(wseed >> 16), b = (UINT8)(wseed >> 24);
    d = scat(d, syl[a & 15]); d = scat(d, syl[a >> 4]);
    if(b & 1) d = scat(d, syl[(b >> 1) & 15]);
    return d;
}
