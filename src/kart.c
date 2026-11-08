#pragma bank 24
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "bug.h"
#include "spell.h"
#include "raster.h"
#include "kart.h"
#include "font.h"
#define KART_ART_NO_GFX         /* (the tiles and sprites are loaded from bank 25: kart_gfx) */
#include "kart_art.h"          /* the road tables: made by tools/make_kart.py */

/* KART WORLD: a 3D race. 3 laps, 3 rivals. Units: a lap is 2048 "metres"; positions keep 8 more bits (so 1 metre = 256). */
#define LAPM   2048U
#define VMAX   256

#define HZ     64                              /* first road scanline */
#ifndef KX_GAS
#define KX_GAS 0
#endif
#ifndef KX_WAIT
#define KX_WAIT 40
#endif
#ifndef KX_T0
#define KX_T0 0
#define KX_L0 0
#endif
#define C15(r,g,b) ((UINT16)(((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))
#define NSCEN  52                              /* trees and rocks along a lap, one every 40 metres */


/* three tracks: the bend of each 128 m of road (+ right), and the colours of the land */
static const INT8 segs[96] = {
     0,  0, 10, 28, 28, 10,   0, -14, -30, -30, -14,   0, 18, 18,   0,  -8,           /* MEADOW: easy */
     0, 12, 26, 26, 12,  0, -10, -22, -22,  -8,   0,  20,  29,  29,  12,   0,           /* DESERT: wide bends */
    10, 29, 29, -8, -29, -29,  0,  24,  24, -12, -26, -26,   6,  29,  12, -10,         /* SNOW PEAK: twisty */
    14, 29, 29,  6, -12, -29, -29, -20,   0,  24,  29,  29,  -6, -24, -29,   8,         /* LAVA RIM: hard, with a hairpin pair */
    16, 16, 16,  8,   0, -16, -16, -16,  -8,   0,  22,  22,   0, -22, -22,   0,         /* CANDY COAST: long sweepers */
    29, -29, 29, -12, -29, 29,  12, -29, -29,  20,  29, -20, -29,  12,  29,  -6 };       /* NIGHT CITY: the hardest */
static const UINT16 tcol[6][6] = {                                                    /* grass, grass 2, sky, low sky, mountain, snow cap */
    { C15(70,175,70),  C15(45,145,55),  C15(70,130,240),  C15(150,200,255), C15(95,105,160),  C15(215,225,245) },
    { C15(225,190,110), C15(205,168,88), C15(240,150,70),  C15(255,210,140), C15(170,90,60),   C15(230,190,150) },
    { C15(235,242,250), C15(205,218,235), C15(120,150,200), C15(190,210,235), C15(110,120,150), C15(255,255,255) },
    { C15(95,35,25),    C15(70,25,18),    C15(255,110,30),  C15(255,180,70),  C15(50,30,35),    C15(255,140,40) },
    { C15(150,230,190), C15(120,210,170), C15(255,150,200), C15(255,215,235), C15(190,150,225), C15(255,255,255) },
    { C15(30,62,62),    C15(22,48,50),    C15(15,15,70),    C15(60,60,125),   C15(45,40,90),    C15(210,210,255) } };
static const UINT8 scn_r3[NSCEN] = { 0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0 };   /* kq % 3 */
static const UINT8 tdif[6] = { 0, 6, 12, 14, 8, 20 };                                           /* the rivals get quicker */
static const INT8 wob[8] = { 0, 10, 18, 10, 0, -10, -18, -10 };
static const INT8 lane[NRIV] = { -70, 0, 70 };
static const UINT8 vrv[NRIV] = { 232, 224, 214 };
static const UINT16 BXM[3] = { 300, 900, 1500 };                                      /* item boxes: where in the lap, and which side */
static const INT8  BXL[3] = { -80, 80, 0 };
#define BXMR(b) (rev ? (UINT16)(LAPM - BXM[b]) : BXM[b])        /* (driven backwards the boxes are where the road has them from the other end) */

static const INT8 *sg;
static INT8 rsg[16];                            /* the road driven the other way round: the bends reversed and mirrored */
static UINT16 kp[12];                           /* this track's palettes */
UINT8  rivals_on; UINT8 kart_trk; UINT8 kart_laps = 3, kart_rev = 0;
#define trk kart_trk
extern volatile UINT8 snd_song;
UINT8 kart_nlaps; static UINT8 rev, old_song;
#define nlaps kart_nlaps
static UINT16 bst;     /* bst: the record shown while you race alone */

static UINT16 tm[1 + NRIV];                     /* distance driven, whole metres; 0 is you */
static UINT8  tf[1 + NRIV];                     /* ...and the 1/256 of a metre that goes with it */
static INT16  vel[1 + NRIV], lx[1 + NRIV];     /* speed (1/256 m a frame) and the sideways spot (128 = the road's edge) */
static const INT8 *brow;                         /* the bend of every pair of road lines for the curve you are on: read straight from the ROM table */
#define bend_at(i) (brow[(i) >> 1])
static UINT8  tab[144];
static UINT8  stripe, sky_x;
UINT16 tot_t, best_run; static UINT16 stripe_acc, lap_t, frame;
static INT16  head;
static UINT8  lapn, bump_cd;
static UINT16 nextlap;
/* items */
typedef struct { UINT16 m; INT16 lane; UINT16 life; UINT8 owner, f; INT8 tg; } Shell;
typedef struct { UINT16 m; INT16 lane; UINT16 life; } Oil;
static Shell  shl;
static Oil    oil[3];
static UINT8  hold, boost_t, drift_t, got, stun[1 + NRIV], shield_t, gh_on; static UINT16 ghv;
static UINT16 aicd[1 + NRIV], rs = 0xACE1;
/* what is kept on the cartridge (after the Extra block of this slot): best trial times, the best cup */
KSave ks;
UINT8 kart_rk[1 + NRIV];

static UINT8 krnd(void){ rs ^= rs << 7; rs ^= rs >> 9; rs ^= rs << 8; return (UINT8)rs; }
static UINT8 ks_sum(void){ const UINT8 *p = (const UINT8 *)&ks; UINT8 i, s = 0x5A; for(i = 0; i < sizeof ks - 1; i++) s ^= (UINT8)(p[i] + i); return s; }
void kart_ks_save(void) BANKED {
    UINT8 *p = (UINT8 *)&ks, i;
    ks.seed = sv.seed; ks.chk = ks_sum();
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof ks; i++) *(volatile UINT8 *)(0xA000U + KS_OFF + i) = p[i];
    DISABLE_RAM;
}

static void add_pos(UINT8 k, INT16 v){
    UINT16 s = (UINT16)tf[k] + ((UINT16)v & 255);
    tf[k] = (UINT8)s; tm[k] += (UINT16)(v >> 8) + (s >> 8);
}
static INT16 curve_at(UINT16 m, UINT8 fr){
    UINT8 i = (UINT8)(m >> 7) & 15, f = (UINT8)((m << 1) | (fr >> 7));
    INT8 a = sg[i], b = sg[(i + 1) & 15];
    return a + (INT16)((((INT16)b - a) * (INT16)f) >> 8);
}

static void set_stripes(void){                                              /* (right after VBlank starts: straight into the palette, no waiting for the LCD, which the road interrupt would never let us have) */
    const UINT16 *a = &kp[stripe ? 4 : 0], *b = &kp[stripe ? 0 : 4];
    UINT8 i;
    BCPS_REG = 0x80 | 32;
    for(i = 0; i < 4; i++){ BCPD_REG = (UINT8)a[i]; BCPD_REG = (UINT8)(a[i] >> 8); }
    for(i = 0; i < 4; i++){ BCPD_REG = (UINT8)b[i]; BCPD_REG = (UINT8)(b[i] >> 8); }
}
static void track_pal(UINT8 t){
    UINT8 i;
    for(i = 0; i < 12; i++) kp[i] = kt_pal[i];
    kp[0] = tcol[t][0]; kp[4] = tcol[t][1];
    kp[8] = tcol[t][2]; kp[9] = tcol[t][3]; kp[10] = tcol[t][4]; kp[11] = tcol[t][5];
    set_bkg_palette(6, 1, &kp[8]);
}

static void put_obj(UINT8 slot, UINT8 unit, UINT8 cols, UINT8 rows, UINT8 pal, INT16 cx, INT16 by){      /* (adds only, and straight into the sprite table: a multiply or a call costs more than a sprite) */
    OAM_item_t *o = &shadow_OAM[slot]; UINT8 tile = (UINT8)(128 + (unit << 1)), c, r;
    INT16 x = cx - (INT16)(cols << 2), y0 = by - (INT16)(rows << 4), y;
    for(c = 0; c < cols; c++, x += 8){
        if(x < -7 || x > 159){ for(r = 0; r < rows; r++, o++, tile += 2) o->y = 0; continue; }
        y = y0;
        for(r = 0; r < rows; r++, y += 16, o++, tile += 2){
            if(y < 16 || y > 143){ o->y = 0; continue; }
            o->tile = tile; o->prop = pal; o->x = (UINT8)(x + 8); o->y = (UINT8)(y + 16);
        }
    }
}
static void hide_from(UINT8 s){ for(; s < 40; s++) shadow_OAM[s].y = 0; }
/* one flat object on the road (an item box, an oil slick, a shell): three sizes, three sprites at most, from slot o */
static void show3(UINT8 o, INT16 dz, INT16 lat, UINT8 u0, UINT8 u1, UINT8 u2, UINT8 pal, INT16 px){
    UINT8 y, i; INT16 x;
    if(dz < 2 || dz > 90){ shadow_OAM[o].y = 0; shadow_OAM[o + 1].y = 0; shadow_OAM[o + 2].y = 0; return; }
    y = kt_y[dz]; i = y - HZ;
    x = 80 + (((lat - px) * kt_hw[i]) >> 7) + bend_at(i);
    if(dz < 18) put_obj(o, u2, 3, 1, pal, x, y);
    else if(dz < 44){ put_obj(o, u1, 2, 1, pal, x, y); shadow_OAM[o + 2].y = 0; }
    else { put_obj(o, u0, 1, 1, pal, x, y); shadow_OAM[o + 1].y = 0; shadow_OAM[o + 2].y = 0; }
}

/* the HUD (two BG rows) and the banner (a box in the window) are made in RAM during the frame and copied into video memory in VBlank (vflush),
   because a write in the middle of the picture is lost or garbled (the road interrupt fires on every line) */
static UINT8 hb[2][20], hbd, hbp, hbpd, bb[16], bbd, bnt, winreq, winnow;
static void fill(UINT8 *d, const char *s, UINT8 w){ UINT8 i = 0; while(i < w && s[i]){ d[i] = (UINT8)s[i] & 0x7F; i++; } while(i < w) d[i++] = T_BLANK; }
static void banner(const char *s){ fill(bb, s, 16); bbd = 1; winreq = 1; bnt = 90; }
static void hud_row(UINT8 r, const char *s){
    UINT8 t[20], i;
    fill(t, s, 20);
    for(i = 0; i < 20; i++) if(t[i] != hb[r][i]){ memcpy(hb[r], t, 20); hbd |= 1 << r; return; }
}
static void vflush(void){                                                       /* call right after wait_vbl_done */
    UINT8 i; volatile UINT8 *p;
    if(hbd){
        for(i = 0; i < 20; i++){ if(hbd & 1) ((volatile UINT8 *)0x9800)[i] = hb[0][i]; if(hbd & 2) ((volatile UINT8 *)0x9820)[i] = hb[1][i]; }
        hbd = 0;
    }
    if(hbpd){ p = (volatile UINT8 *)0x9820; VBK_REG = 1; for(i = 0; i < 20; i++) p[i] = hbp; VBK_REG = 0; hbpd = 0; }
    if(bbd){ p = (volatile UINT8 *)(0x9C00 + 34); for(i = 0; i < 16; i++) p[i] = bb[i]; bbd = 0; }
    if(winreq != winnow){ winnow = winreq; if(winnow) SHOW_WIN; else HIDE_WIN; }
}
static char *ftime(char *d, UINT16 t){                          /* m:ss.d from 60ths of a second */
    UINT16 s = t / 60;
    d = u16s(d, s / 60); d = scat(d, ":"); if(s % 60 < 10) d = scat(d, "0");
    d = u16s(d, s % 60); d = scat(d, "."); d = u16s(d, (t % 60) / 6); return d;
}
static const char itnm[9][5] = { {' ',' ',' ',' ',' '}, {'B','O','O','S','T'}, {'S','H','E','L','L'}, {'O','I','L',' ',' '}, {'T','U','R','B','O'}, {'S','P','I','N','!'}, {'D','R','I','F','T'}, {'M','E','G','A','!'}, {'S','H','I','E','L'} };   /* the last field of the second row: your item, or what the kart is doing */
/* the race clock is kept by counting (6 frames a tenth, 10 tenths a second...), so the HUD never has to divide: a division on this CPU costs 300 to 600 cycles */
static UINT8 tmin, ts10, ts1, tten, tc6, clk_d;
static void clk_tick(void){ if(++tc6 >= 6){ tc6 = 0; clk_d = 1; if(++tten >= 10){ tten = 0; if(++ts1 >= 10){ ts1 = 0; if(++ts10 >= 6){ ts10 = 0; if(tmin < 9) tmin++; } } } } }
static UINT8 hkey = 255, hhold; static UINT16 hsp;
static void n3(UINT8 *d, UINT16 v){                              /* 0..999 as three characters, leading spaces, by subtracting */
    UINT8 h = 0, t = 0;
    while(v >= 100){ v -= 100; h++; }
    while(v >= 10){ v -= 10; t++; }
    d[0] = h ? '0' + h : ' '; d[1] = (h || t) ? '0' + t : ' '; d[2] = '0' + (UINT8)v;
}
static void hud(UINT8 place){
    UINT8 i, key, fld;
    key = (UINT8)(lapn | (place << 3));                                                                   /* the top row changes only now and then */
    if(key != hkey){
        char b[32], *d;
        hkey = key;
        d = scat(b, "LAP "); d = u16s(d, lapn < nlaps ? lapn + 1 : nlaps);
        if(rivals_on){ d = scat(d, "/"); d = u16s(d, nlaps); d = scat(d, " PLACE "); d = u16s(d, place); d = scat(d, "/4"); }
        else { UINT16 bt = bst; d = scat(d, "/"); d = u16s(d, nlaps); d = scat(d, " BEST "); if(bt) d = ftime(d, bt); else d = scat(d, "-:--.-"); }
        *d = 0;
        hud_row(0, b);
    }
    { UINT16 sp = ((UINT16)vel[0] * 3) >> 2;                       /* the second row is patched cell by cell: only what changed */
      if(sp != hsp){ hsp = sp; n3(&hb[1][4], sp); hbd |= 2; }
      if(clk_d){ clk_d = 0; hb[1][8] = '0' + tmin; hb[1][10] = '0' + ts10; hb[1][11] = '0' + ts1; hb[1][13] = '0' + tten; hbd |= 2; }
      fld = boost_t ? 4 : stun[0] ? 5 : hold ? hold : shield_t ? 8 : drift_t > 30 ? 6 : 0;
      if(fld != hhold){ hhold = fld; for(i = 0; i < 5; i++) hb[1][15 + i] = itnm[fld][i]; hbd |= 2; } }
    { UINT8 pp = (hold || boost_t) ? TXT_RED : TXT_WHITE; if(pp != hbp){ hbp = pp; hbpd = 1; } }
}

static UINT8 rank_of(UINT8 j, const UINT16 *s){                   /* where racer j finishes: ties go to the rivals (lower number first) */
    UINT8 i, r = 1;
    for(i = 0; i <= NRIV; i++){
        if(i == j) continue;
        if(s[i] > s[j]) r++;
        else if(s[i] == s[j] && i != 0 && (j == 0 || i < j)) r++;
    }
    return r;
}
static UINT8 place_now(void){
    UINT8 k, p = 1;
    if(!rivals_on) return 1;
    for(k = 1; k <= NRIV; k++) if(tm[k] > tm[0]) p++;
    return p;
}

static void hit_kart(UINT8 k, UINT8 big){
    if(k == 0 && shield_t){ shield_t = shield_t > 90 ? shield_t - 90 : 1; sfx(SFX_BACK); return; }       /* the SHIELD takes the hit (and part of its time) */
    stun[k] = big ? 56 : 40;
    if(k == 0){ vel[0] >>= 2; sfx(SFX_HURT); } else vel[k] = 0;
}
static void drop_oil(UINT16 m, INT16 l){
    UINT8 b; for(b = 0; b < 3 && oil[b].life; b++) ;
    if(b == 3) b = 0;
    oil[b].m = m; oil[b].lane = l; oil[b].life = 900;
}
static void use_item(void){
    if(hold == 1){ boost_t = 70; sfx(SFX_LASER); }
    else if(hold == 7){ boost_t = 150; sfx(SFX_LEVEL); }
    else if(hold == 8){ shield_t = 420; sfx(SFX_SPARK); }
    else if(hold == 2){
        UINT8 k, tg = 255; INT16 g, best = 400;
        for(k = 1; k <= NRIV && rivals_on; k++){ g = (INT16)(tm[k] - tm[0]); if(g > 0 && g < best){ best = g; tg = k; } }
        shl.m = tm[0] + 10; shl.lane = lx[0]; shl.owner = 0; shl.life = 240; shl.f = 0; shl.tg = (INT8)tg; sfx(SFX_FIRE);
    } else if(hold == 3){ drop_oil(tm[0] - 9, lx[0]); sfx(SFX_SPLASH); }
    hold = 0;
}

/* once a frame: boxes, shells and oil, for you and the rivals */
static void items_tick(UINT8 go){
    UINT8 k, b; INT16 dz, dx; UINT16 pm = tm[0] & (LAPM - 1);
    if(shield_t) shield_t--;
    if(boost_t){ boost_t--; rum_hum(110); }                                      /* (a steady rumble while the turbo burns, and in a long drift) */
    if(drift_t > 30) rum_hum(60);
    for(k = 0; k <= NRIV; k++) if(stun[k]) stun[k]--;
    if(!go) return;
    for(b = 0; b < 3; b++) if(!(got & (1 << b)) && !hold){                      /* the boxes */
        dz = (INT16)BXMR(b) - (INT16)pm; dx = lx[0] - BXL[b];
        if(dz <= 3 && dz >= -12 && dx > -44 && dx < 44){ UINT8 r = krnd() & 7; got |= 1 << b; hold = r < 2 ? 1 : r < 4 ? 2 : r < 5 ? 3 : r < 7 ? 7 : 8; if(r > 4 && place_now() == 1 && rivals_on && (krnd() & 1)) hold = 3; sfx(SFX_PICK); }
    }
    for(b = 0; b < 3; b++) if(oil[b].life){                                       /* oil: whoever drives into it spins */
        oil[b].life--;
        for(k = 0; k <= NRIV; k++){
            if(k && !rivals_on) break;
            dz = (INT16)(tm[k] - oil[b].m); dx = lx[k] - oil[b].lane;
            if(dz > -6 && dz < 6 && dx > -30 && dx < 30 && !stun[k]){ hit_kart(k, 0); oil[b].life = 0; if(k) sfx(SFX_SPLASH); break; }
        }
    }
    if(shl.life){                                                              /* a shell: quicker than a kart, it steers toward its kart a little */
        shl.life--; shl.f += 86; shl.m += 1 + (shl.f < 86);
        if(shl.tg >= 0){ INT16 tl = lx[(UINT8)shl.tg]; if(shl.lane < tl - 3) shl.lane += 3; else if(shl.lane > tl + 3) shl.lane -= 3; }
        for(k = 0; k <= NRIV; k++){
            if(k == shl.owner) continue;
            if(k && !rivals_on) break;
            dz = (INT16)(tm[k] - shl.m); dx = lx[k] - shl.lane;
            if(dz > -7 && dz < 7 && dx > -34 && dx < 34){ hit_kart(k, 1); shl.life = 0; if(k) sfx(SFX_BOOM); break; }
        }
    }
    if(rivals_on) for(k = 1; k <= NRIV; k++){                                  /* the rivals play too */
        if(aicd[k]){ aicd[k]--; continue; }
        aicd[k] = 300 + ((UINT16)krnd() << 1);
        dz = (INT16)(tm[0] - tm[k]);
        if(dz > 20 && dz < 320 && !shl.life && !stun[k]){ shl.m = tm[k] + 8; shl.lane = lx[k]; shl.owner = k; shl.life = 260; shl.f = 0; shl.tg = 0; sfx(SFX_FIRE); }
        else if(dz < -10 && dz > -200) drop_oil(tm[k] - 10, lx[k]);
    }
}

/* one frame of the race: you, then the three rivals */
static void drive(UINT8 go){
    INT16 v = vel[0], cur = curve_at(tm[0], tf[0]), cap;
    INT8 dir = 0; UINT8 k, off = (lx[0] > 150 || lx[0] < -150);
    cap = off ? (boost_t ? 140 : 70) : (boost_t ? 400 : VMAX);
    if(go && !stun[0]){
        if(HELD(J_A) || KX_GAS) v += boost_t ? 12 : 4; else v -= 2;
        if(HELD(J_DOWN)) v -= 8;
        if(HELD(J_LEFT)) dir = -1;
        if(HELD(J_RIGHT)) dir = 1;
#ifdef KX_STEER
        dir = lx[0] > 20 ? -1 : lx[0] < -20 ? 1 : 0;
#endif
        if(HIT(J_B) && hold && !air_t) use_item();
        if(dir && v > 160 && HELD(J_A)){ if(drift_t < 250) drift_t++; }              /* a long, held bend charges a little turbo */
        else { if(drift_t > 60){ boost_t = 36; sfx(SFX_LASER); } drift_t = 0; }
    } else if(!go) v = 0; else { v -= (v >> 3) + 1; drift_t = 0; }
    if(v > cap){ v -= (v - cap > 8) ? 8 : (v - cap); }
    if(v < 0) v = 0;
    lx[0] += dir * ((v >> 6) + (v > 0 ? 1 : 0));
    lx[0] -= (cur * v) >> 12;
    if(lx[0] > 200) lx[0] = 200; if(lx[0] < -200) lx[0] = -200;
    if(kart_air((UINT16)(tm[0] & (LAPM - 1)), lx[0], v, HIT(J_B))){ boost_t = 60; sfx(SFX_LASER); }
    vel[0] = v; add_pos(0, v);
    head += (cur * v) >> 10;
    stripe_acc += (UINT16)v;
    if(stripe_acc >= 1536){ stripe_acc -= 1536; stripe ^= 1; }
    if(gh_on && go){ vel[NRIV] = ghv; add_pos(NRIV, ghv); lx[NRIV] = -30; }                    /* the GHOST: the third rival's car, at your best lap's pace, in a time trial */
    for(k = 1; k <= NRIV && rivals_on; k++){
        INT16 gap = (INT16)(tm[k] - tm[0]), cv = curve_at(tm[k], tf[k]), want, tl;
        if(cv < 0) cv = -cv;
        want = vrv[k - 1] + tdif[trk] - (cv >> 1);
        tl = lane[k - 1] + wob[(UINT8)(tm[k] >> 4) & 7];
        if(gap > 120) want -= 24; else if(gap < -120) want += 20;
        if(!go || stun[k]) want = 0;
        if(vel[k] < want) vel[k] += 3; else if(vel[k] > want) vel[k] -= 3;
        add_pos(k, vel[k]);
        if(lx[k] < tl) lx[k] += 2; else if(lx[k] > tl) lx[k] -= 2;
        if(gap > -7 && gap < 7){                                            /* a bump */
            INT16 dx = lx[k] - lx[0];
            if(dx > -26 && dx < 26){
                if(gap >= 0){ if(vel[0] > vel[k] - 30) vel[0] = vel[k] > 30 ? vel[k] - 30 : 0; } else if(vel[k] > 40) vel[k] -= 40;
                lx[0] += dx > 0 ? -3 : 3;
                if(!bump_cd){ bump_cd = 20; sfx(SFX_HURT); } } }
    }
    if(bump_cd) bump_cd--;
    items_tick(go);
}

/* the picture: the sideways slide of every road line, then the sprites. (No multiplies in the per-line loops: they only add.) */
static UINT8 tf_n;
static const INT8 *tf_row; static UINT8 *tf_tp; static INT16 tf_sh, tf_sd;                     /* (arguments of tab_fill, which is hand-written assembly: the C version cost a tenth of a frame) */
static void tab_fill(void) __naked {                                   /* tab[HZ + 2k], tab[HZ + 2k + 1] = (sh >> 8) - row[k], with sh += sd each k, for 40 pairs */
__asm
    push bc
    ld   hl, #_tf_row
    ld   a, (hl+)
    ld   e, a
    ld   a, (hl+)
    ld   d, a                    ; de = row
    ld   a, (hl+)
    ld   c, a
    ld   a, (hl+)
    ld   b, a                    ; bc = tp
    ld   a, (hl+)
    push af
    ld   a, (hl+)
    ld   h, a                    ; h = sh high ...
    pop  af
    ld   l, a                    ; hl = sh
    ld   a, #40
    ld   (_tf_n), a
00001$:
    ld   a, (de)                 ; bv
    inc  de
    cpl
    inc  a                       ; -bv
    add  a, h                    ; (sh >> 8) - bv
    ld   (bc), a
    inc  bc
    ld   (bc), a
    inc  bc
    push de
    ld   de, #_tf_sd
    ld   a, (de)
    add  a, l
    ld   l, a
    inc  de
    ld   a, (de)
    adc  a, h
    ld   h, a
    pop  de
    ld   a, (_tf_n)
    dec  a
    ld   (_tf_n), a
    jr   NZ, 00001$
    pop  bc
    ret
__endasm;
}
static void scene(void){
    UINT8 i, k, n = 21; INT16 cur = curve_at(tm[0] + 24, tf[0]), px = lx[0];
    UINT16 pm = tm[0] & (LAPM - 1);
    INT16 sh = px * 6, sh_d = (INT16)(((INT32)px * 466L) >> 7);              /* the sideways shift of the road (8.8): px * hw / 128, two lines at a time */
    if(cur < -30) cur = -30; if(cur > 29) cur = 29;
    brow = &kt_bv[(UINT8)(cur + 30) * 40U];
    tf_row = brow; tf_tp = &tab[HZ]; tf_sh = sh; tf_sd = sh_d; tab_fill();
    memset(tab, 0, 16); memset(&tab[16], sky_x = (UINT8)(head >> 4), HZ - 16);
    for(k = 1; k <= NRIV; k++){                                             /* the rivals ahead of you, bigger as they near */
        INT16 dz = (INT16)(tm[k] - tm[0]);
        UINT8 y, o = 3 * (k - 1) + 3;
        if((!rivals_on && !(gh_on && k == NRIV)) || dz < 1 || dz > 90){ for(i = 0; i < 3; i++) shadow_OAM[o + i].y = 0; continue; }
        y = kt_y[dz]; i = y - HZ;
        { INT16 x = 80 + (((lx[k] - px) * kt_hw[i]) >> 7) + bend_at(i);
          if(stun[k]) x += (frame & 2) ? 5 : -5;
          if(dz < 18) put_obj(o, KU_AI2, 3, 1, 1 + k, x, y);
          else if(dz < 44){ put_obj(o, KU_AI1, 2, 1, 1 + k, x, y); shadow_OAM[o + 2].y = 0; }
          else { put_obj(o, KU_AI0, 1, 1, 1 + k, x, y); shadow_OAM[o + 1].y = 0; shadow_OAM[o + 2].y = 0; } }
    }
    { UINT8 b, nb = 0; INT16 dz, bd = 999;                                    /* the nearest item box ahead */
      for(b = 0; b < 3; b++) if(!(got & (1 << b))){ dz = (INT16)BXMR(b) - (INT16)pm; if(dz < 0) dz += (INT16)LAPM; if(dz < bd){ bd = dz; nb = b; } }
      show3(12, bd, BXL[nb], KU_BOX0, KU_BOX1, KU_BOX2, 6, px);
      bd = 999;                                                              /* the nearest oil slick */
      for(b = 0; b < 3; b++) if(oil[b].life){ dz = (INT16)(oil[b].m - tm[0]); if(dz > 0 && dz < bd){ bd = dz; nb = b; } }
      if(bd < 999) show3(15, bd, oil[nb].lane, KU_OIL0, KU_OIL1, KU_OIL2, 7, px); else show3(15, 0, 0, 0, 0, 0, 7, px);
      { INT16 pl, pd = kart_pad(pm, &pl); show3(37, pd, pl, KU_OIL0, KU_OIL1, KU_OIL2, 6, px); }
      if(shl.life) show3(18, (INT16)(shl.m - tm[0]), shl.lane, KU_SH0, KU_SH1, KU_SH2, 7, px); else show3(18, 0, 0, 0, 0, 0, 7, px);
    }
    { UINT8 n0 = (UINT8)(pm / 40U), q; INT16 dz;
      for(q = 0; q < 4 && n < 37; q++){                                   /* trees and rocks along the road: only the next few are near enough to see */
        UINT8 kq = n0 + q, y, rk2, r3; INT16 lat;
        if(kq >= NSCEN) kq -= NSCEN;
        dz = (INT16)(((UINT16)kq << 5) + ((UINT16)kq << 3)) - (INT16)pm; if(dz < 0) dz += (INT16)LAPM;       /* (kq * 40, without a multiply call) */
        if(dz < 2 || dz > 90) continue;
        r3 = scn_r3[kq]; rk2 = (r3 == 0);
        lat = (kq & 1) ? 150 + (((kq << 2) + kq) & 31) : -150 - (((kq << 2) + kq) & 31);
        y = kt_y[dz]; i = y - HZ;
        { INT16 x = 80 + (((lat - px) * kt_hw[i]) >> 7) + bend_at(i);
          if(dz < 20){ put_obj(n, rk2 ? KU_ROCK2 : KU_TREE2, 2, 2, rk2, x, y); n += 4; }
          else if(dz < 48){ put_obj(n, rk2 ? KU_ROCK1 : KU_TREE1, 2, 1, rk2, x, y); n += 2; }
          else { put_obj(n, rk2 ? KU_ROCK0 : KU_TREE0, 1, 1, rk2, x, y); n += 1; } }
      } }
    for(; n < 37; n++) shadow_OAM[n].y = 0;                                  /* (slots 37..39 are the jump pad's: hiding them too made the pad never show) */
}

static void me_draw(INT8 dir){
    if(stun[0]) dir = (INT8)((frame >> 2) % 3) - 1;                          /* spinning out */
    if(air_trick) dir = (INT8)((frame >> 1) % 3) - 1;                        /* a trick: spin in the air */
    put_obj(0, KU_ME0 + 3 * (UINT8)(dir + 1), 3, 1, 5, 80, 141 - air_h() - ((frame >> 2) & (vel[0] > 0 ? 1 : 0)) - (boost_t ? 2 : 0));
}

static void flip(void){
    memcpy(road_scx, tab, 144);
}

/* one race. mode: 0 a race, 1 a time trial (alone). gpn: 1..3 inside a grand prix. Returns your place (1..4), or 0 when you quit (START). */
UINT8 kart_race(UINT8 t, UINT8 tt, UINT8 gpn) BANKED {
    UINT8 k, done = 0, state = 0, place = 1;
    UINT16 sc[1 + NRIV];
    INT8 dir = 0; UINT16 tcount = 0; UINT32 lastf = 0;
    trk = t; rivals_on = !tt; rev = kart_rev & 1; nlaps = gpn ? 3 : kart_laps; if(nlaps != 1 && nlaps != 5) nlaps = 3;
    if(rev){ for(k = 0; k < 16; k++) rsg[k] = -segs[(t << 4) + ((16 - k) & 15)]; sg = rsg; } else sg = &segs[t << 4];
    bst = nlaps == 3 ? kart_rec(2, t, rev) : 0;
    { UINT16 bl = kart_rec(0, t, rev); gh_on = (UINT8)(tt && bl >= 1500 && bl < 6000); ghv = gh_on ? (UINT16)(524288UL / bl) : 0; shield_t = 0; }
    old_song = snd_song; { static const UINT8 kt_tune[6] = { MUS_KART, MUS_KART3, MUS_KART2, MUS_KART3, MUS_KART4, MUS_KART2 }; music(kt_tune[t % 6]); }
    DISPLAY_OFF; HIDE_WIN;
    track_pal(t);
    for(k = 0; k <= NRIV; k++){ tm[k] = (UINT16)((gh_on && k == NRIV ? 0 : k * 5) + KX_T0); tf[k] = 0; vel[k] = 0; lx[k] = k ? lane[k - 1] : 0; stun[k] = 0; aicd[k] = 120 + ((UINT16)krnd() << 1); }   /* the start grid: you at the back */
    head = 0; stripe = 0; stripe_acc = 0; tot_t = lap_t = best_run = 0; tmin = ts10 = ts1 = tten = tc6 = 0; hkey = 255; lapn = KX_L0; nextlap = LAPM * (KX_L0 + 1); bump_cd = 0; frame = 0;
    kart_air_reset(); hold = 0; boost_t = 0; drift_t = 0; got = 0; shl.life = 0; for(k = 0; k < 3; k++) oil[k].life = 0;
    rs ^= (UINT16)frames32() | 1;
#ifdef KX_ITEM
    hold = KX_ITEM;
#endif
#ifdef KX_OILSELF
    oil[0].m = tm[0] + 60; oil[0].lane = 0; oil[0].life = 900;
#endif
    memset(tab, 0, sizeof tab); flip(); set_stripes();
    txt_win(0);                                                                   /* the HUD rows (BG rows 0 and 1) and the banner box (window rows 0..2) are drawn once; only their text changes */
    VBK_REG = 0; fill_bkg_rect(0, 0, 32, 2, T_BLANK); VBK_REG = 1; fill_bkg_rect(0, 0, 32, 1, TXT_GOLD); fill_bkg_rect(0, 1, 32, 1, TXT_WHITE); VBK_REG = 0;
    hbp = TXT_WHITE; hbpd = 0;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 3); txt_color(2, 1, 16, 1, TXT_GOLD); txt_win(0); move_win(7, 144 - 24);
    memset(hb, 0xFF, sizeof hb); hbd = 0; bbd = 0; winreq = winnow = 0; bnt = 0;
    memset(hb[1], T_BLANK, 20); hb[1][0] = 'S'; hb[1][1] = 'P'; hb[1][2] = 'D'; hb[1][9] = ':'; hb[1][12] = '.'; hsp = 9999; hhold = 255; clk_d = 1;
    scene(); flip(); me_draw(0); hud(place); vflush();
    SHOW_BKG; SHOW_SPRITES; DISPLAY_ON;
#ifndef KX_NOISR
    road_start();
#endif
    banner(gh_on ? "BEAT THE GHOST!" : tt ? "TIME TRIAL!" : "GET READY!");
    while(!done){
        wait_vbl_done(); vflush();
        input_poll(); frame++;
        flip(); set_stripes();
        if(bnt && !--bnt) winreq = 0;
        if(HIT(J_START)){ place = 0; break; }
        if(state == 0){                                                        /* 3, 2, 1, GO */
            tcount++;
            if(tcount == 1 || tcount == 60 || tcount == 120){ char c[16]; c[0] = (char)('3' - tcount / 60); c[1] = '.'; c[2] = '.'; c[3] = '.'; c[4] = 0; banner(c); bnt = 0; sfx(SFX_COUNT); }
            if(tcount == 180){ banner(rev ? "REV GO" : "GO!"); bnt = 50; sfx(SFX_GO); state = 1; tcount = 0; }
            drive(0); lastf = frames32();
        } else if(state == 1){
            { UINT32 nf = frames32(); UINT8 ex = (UINT8)(nf - lastf); lastf = nf; ex = ex >= 3 ? 2 : 0;       /* a loop that took three frames or more (a hiccup: a save, a long item) runs the car for the frames it missed: the speed and the clock stay true */
#ifndef KX_NOCATCH
              while(ex--){ tot_t++; lap_t++; clk_tick(); drive(1); }
#endif
            }
            tcount++;
            tot_t++; lap_t++; clk_tick();
            drive(1);
#ifdef KX_JUMP
            if(tot_t == 40) air_t = 1;
#endif
            if(tm[0] >= nextlap && lapn < nlaps){ nextlap += LAPM;                  /* across the line */
                if(!best_run || lap_t < best_run) best_run = lap_t;
                { char c[17], *d = c; UINT16 lt = lap_t;
                  lap_t = 0; lapn++; got = 0;
                  if(lapn < nlaps){
                      if(lapn == nlaps - 1) d = scat(d, "FINAL     "); else { d = scat(d, "LAP "); d = u16s(d, lapn + 1); d = scat(d, "     "); }   /* (words left, time right: your car sits in the middle of the box) */
                      d = ftime(d, lt); *d = 0; banner(c); bnt = 110; sfx(SFX_COIN); } }
            }
            if(lapn >= nlaps){
                for(k = 1; k <= NRIV; k++) sc[k] = tm[k]; sc[0] = LAPM * nlaps;
                for(k = 0; k <= NRIV; k++) kart_rk[k] = rivals_on ? rank_of(k, sc) : 1;
                place = kart_rk[0]; state = 2; tcount = 0;
                hide_from(0); winreq = 0; vflush();
                kart_results(place, gpn); winnow = winreq = 1;
            }
        } else if(state == 2){
            drive(0);
            if(HIT(J_A) || HIT(J_B) || HIT(J_START)){ done = 1; }
#ifdef KX_AUTO
            if(++tcount > KX_WAIT) done = 1;
#endif
        }
        dir = HELD(J_LEFT) ? -1 : HELD(J_RIGHT) ? 1 : 0;
        if(state < 2){ scene(); me_draw(dir); hud(place_now()); }
#ifdef TEST_KFPS                                   /* how many frames (60ths of a second) 120 loops of the race take: 120 is a full 60 frames a second */
        { static UINT32 f0; static char pb[21]; if(frame == 400) f0 = frames32(); if(frame == 520){ char *pp = u16s(pb, (UINT16)(frames32() - f0)); *pp = 0; } if(frame >= 520) hud_row(0, pb); }
#endif
    }
    road_stop();
    hide_from(0); HIDE_WIN; SCX_REG = 0; SCY_REG = 0;
    music(old_song == MUS_NONE ? MUS_SPACE : old_song);
    return place;
}


void kart_setup(void) BANKED {                                                 /* the road, sky and sprites into video memory (the LCD is off) */
    kart_gfx();
    set_bkg_palette(4, 3, kt_pal);
    hide_from(0);
    SCX_REG = 0; SCY_REG = 0;
    track_pal(0);
    memset(tab, 0, sizeof tab); flip();
}
void kart_pal0(void) BANKED { track_pal(0); set_bkg_palette(4, 3, kt_pal); hide_from(0); }
