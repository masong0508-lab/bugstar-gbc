#pragma bank 29
/* WILD, part 3: LIVING CREATURES. Four harmless animals live around the ship on every world (2 kinds for each of the 6 worlds: art in life_art.c,
   made by tools/make_life.py). Each one has a mood: SHY ones run from you, FRIENDLY ones just wander and like a pat (A), CURIOUS ones
   come to look at you. A pat pays a little gold once per landing and counts as a friend made (the book shows them). They live in WRAM
   bank 2 (D100..), think every other frame, and the planet checks their ground (life_peek / life_back): they never go in walls or water.
   Sprites: OAM 15..18, tiles 128..135 (VRAM bank 1), sprite palette 3. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "wram.h"
#include "sound.h"
#include "save.h"
#include "hud.h"
#include "wild.h"

extern const UINT8 life_tiles[6][128];
extern const palette_color_t life_pal[6][4];

typedef struct { UINT16 x, y, ox, oy; INT8 vx, vy; UINT8 sp, mood, hop, pat, t, face; } An;
typedef struct { UINT16 rs; An a[4]; } LifeS;
#define LF ((LifeS *)0xD100)
#define LIFE_OAM0 15

static INT16 wd(UINT16 a, UINT16 b){ INT16 d = (INT16)((a - b) & 4095); return d > 2048 ? d - 4096 : d; }
static INT16 ab(INT16 v){ return v < 0 ? -v : v; }
static UINT8 rnd(void){ UINT16 r = LF->rs; r ^= r << 7; r ^= r >> 9; r ^= r << 8; LF->rs = r; return (UINT8)r; }

void life_gfx(UINT8 wk) BANKED {
    VBK_REG = VBK_BANK_1;
    set_sprite_data(128, 8, life_tiles[wk]);
    VBK_REG = VBK_BANK_0;
    set_sprite_palette(3, 1, life_pal[wk]);
}
void life_load(UINT8 wk, UINT16 px, UINT16 py) BANKED {
    static const INT8 ox[4] = { -24, 24, -10, 34 }, oy[4] = { 20, 14, 34, 30 };
    UINT8 i;
    wram_bank(2);
    memset(LF, 0, sizeof *LF);
    LF->rs = (UINT16)frames32() | 1;
    for(i = 0; i < 4; i++){
        An *a = &LF->a[i];
        a->x = a->ox = (px + ox[i]) & 4095; a->y = a->oy = (py + oy[i]) & 4095;
        a->sp = i & 1; a->mood = (i + wk) % 3; a->t = i * 5; a->face = i >> 1;
    }
    wram_bank(1);
}
void life_step(UINT16 bx, UINT16 by, UINT8 tick) BANKED {
    UINT8 i, st; INT16 dx, dy, near; (void)tick;
    wram_bank(2);
    for(i = 0; i < 4; i++){
        An *a = &LF->a[i];
        a->ox = a->x; a->oy = a->y; a->t++;
        dx = wd(a->x, bx); dy = wd(a->y, by);
        if(ab(dx) > 170 || ab(dy) > 130) continue;
        if(a->hop) a->hop--;
        near = ab(dx) + ab(dy); st = 0;
        if(a->mood == 0 && near < 44){                                  /* shy: runs, as fast as you walk */
            a->vx = dx > 0 ? 1 : dx < 0 ? -1 : 0; a->vy = dy > 0 ? 1 : dy < 0 ? -1 : 0; st = 2;
        } else if(a->mood == 2 && near > 30 && near < 120){             /* curious: comes to look */
            a->vx = dx > 8 ? -1 : dx < -8 ? 1 : 0; a->vy = dy > 8 ? -1 : dy < -8 ? 1 : 0; st = 1;
        } else {                                                        /* wander */
            if(!(a->t & 15)){ UINT8 r = rnd(); a->vx = (INT8)(r % 3) - 1; a->vy = (INT8)((r >> 2) % 3) - 1; }
            st = (a->t & 1) ? 1 : 0;
        }
        if(st && a->hop < 8){ a->x = (a->x + a->vx * st) & 4095; a->y = (a->y + a->vy * st) & 4095; if(a->vx) a->face = a->vx < 0; }
    }
    wram_bank(1);
}
UINT8 life_peek(UINT8 i, UINT16 *x, UINT16 *y) BANKED {
    UINT8 r;
    wram_bank(2);
    r = LF->a[i].x != LF->a[i].ox || LF->a[i].y != LF->a[i].oy;
    *x = LF->a[i].x; *y = LF->a[i].y;
    wram_bank(1);
    return r;
}
void life_back(UINT8 i) BANKED {
    An *a;
    wram_bank(2);
    a = &LF->a[i]; a->x = a->ox; a->y = a->oy; a->vx = -a->vx; a->vy = -a->vy;
    wram_bank(1);
}
void life_draw(UINT16 cx, UINT16 cy) BANKED {
    UINT8 i;
    wram_bank(2);
    for(i = 0; i < 4; i++){
        An *a = &LF->a[i];
        INT16 sx = (INT16)((a->x - cx) & 4095), sy = (INT16)((a->y - cy) & 4095);
        UINT8 f = a->hop ? 1 : ((a->x + a->y) >> 3) & 1, lift = a->hop ? (a->hop < 8 ? a->hop : 16 - a->hop) : 0;
        if(sx > 2048) sx -= 4096; if(sy > 2048) sy -= 4096;
        sy -= lift;
        if(sx < -8 || sx > 168 || sy < 0 || sy > 140){ move_sprite(LIFE_OAM0 + i, 0, 0); continue; }
        set_sprite_tile(LIFE_OAM0 + i, 128 + (a->sp * 2 + f) * 2);
        set_sprite_prop(LIFE_OAM0 + i, 3 | 0x08 | (a->face ? 0x20 : 0));
        move_sprite(LIFE_OAM0 + i, (UINT8)(sx + 4), (UINT8)sy);
    }
    wram_bank(1);
}
UINT8 life_pat(UINT16 bx, UINT16 by) BANKED {
    UINT8 i, r = 0;
    wram_bank(2);
    for(i = 0; i < 4; i++){
        An *a = &LF->a[i];
        if(ab(wd(a->x, bx)) < 18 && ab(wd(a->y, by)) < 18){
            if(a->mood == 0){ r = 3; a->hop = 0; }
            else if(a->pat) r = 4;
            else { a->pat = 1; a->hop = 16; r = a->mood; }
            break;
        }
    }
    wram_bank(1);
    if(r == 1 || r == 2){
        sv.gold += 3 + sv.level / 2 + r; gain_xp(1 + sv.level / 8); sfx(SFX_OK);
        wild_load();
        wram_bank(2); if(XAP->petn < 255) XAP->petn++; wram_bank(1);
        wild_save();
    }
    return r;
}
