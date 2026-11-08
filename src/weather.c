#pragma bank 44
/* BUGSTAR GBC - version 6: weather (see weather.h). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "save.h"
#include "sound.h"
#include "menus.h"
#include "weather.h"

extern const UINT8 wx_tiles[8 * 32];
UINT8 wx_type, wx_flash;
static UINT8 px[WX_NP], py[WX_NP], pp[WX_NP], wx_ns, wx_rs = 77, wx_thunder;

/* what each kind of world can have: 8 picks, the hash chooses one (so CLEAR days are common) */
static const UINT8 wx_pick[12][8] = {
    { WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_RAIN, WX_RAIN, WX_STORM, WX_FOG },            /* GARDEN */
    { WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_DUST, WX_DUST, WX_DUST, WX_CLEAR, WX_DUST },            /* DESERT */
    { WX_SNOW, WX_SNOW, WX_SNOW, WX_SNOW, WX_CLEAR, WX_CLEAR, WX_FOG, WX_STORM },              /* ICE */
    { WX_RAIN, WX_RAIN, WX_RAIN, WX_STORM, WX_STORM, WX_CLEAR, WX_CLEAR, WX_FOG },             /* JUNGLE */
    { WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_RAIN, WX_RAIN, WX_STORM, WX_STORM, WX_FOG },            /* OCEAN */
    { WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_FOG, WX_FOG, WX_SHARDS, WX_SPORES, WX_SHARDS },         /* DUSK */
    { WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_DUST, WX_DUST, WX_DUST, WX_STORM },           /* MESA */
    { WX_SPORES, WX_SPORES, WX_SPORES, WX_SPORES, WX_SPORES, WX_FOG, WX_FOG, WX_CLEAR },       /* FUNGAL */
    { WX_SHARDS, WX_SHARDS, WX_SHARDS, WX_SHARDS, WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_STORM },    /* CRYSTAL */
    { WX_ASH, WX_ASH, WX_ASH, WX_ASH, WX_ASH, WX_CLEAR, WX_CLEAR, WX_STORM },                  /* LAVA */
    { WX_FOG, WX_FOG, WX_FOG, WX_RAIN, WX_RAIN, WX_RAIN, WX_SPORES, WX_CLEAR },                /* SWAMP */
    { WX_CONFETTI, WX_CONFETTI, WX_CONFETTI, WX_CONFETTI, WX_CLEAR, WX_CLEAR, WX_CLEAR, WX_RAIN } /* CANDY */
};
static const char *const wx_nm[WX_N] = { "CLEAR", "RAIN", "A STORM", "SNOWFALL", "ASH FALL", "SPORES", "DUST STORM", "SHARDS", "FOG", "CONFETTI" };

static UINT8 rnd(void){ wx_rs ^= wx_rs << 7; wx_rs ^= wx_rs >> 9; wx_rs ^= wx_rs << 8; return (UINT8)wx_rs; }

void weather_start(UINT8 wk, UINT8 star) BANKED {
    UINT32 h = world_seed(star) ^ ((UINT32)(sv.secs / 480UL) * 2654435761UL);
    UINT8 i;
    h ^= h >> 15; h *= 2246822519UL; h ^= h >> 13;
#ifdef TEST_WEATHER
    wx_type = TEST_WEATHER;
#else
    wx_type = wx_pick[wk % 12][(UINT8)(h >> 5) & 7];
#endif
    wx_ns = wx_type == WX_FOG || wx_type == WX_CLEAR ? 0 : WX_NP;
    wx_flash = 0; wx_thunder = 120; wx_rs = (UINT16)h | 1;
    for(i = 0; i < WX_NP; i++){ px[i] = rnd() % 160; py[i] = rnd() % 124; pp[i] = rnd(); }
}
void weather_gfx(void) BANKED { set_sprite_data(WX_TILE0, 16, wx_tiles); }
static UINT8 tile_of(UINT8 i){
    switch(wx_type){
    case WX_RAIN: case WX_STORM: return 0;
    case WX_SNOW: return 2;
    case WX_ASH: return 4;
    case WX_SPORES: return 6;
    case WX_DUST: return 8;
    case WX_SHARDS: return 10;
    default: return 12 + ((pp[i] >> 3) & 1) * 2;               /* confetti turns over */
    }
}
void weather_step(UINT8 tick) BANKED {
    UINT8 i;
    if(wx_flash) wx_flash--;
    if(wx_type == WX_STORM && !--wx_thunder){ wx_flash = 5; wx_thunder = 140 + (rnd() & 127); sfx(SFX_HISS); }
    for(i = 0; i < wx_ns; i++){
        switch(wx_type){
        case WX_RAIN: py[i] += 5; px[i] -= 1; break;
        case WX_STORM: py[i] += 7; px[i] -= 3; break;
        case WX_SNOW: if(tick & 1) py[i]++; if(!(tick & 7)) px[i] += (pp[i] & 1) ? 1 : -1; if(!(tick & 31)) pp[i] ^= 1; break;
        case WX_ASH: if(tick & 1) py[i]--; if(!(tick & 7)) px[i] += (pp[i] & 2) ? 1 : -1; if(!(tick & 31)) pp[i] += 2; break;
        case WX_SPORES: if(!(tick & 3)){ py[i]--; px[i] += (pp[i] & 1) ? 1 : -1; } if(!(tick & 15)) pp[i] += 1 + (rnd() & 1); break;
        case WX_DUST: px[i] += 5; if(!(tick & 3)) py[i] += (pp[i] & 1) ? 1 : 0; break;
        case WX_SHARDS: if(!(++pp[i] & 63)){ px[i] = rnd() % 160; py[i] = rnd() % 128; } break;
        default: if(tick & 1) py[i]++; if(!(tick & 3)) px[i] += (pp[i] & 4) ? 1 : -1; pp[i]++; break;
        }
        if(wx_type != WX_SHARDS){
            if(py[i] >= 150 && py[i] < 200) py[i] = 0;            /* wraps: off the bottom comes back at the top (and the other way for what rises) */
            else if(py[i] >= 200){ py[i] = 143; }
            if(px[i] >= 164 && px[i] < 220){ px[i] = 0; py[i] = rnd() % 140; }
            else if(px[i] >= 220){ px[i] = 159; }
        }
    }
}
void weather_draw(void) BANKED {
    UINT8 i;
    for(i = 0; i < WX_NP; i++){
        if(i >= wx_ns || py[i] > 122 || (wx_type == WX_SHARDS && (pp[i] & 63) > 44)){ move_sprite(WX_OAM0 + i, 0, 0); continue; }
        set_sprite_tile(WX_OAM0 + i, WX_TILE0 + tile_of(i)); set_sprite_prop(WX_OAM0 + i, 1 | 0x08);
        move_sprite(WX_OAM0 + i, px[i] + 8, py[i] + 16);          /* screen position + the hardware offsets (8, 16) */
    }
}
void weather_hide(void) BANKED { UINT8 i; for(i = 0; i < WX_NP; i++) move_sprite(WX_OAM0 + i, 0, 0); }
#define LIM(v) ((v) > 31 ? 31 : (v))
void weather_pal(palette_color_t *p, UINT8 n) BANKED {
    UINT8 i;
    if(wx_type == WX_CLEAR || wx_type == WX_RAIN || wx_type == WX_SNOW || wx_type == WX_SPORES || wx_type == WX_SHARDS || wx_type == WX_CONFETTI){
        if(!wx_flash) return;
    }
    for(i = 0; i < n; i++){
        UINT16 c = p[i], r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
        switch(wx_type){
        case WX_FOG: r = (r * 2 + 27) / 3; g = (g * 2 + 27) / 3; b = (b * 2 + 28) / 3; break;                 /* pale and soft */
        case WX_DUST: r = LIM(r + 5); g = LIM(g + 1); b = b > 4 ? b - 4 : 0; r = (r * 7 + 26) >> 3; g = (g * 7 + 18) >> 3; b = (b * 7 + 8) >> 3; break;
        case WX_STORM: r = r * 3 / 4; g = g * 3 / 4; b = b * 4 / 5; break;                                    /* dark skies */
        case WX_ASH: r = (r * 5 + 10) / 6; g = (g * 5 + 7) / 6; b = (b * 5 + 7) / 6; break;                 /* grey haze */
        default: break;
        }
        if(wx_flash){ r = LIM(r + 14); g = LIM(g + 14); b = LIM(b + 16); }                                    /* lightning */
        p[i] = RGB(r, g, b);
    }
}
void weather_name(char *out) BANKED { scat(out, wx_nm[wx_type % WX_N]); }
