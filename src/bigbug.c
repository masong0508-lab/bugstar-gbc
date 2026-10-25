#pragma bank 13
/* The big bug preview (see bigbug.h). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "wram.h"
#include "font.h"
#include "bug.h"
#include "bigbug.h"

#define bpx   WRAM_AT(WRAM_BPX, UINT8)
#define info  WRAM_AT(WRAM_MAKE, UINT8)                   /* [set][ty][tx]: the tile's BG palette, or 0xFF when it is clear */
#define PAL_STAGE 4
#define PAL_MIX   2

static const UINT8 zrow[BUG_BW];
static const BugLook *b_look;
static UINT8 b_hc, b_set, b_row, b_active, b_ready;

void big_colors(const BugLook *l, UINT16 stage) BANKED {
    UINT16 p[16]; palette_color_t q[4]; UINT8 k;
    bug_palettes(l, p);
    for(k = 0; k < 4; k++){ q[0] = stage; q[1] = p[k * 4 + 1]; q[2] = p[k * 4 + 2]; q[3] = p[k * 4 + 3]; set_bkg_palette(4 + k, 1, q); }
    q[0] = stage; q[1] = p[1]; q[2] = p[6]; q[3] = p[11]; set_bkg_palette(PAL_MIX, 1, q);
}

void big_start(const BugLook *l) BANKED {
    b_look = l; b_hc = bug_hc(l); b_set = 0; b_row = 0; b_active = 1; b_ready = 0;
    bug_canvas(l, 0);
}
UINT8 big_ready(void) BANKED { return b_ready; }

static void render_row(UINT8 set, UINT8 ty){
    UINT8 tx, nw = (b_hc + 1) * 2, sx, sy, x, y, P, A, B, C, D, a, m, r, k, pal, lo, hi, o[64];
    const UINT8 *r0, *r1, *r2;
    for(tx = 0; tx < nw; tx++){
        m = 0;
        for(sy = 0; sy < 4; sy++){
            y = ty * 4 + sy;
            r1 = y < BUG_BH ? bpx + (UINT16)y * BUG_BW : zrow;
            r0 = (y && y - 1 < BUG_BH) ? r1 - BUG_BW : zrow;
            r2 = y + 1 < BUG_BH ? r1 + BUG_BW : zrow;
            for(sx = 0; sx < 4; sx++){
                x = tx * 4 + sx;
                P = r1[x]; A = r0[x]; D = r2[x]; C = x ? r1[x - 1] : 0; B = x < BUG_BW - 1 ? r1[x + 1] : 0;
                k = (UINT8)(sy * 16 + sx * 2);
                o[k]     = (C == A && C != D && A != B) ? A : P;
                o[k + 1] = (A == B && A != C && B != D) ? B : P;
                o[k + 8] = (D == C && D != B && C != A) ? C : P;
                o[k + 9] = (B == D && B != A && D != C) ? D : P;
            }
        }
        for(k = 0; k < 64; k++) if(o[k]) m |= 1 << (o[k] >> 2);
        a = set * BIG_TW * BIG_TH + ty * BIG_TW + tx;
        if(!m){ info[a] = 0xFF; continue; }
        pal = (m & 6) == 6 ? PAL_MIX : (m & 1) ? 4 : (m & 2) ? 5 : (m & 4) ? 6 : 7;
        info[a] = pal;
        {   UINT8 t[16];
            for(r = 0; r < 8; r++){
                lo = hi = 0;
                for(k = 0; k < 8; k++){ P = o[r * 8 + k] & 3; lo = (lo << 1) | (P & 1); hi = (hi << 1) | (P >> 1); }
                t[r * 2] = lo; t[r * 2 + 1] = hi;
            }
            VBK_REG = VBK_BANK_1; set_bkg_data(a, 1, t); VBK_REG = VBK_BANK_0;
        }
    }
    (void)r0;
}

UINT8 big_step(void) BANKED {
    if(!b_active) return 0;
    render_row(b_set, b_row);
    if(++b_row >= BIG_TH){
        b_row = 0; b_ready |= 1 << b_set;
        if(b_set == 0){ b_set = 1; bug_canvas(b_look, 1); } else b_active = 0;
    }
    return 1;
}

void big_show(UINT8 set, UINT8 flip) BANKED {
    UINT8 ty, tx, nw = (b_hc + 1) * 2, off = (BIG_TW - nw) / 2, sx, a, id[BIG_TW], at[BIG_TW];
    for(ty = 0; ty < BIG_TH; ty++){
        for(tx = 0; tx < BIG_TW; tx++){ id[tx] = T_BLANK; at[tx] = PAL_STAGE; }
        for(tx = 0; tx < nw; tx++){
            sx = flip ? nw - 1 - tx : tx;
            a = info[set * BIG_TW * BIG_TH + ty * BIG_TW + sx];
            if(a != 0xFF){ id[off + tx] = set * BIG_TW * BIG_TH + ty * BIG_TW + sx; at[off + tx] = a | 8 | (flip ? 0x20 : 0); }
        }
        VBK_REG = VBK_BANK_0; set_bkg_tiles(BIG_X, BIG_Y + ty, BIG_TW, 1, id);
        VBK_REG = VBK_BANK_1; set_bkg_tiles(BIG_X, BIG_Y + ty, BIG_TW, 1, at);
        VBK_REG = VBK_BANK_0;
    }
}

void big_clear(void) BANKED {
    UINT8 ty, tx, id[BIG_TW], at[BIG_TW];
    for(tx = 0; tx < BIG_TW; tx++){ id[tx] = T_BLANK; at[tx] = PAL_STAGE; }
    for(ty = 0; ty < BIG_TH; ty++){
        VBK_REG = VBK_BANK_0; set_bkg_tiles(BIG_X, BIG_Y + ty, BIG_TW, 1, id);
        VBK_REG = VBK_BANK_1; set_bkg_tiles(BIG_X, BIG_Y + ty, BIG_TW, 1, at);
        VBK_REG = VBK_BANK_0;
    }
}
