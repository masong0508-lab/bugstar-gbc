#pragma bank 28
/* WILD, part 2: WEIRD WORLDS. About one star in nine has a trait (16 kinds, 9 strengths): the planet's colours bend (a tint, a rainbow that
   turns, inverted, pulsing...), and it changes the game a little: more gold from kills, tougher or fiercer monsters, richer ore, healing or
   bad air. The first of each kind you land on goes in the book (EXTRAS > BESTIARY). A world's value w is (strength << 4) | trait; 0 = normal. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "wram.h"
#include "sound.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "dialog.h"
#include "wild.h"

/* mode: 0 none, 1 tint (quarters per channel), 2 rainbow, 3 inverted, 4 sepia, 5 neon, 6 pulse. The rest are per strength: gold %, monster health %, monster bite (tenths of a point), ore %, healing */
typedef struct { const char *nm; UINT8 mode, tr, tg, tb, gold, foe, hit, ore; INT8 heal; } WT;
static const WT wt[16] = {
    { "GOLDEN",   1, 6, 5, 2, 6, 0, 0, 0,  0 },
    { "ARMORED",  4, 0, 0, 0, 0, 6, 0, 0,  0 },
    { "FIERCE",   1, 6, 2, 2, 3, 0, 4, 0,  0 },
    { "ORE RICH", 1, 3, 4, 6, 0, 0, 0, 8,  0 },
    { "HEALING",  1, 3, 6, 3, 0, 0, 0, 0,  1 },
    { "POISON",   1, 5, 2, 6, 5, 0, 0, 0, -1 },
    { "RAINBOW",  2, 0, 0, 0, 2, 0, 0, 0,  0 },
    { "NEON",     5, 0, 0, 0, 2, 0, 0, 4,  0 },
    { "INVERTED", 3, 0, 0, 0, 4, 0, 0, 0,  0 },
    { "GHOSTLY",  4, 0, 0, 0, 0, 0, 3, 0,  1 },
    { "PULSING",  6, 0, 0, 0, 3, 0, 0, 3,  0 },
    { "CRIMSON",  1, 7, 2, 2, 5, 3, 2, 0,  0 },
    { "FROZEN",   1, 3, 4, 7, 0, 4, 0, 0,  1 },
    { "TOXIC",    1, 3, 7, 2, 0, 0, 0, 6, -1 },
    { "LUCKY",    2, 0, 0, 0, 7, 0, 0, 3,  0 },
    { "STORMY",   6, 0, 0, 0, 3, 3, 3, 0,  0 } };
static const char *const adj[9] = { "FAINT", "MILD", "SOFT", "PLAIN", "STRONG", "HARSH", "WILD", "FIERCE", "EXTREME" };

UINT8 weird_of(UINT8 star) BANKED {
    UINT32 ws = world_seed(star);
#ifdef TEST_WEIRD
    (void)ws; return TEST_WEIRD;
#endif
    UINT16 h = (UINT16)ws ^ (UINT16)(ws >> 16);
    h ^= h >> 7; h *= 0x2D3; h ^= h >> 5;
    if(h % 9) return 0;
    return (UINT8)(((1 + ((h >> 3) & 0x7FF) % 9) << 4) | ((h >> 9) & 15));
}
void weird_name(char *out, UINT8 w) BANKED { char *d = scat(out, adj[(w >> 4) - 1]); d = scat(d, " "); scat(d, wt[w & 15].nm); }
UINT8 weird_pct(UINT8 w, UINT8 what) BANKED {
    const WT *t = &wt[w & 15]; UINT8 s = w >> 4;
    if(!w) return 0;
    switch(what){
    case WP_GOLD: return s * t->gold;
    case WP_FOE:  return s * t->foe;
    case WP_HIT:  return s * t->hit / 10;
    default:      return s * t->ore;
    }
}
void weird_note(char *out, UINT8 w) BANKED {
    const WT *t = &wt[w & 15]; char *d = out; UINT8 s = w >> 4;
    if(t->gold){ d = u16s(scat(d, "+"), s * t->gold); scat(d, "% GOLD"); }
    else if(t->ore){ d = u16s(scat(d, "+"), s * t->ore); scat(d, "% ORE"); }
    else if(t->foe){ d = u16s(scat(d, "FOES +"), s * t->foe); scat(d, "% HP"); }
    else if(t->heal){ scat(d, t->heal > 0 ? "THE AIR HEALS" : "THE AIR STINGS"); }
    else scat(d, "STRANGE COLOURS");
}
#define BL(o, a) ((UINT8)(((UINT16)(o) * (9 - s) + (UINT16)(a) * s) / 9))
#define CL(v) ((v) > 31 ? 31 : (v))
void weird_pal(palette_color_t *p, UINT8 n, UINT8 w, UINT8 tick) BANKED {
    const WT *t = &wt[w & 15]; UINT8 s = w >> 4, i;
    if(!w || !t->mode) return;
    for(i = 0; i < n; i++){
        UINT16 c = p[i], R, G, B;
        UINT8 r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31, l = (UINT8)((r + g + b) / 3);
        switch(t->mode){
        case 1: R = CL((UINT16)r * t->tr >> 2); G = CL((UINT16)g * t->tg >> 2); B = CL((UINT16)b * t->tb >> 2); break;
        case 2: { UINT8 pos = tick % 96, k = pos >> 5, f = pos & 31, r1, g1, b1, r2, g2, b2;
                  if(k == 0){ r1 = r; g1 = g; b1 = b; r2 = g; g2 = b; b2 = r; }
                  else if(k == 1){ r1 = g; g1 = b; b1 = r; r2 = b; g2 = r; b2 = g; }
                  else { r1 = b; g1 = r; b1 = g; r2 = r; g2 = g; b2 = b; }
                  R = ((UINT16)r1 * (32 - f) + (UINT16)r2 * f) >> 5; G = ((UINT16)g1 * (32 - f) + (UINT16)g2 * f) >> 5; B = ((UINT16)b1 * (32 - f) + (UINT16)b2 * f) >> 5; } break;
        case 3: R = 31 - r; G = 31 - g; B = 31 - b; break;
        case 4: R = CL(l + (l >> 2)); G = l; B = l - (l >> 2); break;
        case 5: R = r + r > l ? CL(r + r - l) : 0; G = g + g > l ? CL(g + g - l) : 0; B = b + b > l ? CL(b + b - l) : 0; break;
        default: { UINT8 q = (tick >> 1) & 31, m; q = q > 15 ? 31 - q : q; m = 8 + (q >> 1);
                  R = CL((UINT16)r * m >> 3); G = CL((UINT16)g * m >> 3); B = CL((UINT16)b * m >> 3); } break;
        }
        p[i] = RGB(BL(r, R), BL(g, G), BL(b, B));
    }
}
UINT8 weird_tick(UINT8 w, UINT8 tick) BANKED {
    INT8 h = wt[w & 15].heal; UINT16 mx = hud_hpmax(sv.level);
    (void)tick;
    if(!w || !h) return 0;
    if(h > 0){ if(sv.hp >= mx) return 0; sv.hp += 1 + (w >> 6); if(sv.hp > mx) sv.hp = mx; return 1; }
    if(sv.hp * 4 <= mx) return 0;                                  /* the bad air never kills: it stops at a quarter */
    sv.hp--; return 1;
}
void weird_land(UINT8 w) BANKED {
    char b[80], *d; UINT8 first;
    if(!w) return;
    wild_load();
    wram_bank(2); first = !(XAP->wseen & (1U << (w & 15))); XAP->wseen |= 1U << (w & 15); wram_bank(1);
    if(!first) return;
    wild_save();
    d = scat(b, "THIS WORLD IS "); weird_name(d, w); d = b + strlen(b); d = scat(d, ": "); weird_note(d, w); d = b + strlen(b); scat(d, ". LOOK AT THE COLOURS!");
    sfx(SFX_FANFARE);
    dlg_say("BOLT", b);
}
void weird_screen(UINT8 y0) BANKED {
    UINT8 i, n = 0; UINT16 seen; char b[21], *d; UINT8 pt;
    wild_load();
    wram_bank(2); seen = XAP->wseen; pt = XAP->petn; wram_bank(1);
    for(i = 0; i < 16; i++){
        UINT8 x = (i & 1) ? 11 : 2, y = y0 + (i >> 1), got = (seen >> i) & 1;
        n += got;
        txt_line(x, y, got ? wt[i].nm : "?????", 8); txt_color(x, y, 8, 1, got ? TXT_GOLD : TXT_GREY);
    }
    d = u16s(b, n); d = scat(d, " OF 16 MET"); *d = 0; txt_line(2, y0 + 9, b, 16); txt_color(2, y0 + 9, 16, 1, TXT_WHITE);
    d = scat(b, "FRIENDS MADE "); d = u16s(d, pt); *d = 0; txt_line(2, y0 + 10, b, 16); txt_color(2, y0 + 10, 16, 1, TXT_GREY);
}
