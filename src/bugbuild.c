#pragma bank 12
/* The bug builder (step 5, split from bug.c so the art can grow): stamps the parts into a canvas and cuts it into sprites. */
#include <string.h>
#include "wram.h"
#include "bug.h"

/* ---- the picture */
BugSet bug_set;
#define bpx WRAM_AT(WRAM_BPX, UINT8)                     /* the canvas, BUG_BW x BUG_BH (wram.h) */
#define PX(x, y) bpx[(UINT16)(y) * BUG_BW + (x)]
#define HEAD_UP 1                                  /* the head sits level with the shell: its oval (rows 0..9 of its cell) is centred on the body */
#define HEAD_LO 17

static UINT8 part_px(const BugPart *p, UINT8 x, UINT8 y){ UINT16 i = (UINT16)y * p->w + x; return (p->px[i >> 2] >> ((i & 3) << 1)) & 3; }
static void stamp(const BugPart *p, UINT8 x0, UINT8 y0){
    UINT8 x, y, v;
    if(!p->w) return;
    for(y = 0; y < p->h; y++) for(x = 0; x < p->w; x++){ v = part_px(p, x, y); if(v) PX(x0 + p->ox + x, y0 + p->oy + y) = v; }
}

static UINT8 pat_on(UINT8 p, UINT8 x, UINT8 y, UINT8 w){
    INT8 dx, dy; UINT8 t;
    switch(p){
        case 1:  return (x % 6) < 2;
        case 2:  return (((x + ((y >> 2) & 1) * 4) & 7) < 2) && ((y & 3) < 2);
        case 3:  return ((y + 1) % 5) < 2;
        case 4:  return ((x >> 1) + (y >> 1)) & 1;
        case 5:  t = x & 7; t = t < 4 ? t : 8 - t; return y == 4 + t || y == 5 + t;
        case 6:  return (x & 3) == 2 && (y & 3) == 2;
        case 7:  dx = (x & 7) - 4; dy = (y & 7) - 4; if(dx < 0) dx = -dx; if(dy < 0) dy = -dy; return dx + dy == 3;
        case 8:  return ((x + (y >> 1)) % 6) < 2;
        case 9:  return x >= (w >> 1);
        case 10: return ((x * 5 + y * 11 + x * y) & 7) == 0;
        case 11: dx = x - (w >> 1); dy = (y - 6) * 2; return (((INT16)dx * dx + (INT16)dy * dy) / 12) & 1;
    }
    return 0;
}
static void draw_shell(const BugLook *l, UINT8 x0, UINT8 W){
    const BugPart *s = &bp_shell[l->pt[BP_SHAPE] * 3 + l->pt[BP_LENGTH]];
    UINT8 top[32], x, y, d, pat = l->pt[BP_PAT], fin = l->pt[BP_FINISH], on;
    stamp(s, x0, 16);
    for(x = 0; x < W; x++){ top[x] = 99; for(y = 0; y < 12; y++) if(PX(x0 + x, 16 + y)){ top[x] = y; break; } }
    for(y = 0; y < 12; y++) for(x = 0; x < W; x++) if(PX(x0 + x, 16 + y) == 2){
        if(pat) on = pat_on(pat, x, y, W);
        else {
            d = y - top[x]; on = 0;
            if(fin == 0) on = (d == 1 && x >= W / 5 + 1 && x < W / 5 + 5);
            else if(fin == 2) on = (d == 1 && x >= 3 && x + 4 < W);
            else if(fin == 3) on = (d >= 1 && d < 4 && (x * 3 + y) % 7 == 0);
        }
        if(on) PX(x0 + x, 16 + y) = 3;
    }
    if(W >= 24 && !pat){                                              /* a seam between the thorax and the abdomen: it is a bug, not a pill */
        UINT8 sx = W - 9;
        for(y = 1; y < 11; y++) if(PX(x0 + sx, 16 + y) == 2 && PX(x0 + sx, 16 + y - 1) && PX(x0 + sx, 16 + y + 1)) PX(x0 + sx, 16 + y) = 1;
    }
}
static void draw_back(const BugLook *l, UINT8 x0, UINT8 W){      /* gear on the back: in the wing cell, with a stem down to the shell */
    const BugPart *b = &bp_back[l->pt[BP_BACK]];
    UINT8 cx = x0 + (W >> 1) - 4, x, y, top;
    if(!b->w) return;
    stamp(b, cx, 0);
    for(x = cx; x < cx + 8; x++) if(PX(x, 15)){
        for(top = 16; top < 28 && !PX(x, top); top++) ;
        if(top < 28) for(y = 16; y < top; y++) PX(x, y) = 1;       /* (index 1 is the outline in the shell's palette) */
    }
}
static void join_wings(UINT8 x0, UINT8 W){                          /* wings sit ON the shell: no hovering gap, no double outline between them */
    UINT8 x;
    for(x = x0; x < x0 + W; x++){
        if(!PX(x, 15)) continue;
        if(!PX(x, 16) && PX(x, 17)) PX(x, 16) = 1;                      /* a gap row: bridge it with the shell's outline */
        if(PX(x, 16) && PX(x, 14)) PX(x, 15) = 2;                        /* the wing's own bottom outline: its body runs down into the shell instead */
    }
}
static void draw_legs(const BugLook *l, UINT8 x0, UINT8 W, UINT8 f){      /* splayed, jointed legs: the front ones lean forward, the back ones back, they step in pairs */
    UINT8 n = 3 + l->pt[BP_LEGS], i, lx, by, y, k, st = l->pt[BP_LEGST], lift, len;
    INT8 dir, x;
    for(i = 0; i < n; i++){
        lx = x0 + 3 + (UINT8)(((UINT16)(W - 7) * i) / (n - 1));
        dir = (i * 2 < n - 1) ? -1 : (i * 2 > n - 1) ? 1 : 0;
        lift = (i + f) & 1; len = 4 - lift;
        by = 16;
        for(y = 0; y < 12; y++) if(PX(lx, 16 + y)) by = 16 + y;
        for(k = 1; k <= len; k++){
            y = by + k;
            x = (INT8)lx + (k >= 2 ? dir : 0) + (k >= 3 ? dir * (lift ? 0 : 1) : 0) + (lift && k == len ? 1 : 0);
            if(x < 0) continue;
            if(!PX(x, y)) PX(x, y) = 1;
            if(st == 1 && !PX(x + 1, y)) PX(x + 1, y) = 1;
            if(st == 2 && k == 1 && !PX(x + 1, y)) PX(x + 1, y) = 1;
            if(st == 3 && k == len){ if(!PX(x + 1, y)) PX(x + 1, y) = 1; if(!PX(x + 2, y)) PX(x + 2, y) = 1; }
        }
    }
}

static UINT8 cell_x(UINT8 c, UINT8 tc, UINT8 hc){ return c < 4 ? (tc + c) * 8 : c < 8 ? (tc + c - 4) * 8 : c == 8 ? 0 : hc * 8; }
static UINT8 cell_y(UINT8 c){ return c < 4 ? 0 : c < 9 ? 16 : c == 9 ? HEAD_UP : HEAD_LO; }
UINT8 bug_cell_pal(UINT8 c){ return c < 4 ? 3 : c < 9 ? 0 : c == 9 ? 2 : 1; }

static void draw_canvas(const BugLook *l, UINT8 f, UINT8 tc, UINT8 hc, UINT8 W){
    memset(bpx, 0, BUG_BW * BUG_BH);
    if(l->pt[BP_WING]) stamp(&bp_wings[l->pt[BP_WING] * 3 + l->pt[BP_LENGTH]], tc * 8, 0);
    if(tc) stamp(&bp_tail[l->pt[BP_TAIL]], 0, 16);
    draw_shell(l, tc * 8, W);
    draw_back(l, tc * 8, W);
    if(l->pt[BP_WING]) join_wings(tc * 8, W);
    draw_legs(l, tc * 8, W, f);
    stamp(&bp_head[0], hc * 8, HEAD_LO);
    stamp(&bp_neck[l->pt[BP_NECK]], hc * 8, HEAD_LO);
    stamp(&bp_eye[l->pt[BP_EYE]], hc * 8, HEAD_LO);
    stamp(&bp_mouth[l->pt[BP_MOUTH]], hc * 8, HEAD_LO);
    stamp(&bp_cheek[l->pt[BP_CHEEK]], hc * 8, HEAD_LO);
    stamp(&bp_ant[l->pt[BP_ANT]], hc * 8, HEAD_UP);
    stamp(&bp_horn[l->pt[BP_HORN]], hc * 8, HEAD_UP);
    stamp(&bp_hat[l->pt[BP_HAT]], hc * 8, HEAD_UP);
}
static void cut_cells(UINT8 f, UINT8 tc, UINT8 hc, UINT8 W, UINT8 *used){
    UINT8 c, r, x, v, lo, hi, nz, *t, x0, y0;
    for(c = 0; c < BUG_MAXCELL; c++){
        x0 = cell_x(c, tc, hc); y0 = cell_y(c); t = bug_set.tiles[f][c]; nz = 0;
        if((c < 4 && c >= (W >> 3)) || (c >= 4 && c < 8 && c - 4 >= (W >> 3)) || (c == 8 && !tc)){ memset(t, 0, 32); continue; }
        for(r = 0; r < 16; r++){
            lo = hi = 0;
            for(x = 0; x < 8; x++){ v = PX(x0 + x, y0 + r); lo = (lo << 1) | (v & 1); hi = (hi << 1) | (v >> 1); nz |= v; }
            t[r * 2] = lo; t[r * 2 + 1] = hi;
        }
        if(nz) used[c] = 1;
    }
}

void bug_build(const BugLook *l) BANKED {
    UINT8 W = 16 + 8 * l->pt[BP_LENGTH], tc = l->pt[BP_TAIL] ? 1 : 0, hc = tc + (W >> 3), c, n = 0;
    UINT8 used[BUG_MAXCELL];
    memset(used, 0, sizeof used);
    draw_canvas(l, 1, tc, hc, W); cut_cells(1, tc, hc, W, used);
    draw_canvas(l, 0, tc, hc, W); cut_cells(0, tc, hc, W, used);       /* frame 0 last: it stays in bpx for previews */
    for(c = 0; c < BUG_MAXCELL; c++) if(used[c]){
        bug_set.s[n].x = cell_x(c, tc, hc); bug_set.s[n].y = cell_y(c); bug_set.s[n].cell = c; bug_set.s[n].pal = bug_cell_pal(c); n++;
    }
    bug_set.n = n; bug_set.w = (hc + 1) * 8;
}
UINT8 bug_hc(const BugLook *l) BANKED { return (l->pt[BP_TAIL] ? 1 : 0) + ((16 + 8 * l->pt[BP_LENGTH]) >> 3); }
void bug_canvas(const BugLook *l, UINT8 f) BANKED {          /* frame f in bpx, each pixel = region * 4 + colour (0 stays clear): for the big preview */
    UINT8 W = 16 + 8 * l->pt[BP_LENGTH], tc = l->pt[BP_TAIL] ? 1 : 0, hc = tc + (W >> 3), x, y, v, *p = bpx;
    draw_canvas(l, f, tc, hc, W);
    for(y = 0; y < BUG_BH; y++) for(x = 0; x < BUG_BW; x++, p++){
        v = *p;
        if(v) *p = v | ((x >= hc * 8 ? (y < HEAD_LO ? 2 : 1) : (y < 16 ? 3 : 0)) << 2);
    }
}
UINT8 bug_pixel(UINT8 x, UINT8 y){ return (x < BUG_BW && y < BUG_BH) ? PX(x, y) : 0; }
UINT8 bug_peak(void){
    UINT8 cnt[BUG_BH + 16], i, y, peak = 0;
    memset(cnt, 0, sizeof cnt);
    for(i = 0; i < bug_set.n; i++) for(y = 0; y < 16; y++) if(++cnt[bug_set.s[i].y + y] > peak) peak = cnt[bug_set.s[i].y + y];
    return peak;
}
