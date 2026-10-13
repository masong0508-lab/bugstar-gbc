/* The bug (step 5): look data, ready-made looks, colour tools and the sprite builder. See bug.h. No hardware in here. */
#include <string.h>
#include "bug.h"

#define C15(r,g,b) ((UINT16)(((r)>>3)|(((g)>>3)<<5)|(((b)>>3)<<10)))

const UINT8 bug_part_n[BP_N] = { 3, 5, 4, 12, 8, 6, 8, 3, 4, 6, 6, 5, 12 };
static const char *const n_length[] = { "SHORT", "NORMAL", "LONG" };
static const char *const n_shape[]  = { "ROUND", "DOME", "FLAT", "SPIKY", "HUMPED" };
static const char *const n_finish[] = { "SHINY", "MATTE", "METAL", "PEARL" };
static const char *const n_pat[]    = { "PLAIN", "STRIPED", "SPOTTED", "BANDED", "CHECKED", "ZIGZAG", "DOTTED", "DIAMONDS", "TIGER", "HALVES", "SPECKLED", "RINGS" };
static const char *const n_eye[]    = { "SMALL", "BIG", "GOOGLY", "SLEEPY", "ANGRY", "STARRY", "CYCLOPS", "SHADES" };
static const char *const n_mouth[]  = { "NONE", "SMILE", "JAWS", "PINCERS", "FANGS", "TONGUE" };
static const char *const n_ant[]    = { "CURLED", "STRAIGHT", "DROOPY", "FEATHERY", "CLUBBED", "ZIGZAG", "WHIPS", "NONE" };
static const char *const n_legs[]   = { "FOUR", "SIX", "EIGHT" };
static const char *const n_legst[]  = { "THIN", "CHUNKY", "HAIRY", "BOOTS" };
static const char *const n_horn[]   = { "NONE", "HORN", "TWINS", "RHINO", "UNICORN", "ANTLERS" };
static const char *const n_wing[]   = { "NONE", "LITTLE", "BEETLE", "FLUTTER", "BAT", "FAIRY" };
static const char *const n_tail[]   = { "NONE", "STINGER", "SPIKES", "CURLY", "LONG" };
static const char *const n_hat[]    = { "NONE", "CAP", "TOP HAT", "BOW", "CROWN", "PARTY", "FLOWER", "HALO", "BEANIE", "CHEF", "HELMET", "VIKING" };
const char *const *const bug_part_names[BP_N] = { n_length, n_shape, n_finish, n_pat, n_eye, n_mouth, n_ant, n_legs, n_legst, n_horn, n_wing, n_tail, n_hat };
const char *const bug_part_label[BP_N] = { "LENGTH", "SHAPE", "FINISH", "PATTERN", "EYES", "MOUTH", "ANTENNAE", "LEGS", "LEG STYLE", "HORN", "WINGS", "TAIL", "HAT" };
const char *const bug_col_label[BC_N]  = { "SHELL", "MARKINGS", "HEAD", "EYES", "HORN", "WINGS", "HAT", "OUTLINE" };

const UINT16 bug_swatch[BUG_NSWATCH] = {
    C15(92,108,148), C15(190,50,60), C15(50,140,60), C15(70,150,230), C15(230,100,80), C15(60,90,40), C15(120,60,180), C15(120,80,40),
    C15(220,170,40), C15(200,200,215), C15(245,245,255), C15(20,20,28), C15(250,220,60), C15(110,40,110), C15(240,130,180), C15(170,80,30) };
const char *const bug_swatch_name[BUG_NSWATCH] = { "STEEL","RUBY","LEAF","SKY","CORAL","MOSS","VIOLET","OAK","GOLD","PEARL","WHITE","BLACK","SUN","PLUM","ROSE","RUST" };
const char *const bug_scheme_name[BUG_NSCHEME] = { "ANALOG","COMPLEM","TRIAD","SPLIT","MONO","PASTEL","NEON","EARTH","NIGHT" };

/* ---- ready-made looks (ported from the GBA game's creator; its antenna and leg colours are now the outline) */
typedef struct { const char *name; UINT8 p[BP_N]; UINT16 c[BC_N]; } Preset;
static const Preset presets[BUG_NPRESET] = {
    {"CLASSIC", {1,0,0,0,0,0,0,1,0,0,0,0,0}, {C15(92,108,148),C15(200,200,215),C15(70,84,120),C15(240,240,250),C15(220,170,40),C15(190,225,255),C15(210,60,70),C15(14,16,28)}},
    {"LADYBUG", {0,1,0,2,1,1,0,1,0,0,2,0,0}, {C15(210,40,50),C15(20,20,28),C15(20,20,28),C15(240,240,250),C15(220,170,40),C15(230,235,255),C15(210,60,70),C15(14,14,20)}},
    {"WASP", {2,0,0,1,0,2,1,1,0,0,1,1,0}, {C15(250,210,50),C15(30,28,24),C15(30,28,24),C15(230,60,40),C15(220,170,40),C15(220,240,255),C15(30,28,24),C15(20,18,14)}},
    {"STAG", {2,0,2,0,0,3,2,2,1,0,0,0,0}, {C15(110,60,30),C15(150,100,50),C15(70,40,20),C15(240,240,250),C15(220,180,90),C15(200,180,150),C15(150,100,50),C15(30,16,8)}},
    {"FIREFLY", {2,0,1,3,5,0,1,1,0,0,1,0,0}, {C15(40,36,60),C15(250,230,70),C15(250,150,40),C15(255,255,200),C15(220,170,40),C15(255,250,190),C15(250,150,40),C15(14,12,24)}},
    {"MOTH", {1,0,1,8,1,0,3,0,2,0,3,0,0}, {C15(210,200,215),C15(150,130,170),C15(120,100,130),C15(40,30,50),C15(220,170,40),C15(200,190,170),C15(150,130,170),C15(50,40,60)}},
    {"FLUTTER", {0,0,3,0,1,1,4,1,0,0,3,0,0}, {C15(40,40,60),C15(250,250,250),C15(30,30,40),C15(240,240,250),C15(220,170,40),C15(250,140,40),C15(210,60,70),C15(14,14,20)}},
    {"RHINO", {2,1,2,0,0,0,1,1,1,3,0,0,0}, {C15(90,60,40),C15(150,110,80),C15(60,40,26),C15(240,240,250),C15(170,120,70),C15(200,180,150),C15(150,110,80),C15(24,14,8)}},
    {"UNICORN", {1,0,3,11,5,1,0,1,0,4,5,3,0}, {C15(245,245,255),C15(250,160,210),C15(250,220,235),C15(120,90,220),C15(250,220,120),C15(200,230,255),C15(250,160,210),C15(120,90,160)}},
    {"KNIGHT", {1,1,2,0,4,2,1,1,3,0,0,0,10}, {C15(170,175,195),C15(190,40,50),C15(120,125,145),C15(250,250,250),C15(220,170,40),C15(190,225,255),C15(200,200,215),C15(20,20,30)}},
    {"PIRATE", {1,0,0,1,7,1,2,1,3,0,0,4,2}, {C15(240,240,240),C15(190,40,40),C15(210,170,130),C15(250,250,250),C15(220,170,40),C15(190,225,255),C15(30,30,30),C15(20,14,10)}},
    {"ROYAL", {1,0,3,9,1,1,4,1,0,0,0,0,4}, {C15(120,40,150),C15(250,210,60),C15(250,210,170),C15(60,40,20),C15(250,210,60),C15(190,225,255),C15(250,210,60),C15(40,14,50)}},
    {"PARTY", {0,0,0,6,2,1,5,1,0,0,1,0,5}, {C15(80,200,240),C15(250,240,80),C15(250,200,90),C15(250,250,250),C15(220,170,40),C15(250,160,220),C15(240,80,160),C15(20,20,40)}},
    {"CHEF", {1,0,1,0,0,1,0,1,0,0,0,0,9}, {C15(240,140,60),C15(250,250,250),C15(250,210,170),C15(40,30,20),C15(220,170,40),C15(190,225,255),C15(250,250,250),C15(40,26,16)}},
    {"DEMON", {2,3,2,8,4,4,6,2,2,2,4,1,0}, {C15(40,20,30),C15(200,40,40),C15(200,40,40),C15(250,220,60),C15(240,230,210),C15(120,30,40),C15(200,40,40),C15(10,4,8)}},
    {"GHOST", {1,1,1,0,6,0,7,0,0,0,5,0,7}, {C15(220,225,240),C15(245,245,255),C15(245,245,255),C15(90,100,200),C15(220,170,40),C15(220,235,255),C15(250,230,120),C15(120,130,170)}},
    {"ROBOT", {1,2,2,4,7,2,4,2,1,0,0,0,10}, {C15(150,160,170),C15(80,240,250),C15(110,120,130),C15(80,240,250),C15(220,170,40),C15(190,225,255),C15(170,175,185),C15(30,34,40)}},
    {"VIKING", {2,4,0,3,4,0,1,2,3,0,0,0,11}, {C15(150,90,50),C15(220,200,120),C15(240,200,120),C15(70,120,200),C15(240,230,210),C15(190,225,255),C15(170,175,195),C15(40,24,10)}}
};
const char *bug_preset_name(UINT8 i){ return presets[i % BUG_NPRESET].name; }
void bug_preset(BugLook *l, UINT8 i, UINT16 lock_parts, UINT8 lock_cols){
    const Preset *p = &presets[i % BUG_NPRESET]; UINT8 k;
    for(k = 0; k < BP_N; k++) if(!((lock_parts >> k) & 1)) l->pt[k] = p->p[k];
    for(k = 0; k < BC_N; k++) if(!((lock_cols >> k) & 1)) l->col[k] = p->c[k];
}
void bug_default(BugLook *l){ memset(l, 0, sizeof *l); bug_preset(l, 0, 0, 0); }
void bug_clamp(BugLook *l){ UINT8 k; for(k = 0; k < BP_N; k++) if(l->pt[k] >= bug_part_n[k]) l->pt[k] = 0; for(k = 0; k < BC_N; k++) l->col[k] &= 0x7FFF; }
UINT8 bug_col_on(const BugLook *l, UINT8 c){
    switch(c){ case BC_MARK: return l->pt[BP_PAT] != 0; case BC_HORN: return l->pt[BP_HORN] != 0;
               case BC_WING: return l->pt[BP_WING] != 0; case BC_HAT: return l->pt[BP_HAT] != 0; default: return 1; }
}

/* ---- colour tools */
static UINT16 rng = 0xACE1;
void  bug_rand_seed(UINT16 s){ rng = s ? s : 0xACE1; }
UINT8 bug_rand(UINT8 n){ rng = rng * 25173u + 13849u; return n ? (UINT8)((rng >> 8) % n) : 0; }

UINT16 bug_hsv(UINT16 h, UINT16 s, UINT16 v){
    UINT16 i, f, p, q, t, r, g, b;
    h %= 360; if(s > 255) s = 255; if(v > 255) v = 255;
    i = h / 60; f = (h % 60) * 255 / 60;
    p = v * (255 - s) / 255; q = v * (255 - s * f / 255) / 255; t = v * (255 - s * (255 - f) / 255) / 255;
    switch(i){ case 0: r=v; g=t; b=p; break; case 1: r=q; g=v; b=p; break; case 2: r=p; g=v; b=t; break;
               case 3: r=p; g=q; b=v; break; case 4: r=t; g=p; b=v; break; default: r=v; g=p; b=q; }
    return (UINT16)((r >> 3) | ((g >> 3) << 5) | (b >> 3 << 10));
}
static UINT16 hue_of(UINT16 c){
    INT16 r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3, mx = r > g ? r : g, mn = r < g ? r : g, d, h = 0;
    if(b > mx) mx = b;
    if(b < mn) mn = b;
    d = mx - mn;
    if(d){ if(mx == r) h = 60 * (g - b) / d; else if(mx == g) h = 120 + 60 * (b - r) / d; else h = 240 + 60 * (r - g) / d; if(h < 0) h += 360; }
    return (UINT16)h;
}
static const INT16 sc_dh[9][6] = {{30,-30,60,-60,15,-15},{180,0,180,0,180,0},{120,240,120,240,0,180},{150,210,150,210,180,0},{0,0,0,0,0,0},{0,40,80,120,200,300},{0,60,120,180,240,300},{25,35,95,15,45,200},{250,270,230,300,210,50}};
static const UINT8 sc_s[9][6] = {{200,200,170,170,220,220},{210,160,230,120,190,255},{220,220,170,170,255,140},{200,220,180,160,230,120},{230,200,170,140,110,80},{70,80,60,70,90,60},{255,255,255,255,255,255},{160,140,120,180,100,70},{200,180,220,150,240,200}};
static const UINT8 sc_v[9][6] = {{210,160,230,140,190,120},{220,150,120,230,170,90},{220,200,150,130,100,240},{220,150,230,120,190,90},{90,130,170,210,240,255},{250,245,250,240,250,245},{255,255,255,255,255,255},{110,150,130,90,190,160},{60,90,50,110,70,230}};
void bug_scheme(BugLook *l, UINT8 anchor, UINT8 lock_cols, UINT8 sch){
    UINT16 h0 = hue_of(l->col[anchor]); UINT8 k = 0, i;
    sch %= BUG_NSCHEME;
    for(i = 0; i < BC_N; i++) if(i != anchor && !((lock_cols >> i) & 1)){
        l->col[i] = bug_hsv(sch >= 7 ? (UINT16)sc_dh[sch][k % 6] : (UINT16)((INT16)h0 + sc_dh[sch][k % 6] + 360), sc_s[sch][k % 6], sc_v[sch][k % 6]);
        k++;
    }
}
void bug_surprise(BugLook *l, UINT16 lock_parts, UINT8 lock_cols){
    UINT8 k, a = 255;
    for(k = 0; k < BP_N; k++) if(!((lock_parts >> k) & 1)) l->pt[k] = bug_rand(bug_part_n[k]);
    for(k = 0; k < BC_N; k++) if(!((lock_cols >> k) & 1)){ a = k; break; }
    if(a != 255){ l->col[a] = bug_hsv(bug_rand(180) * 2, 140 + bug_rand(116), 120 + bug_rand(136)); bug_scheme(l, a, lock_cols, bug_rand(BUG_NSCHEME)); }
}

/* ---- palettes */
static UINT16 shade15(UINT16 c, UINT8 k){ UINT16 r = (c & 31) * k / 16, g = ((c >> 5) & 31) * k / 16, b = ((c >> 10) & 31) * k / 16; if(r > 31) r = 31; if(g > 31) g = 31; if(b > 31) b = 31; return r | (g << 5) | (b << 10); }
static UINT16 lift15(UINT16 c, UINT8 k){ UINT16 r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31; r += (31 - r) * k / 16; g += (31 - g) * k / 16; b += (31 - b) * k / 16; return r | (g << 5) | (b << 10); }
static UINT16 mix15(UINT16 a, UINT16 b, UINT8 t){
    UINT16 r = ((a & 31) * (16 - t) + (b & 31) * t) >> 4, g = (((a >> 5) & 31) * (16 - t) + ((b >> 5) & 31) * t) >> 4, bl = (((a >> 10) & 31) * (16 - t) + ((b >> 10) & 31) * t) >> 4;
    return r | (g << 5) | (bl << 10);
}
void bug_palettes(const BugLook *l, UINT16 *p){
    UINT16 sh = l->col[BC_SHELL], wg = l->col[BC_WING], shine = lift15(sh, 11);
    UINT8 k;
    for(k = 0; k < 16; k++) p[k] = 0;
    switch(l->pt[BP_FINISH]){
        case 1: sh = shade15(sh, 13); break;
        case 2: sh = mix15(sh, C15(170,170,190), 7); shine = lift15(sh, 12); break;
        case 3: sh = mix15(sh, wg, 4); shine = lift15(wg, 12); break;
    }
    p[1] = l->col[BC_LINE]; p[2] = sh; p[3] = l->pt[BP_PAT] ? l->col[BC_MARK] : shine;
    p[5] = l->col[BC_LINE]; p[6] = l->col[BC_HEAD]; p[7] = l->col[BC_EYE];
    p[9] = l->col[BC_LINE]; p[10] = l->col[BC_HORN]; p[11] = l->col[BC_HAT];
    p[13] = shade15(wg, 9); p[14] = wg; p[15] = lift15(wg, 9);
}

/* ---- the picture */
BugSet bug_set;
static UINT8 bpx[BUG_BW * BUG_BH];
#define PX(x, y) bpx[(UINT16)(y) * BUG_BW + (x)]
#define HEAD_UP 6
#define HEAD_LO 22

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
}
static void draw_legs(const BugLook *l, UINT8 x0, UINT8 W, UINT8 f){
    UINT8 n = 2 + l->pt[BP_LEGS], i, lx, by, y, len, lift, k, st = l->pt[BP_LEGST];
    for(i = 0; i < n; i++){
        lx = x0 + 3 + (UINT8)(((UINT16)(W - 7) * i) / (n - 1));
        lift = (i + f) & 1; len = 3 - lift;
        by = 16;
        for(y = 0; y < 12; y++) if(PX(lx, 16 + y)) by = 16 + y;
        lx += lift;
        for(k = 1; k <= len; k++){
            y = by + k;
            if(!PX(lx, y)) PX(lx, y) = 1;
            if(st == 1 && !PX(lx + 1, y)) PX(lx + 1, y) = 1;
            if(st == 2 && ((k == 2 && lx > x0 && !PX(lx - 1, y)) )) PX(lx - 1, y) = 1;
            if(st == 2 && k == 1 && !PX(lx + 1, y)) PX(lx + 1, y) = 1;
            if(st == 3 && k == len){ if(!PX(lx + 1, y)) PX(lx + 1, y) = 1; if(!PX(lx + 2, y)) PX(lx + 2, y) = 1; }
        }
    }
}

static UINT8 cell_x(UINT8 c, UINT8 tc, UINT8 hc){ return c < 4 ? (tc + c) * 8 : c < 8 ? (tc + c - 4) * 8 : c == 8 ? 0 : hc * 8; }
static UINT8 cell_y(UINT8 c){ return c < 4 ? 0 : c < 9 ? 16 : c == 9 ? HEAD_UP : HEAD_LO; }
UINT8 bug_cell_pal(UINT8 c){ return c < 4 ? 3 : c < 9 ? 0 : c == 9 ? 2 : 1; }

static void draw_frame(const BugLook *l, UINT8 f, UINT8 tc, UINT8 hc, UINT8 W, UINT8 *used){
    UINT8 c, r, x, v, lo, hi, nz, *t, x0, y0;
    memset(bpx, 0, sizeof bpx);
    if(l->pt[BP_WING]) stamp(&bp_wings[l->pt[BP_WING] * 3 + l->pt[BP_LENGTH]], tc * 8, 0);
    if(tc) stamp(&bp_tail[l->pt[BP_TAIL]], 0, 16);
    draw_shell(l, tc * 8, W);
    draw_legs(l, tc * 8, W, f);
    stamp(&bp_head[0], hc * 8, HEAD_LO);
    stamp(&bp_eye[l->pt[BP_EYE]], hc * 8, HEAD_LO);
    stamp(&bp_mouth[l->pt[BP_MOUTH]], hc * 8, HEAD_LO);
    stamp(&bp_ant[l->pt[BP_ANT]], hc * 8, HEAD_UP);
    stamp(&bp_horn[l->pt[BP_HORN]], hc * 8, HEAD_UP);
    stamp(&bp_hat[l->pt[BP_HAT]], hc * 8, HEAD_UP);
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

void bug_build(const BugLook *l){
    UINT8 W = 16 + 8 * l->pt[BP_LENGTH], tc = l->pt[BP_TAIL] ? 1 : 0, hc = tc + (W >> 3), c, n = 0;
    UINT8 used[BUG_MAXCELL];
    memset(used, 0, sizeof used);
    draw_frame(l, 1, tc, hc, W, used);
    draw_frame(l, 0, tc, hc, W, used);                   /* frame 0 last: it stays in bpx for previews */
    for(c = 0; c < BUG_MAXCELL; c++) if(used[c]){
        bug_set.s[n].x = cell_x(c, tc, hc); bug_set.s[n].y = cell_y(c); bug_set.s[n].cell = c; bug_set.s[n].pal = bug_cell_pal(c); n++;
    }
    bug_set.n = n; bug_set.w = (hc + 1) * 8;
}
UINT8 bug_pixel(UINT8 x, UINT8 y){ return (x < BUG_BW && y < BUG_BH) ? PX(x, y) : 0; }
UINT8 bug_peak(void){
    UINT8 cnt[BUG_BH + 16], i, y, peak = 0;
    memset(cnt, 0, sizeof cnt);
    for(i = 0; i < bug_set.n; i++) for(y = 0; y < 16; y++) if(++cnt[bug_set.s[i].y + y] > peak) peak = cnt[bug_set.s[i].y + y];
    return peak;
}
