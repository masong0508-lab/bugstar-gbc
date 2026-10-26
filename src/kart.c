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
#include "kart_art.h"          /* the road, sky, sprites: made by tools/make_kart.py */

/* KART WORLD: a 3D race. 3 laps, 3 rivals. Units: a lap is 2048 "metres"; positions keep 8 more bits (so 1 metre = 256). */
#define LAPM   2048U
#define VMAX   256
#define NLAPS  3

#define HZ     64                              /* first road scanline */
#ifndef KX_T0
#define KX_T0 0
#define KX_L0 0
#endif
#define C15(r,g,b) ((UINT16)(((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))
#define NSCEN  52                              /* trees and rocks along a lap, one every 40 metres */


/* three tracks: the bend of each 128 m of road (+ right), and the colours of the land */
static const INT8 segs[48] = {
     0,  0, 10, 28, 28, 10,   0, -14, -30, -30, -14,   0, 18, 18,   0,  -8,           /* MEADOW: easy */
     0, 12, 26, 26, 12,  0, -10, -22, -22,  -8,   0,  20,  29,  29,  12,   0,           /* DESERT: wide bends */
    10, 29, 29, -8, -29, -29,  0,  24,  24, -12, -26, -26,   6,  29,  12, -10 };      /* SNOW PEAK: twisty */
static const UINT16 tcol[3][6] = {                                                    /* grass, grass 2, sky, low sky, mountain, snow cap */
    { C15(70,175,70),  C15(45,145,55),  C15(70,130,240),  C15(150,200,255), C15(95,105,160),  C15(215,225,245) },
    { C15(225,190,110), C15(205,168,88), C15(240,150,70),  C15(255,210,140), C15(170,90,60),   C15(230,190,150) },
    { C15(235,242,250), C15(205,218,235), C15(120,150,200), C15(190,210,235), C15(110,120,150), C15(255,255,255) } };
static const UINT8 tdif[3] = { 0, 6, 12 };                                           /* the rivals get quicker */
static const INT8 wob[8] = { 0, 10, 18, 10, 0, -10, -18, -10 };
static const INT8 lane[NRIV] = { -70, 0, 70 };
static const UINT8 vrv[NRIV] = { 232, 224, 214 };
static const UINT16 BXM[3] = { 300, 900, 1500 };                                      /* item boxes: where in the lap, and which side */
static const INT8  BXL[3] = { -80, 80, 0 };

static const INT8 *sg;
static UINT16 kp[12];                           /* this track's palettes */
UINT8  rivals_on; static UINT8 trk;

static UINT16 tm[1 + NRIV];                     /* distance driven, whole metres; 0 is you */
static UINT8  tf[1 + NRIV];                     /* ...and the 1/256 of a metre that goes with it */
static INT16  vel[1 + NRIV], lx[1 + NRIV];     /* speed (1/256 m a frame) and the sideways spot (128 = the road's edge) */
static INT8   bend[80];
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
static UINT8  hold, boost_t, drift_t, got, stun[1 + NRIV];
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

static void set_stripes(void){
    set_bkg_palette(4, 1, &kp[stripe ? 4 : 0]);
    set_bkg_palette(5, 1, &kp[stripe ? 0 : 4]);
}
static void track_pal(UINT8 t){
    UINT8 i;
    for(i = 0; i < 12; i++) kp[i] = kt_pal[i];
    kp[0] = tcol[t][0]; kp[4] = tcol[t][1];
    kp[8] = tcol[t][2]; kp[9] = tcol[t][3]; kp[10] = tcol[t][4]; kp[11] = tcol[t][5];
    set_bkg_palette(6, 1, &kp[8]);
}

static void put_obj(UINT8 slot, UINT8 unit, UINT8 cols, UINT8 rows, UINT8 pal, INT16 cx, INT16 by){      /* (adds only, and straight into the sprite table: a multiply or a call costs more than a sprite) */
    UINT8 c, r, s = slot, u = unit;
    INT16 x = cx - (INT16)(cols << 2), y0 = by - (INT16)(rows << 4), y;
    for(c = 0; c < cols; c++, x += 8){
        y = y0;
        for(r = 0; r < rows; r++, y += 16, s++, u++){
            if(x < -7 || x > 159 || y < 16 || y > 143){ shadow_OAM[s].y = 0; continue; }
            shadow_OAM[s].tile = (UINT8)(128 + (u << 1));
            shadow_OAM[s].prop = pal;
            shadow_OAM[s].x = (UINT8)(x + 8); shadow_OAM[s].y = (UINT8)(y + 16);
        }
    }
}
static void hide_from(UINT8 s){ for(; s < 40; s++) shadow_OAM[s].y = 0; }
/* one flat object on the road (an item box, an oil slick, a shell): three sizes, three sprites at most, from slot o */
static void show3(UINT8 o, INT16 dz, INT16 lat, UINT8 u0, UINT8 u1, UINT8 u2, UINT8 pal, INT16 px){
    UINT8 y, i; INT16 x;
    if(dz < 2 || dz > 90){ shadow_OAM[o].y = 0; shadow_OAM[o + 1].y = 0; shadow_OAM[o + 2].y = 0; return; }
    y = kt_y[dz]; i = y - HZ;
    x = 80 + (((lat - px) * kt_hw[i]) >> 7) + bend[i];
    if(dz < 18) put_obj(o, u2, 3, 1, pal, x, y);
    else if(dz < 44){ put_obj(o, u1, 2, 1, pal, x, y); shadow_OAM[o + 2].y = 0; }
    else { put_obj(o, u0, 1, 1, pal, x, y); shadow_OAM[o + 1].y = 0; shadow_OAM[o + 2].y = 0; }
}

static void banner(const char *s){
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 3); txt_line(2, 1, s, 16); txt_color(2, 1, 16, 1, TXT_GOLD); txt_win(0);
    move_win(7, 144 - 24); SHOW_WIN;
}
static char *ftime(char *d, UINT16 t){                          /* m:ss.d from 60ths of a second */
    UINT16 s = t / 60;
    d = u16s(d, s / 60); d = scat(d, ":"); if(s % 60 < 10) d = scat(d, "0");
    d = u16s(d, s % 60); d = scat(d, "."); d = u16s(d, (t % 60) / 6); return d;
}
static const char *const itnm[4] = { "", "BOOST", "SHELL", "OIL" };

static void hud(UINT8 place){
    char b[40], *d;
    d = scat(b, "LAP "); d = u16s(d, lapn < NLAPS ? lapn + 1 : NLAPS);
    if(rivals_on){ d = scat(d, "/3 PLACE "); d = u16s(d, place); d = scat(d, "/4"); }
    else { d = scat(d, "/3 BEST "); if(ks.tt[trk]) d = ftime(d, ks.tt[trk]); else d = scat(d, "-:--.-"); }
    d = scat(d, "  ");
    if(boost_t) d = scat(d, "TURBO!"); else if(stun[0]) d = scat(d, "SPIN!"); else if(drift_t > 30) d = scat(d, "DRIFT");
    *d = 0;
    txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_GOLD);
    d = scat(b, "SPD "); d = u16s(d, (UINT16)vel[0] * 3 / 4); d = scat(d, " "); d = ftime(d, tot_t); d = scat(d, " "); d = scat(d, itnm[hold]); *d = 0;
    txt_line(0, 1, b, 20); txt_color(0, 1, 20, 1, hold ? TXT_RED : TXT_WHITE);
#ifdef KX_DBG
    d = u16s(b, tm[0]); d = scat(d, " "); d = u16s(d, lapn); d = scat(d, " "); d = u16s(d, nextlap); *d = 0; txt_line(0, 17, b, 20);
#endif
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
    if(boost_t) boost_t--;
    for(k = 0; k <= NRIV; k++) if(stun[k]) stun[k]--;
    if(!go) return;
    for(b = 0; b < 3; b++) if(!(got & (1 << b)) && !hold){                      /* the boxes */
        dz = (INT16)BXM[b] - (INT16)pm; dx = lx[0] - BXL[b];
        if(dz <= 3 && dz >= -12 && dx > -44 && dx < 44){ UINT8 r = krnd() & 7; got |= 1 << b; hold = r < 3 ? 1 : r < 6 ? 2 : 3; sfx(SFX_PICK); }
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
        if(HELD(J_A)) v += boost_t ? 12 : 4; else v -= 2;
        if(HELD(J_DOWN)) v -= 8;
        if(HELD(J_LEFT)) dir = -1;
        if(HELD(J_RIGHT)) dir = 1;
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
static void scene(void){
    UINT8 i, k, n = 21; INT16 cur = curve_at(tm[0] + 24, tf[0]), px = lx[0];
    UINT16 pm = tm[0] & (LAPM - 1);
    INT16 sh = px * 6, sh_d = (INT16)(((INT32)px * 466L) >> 7);              /* the sideways shift of the road (8.8): px * hw / 128, two lines at a time */
    { const INT8 *row; UINT8 *tp = &tab[HZ]; INT8 *bp = bend; UINT8 kk;
      if(cur < -30) cur = -30; if(cur > 29) cur = 29;
      row = &kt_bv[(UINT8)(cur + 30) * 40U];
      for(kk = 0; kk < 40; kk++){
        INT8 bv = row[kk];
        UINT8 t = (UINT8)((UINT8)((UINT16)sh >> 8) - (UINT8)bv);
        *bp++ = bv; *bp++ = bv; *tp++ = t; *tp++ = t;
        sh += sh_d;
      } }
    memset(tab, 0, 16); memset(&tab[16], sky_x = (UINT8)(head >> 4), HZ - 16);
    for(k = 1; k <= NRIV; k++){                                             /* the rivals ahead of you, bigger as they near */
        INT16 dz = (INT16)(tm[k] - tm[0]);
        UINT8 y, o = 3 * (k - 1) + 3;
        if(!rivals_on || dz < 1 || dz > 90){ for(i = 0; i < 3; i++) shadow_OAM[o + i].y = 0; continue; }
        y = kt_y[dz]; i = y - HZ;
        { INT16 x = 80 + (((lx[k] - px) * kt_hw[i]) >> 7) + bend[i];
          if(stun[k]) x += (frame & 2) ? 5 : -5;
          if(dz < 18) put_obj(o, KU_AI2, 3, 1, 1 + k, x, y);
          else if(dz < 44){ put_obj(o, KU_AI1, 2, 1, 1 + k, x, y); shadow_OAM[o + 2].y = 0; }
          else { put_obj(o, KU_AI0, 1, 1, 1 + k, x, y); shadow_OAM[o + 1].y = 0; shadow_OAM[o + 2].y = 0; } }
    }
    { UINT8 b, nb = 0; INT16 dz, bd = 999;                                    /* the nearest item box ahead */
      for(b = 0; b < 3; b++) if(!(got & (1 << b))){ dz = (INT16)BXM[b] - (INT16)pm; if(dz < 0) dz += (INT16)LAPM; if(dz < bd){ bd = dz; nb = b; } }
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
        dz = (INT16)(kq * 40U) - (INT16)pm; if(dz < 0) dz += (INT16)LAPM;
        if(dz < 2 || dz > 90) continue;
        r3 = kq % 3; rk2 = (r3 == 0);
        lat = (kq & 1) ? 150 + ((kq * 5) & 31) : -150 - ((kq * 5) & 31);
        y = kt_y[dz]; i = y - HZ;
        { INT16 x = 80 + (((lat - px) * kt_hw[i]) >> 7) + bend[i];
          if(dz < 20){ put_obj(n, rk2 ? KU_ROCK2 : KU_TREE2, 2, 2, rk2, x, y); n += 4; }
          else if(dz < 48){ put_obj(n, rk2 ? KU_ROCK1 : KU_TREE1, 2, 1, rk2, x, y); n += 2; }
          else { put_obj(n, rk2 ? KU_ROCK0 : KU_TREE0, 1, 1, rk2, x, y); n += 1; } }
      } }
    hide_from(n);
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
    UINT8 k, done = 0, state = 0, place = 1, saved = 0;
    UINT16 sc[1 + NRIV];
    INT8 dir = 0; UINT16 tcount = 0;
    trk = t; rivals_on = !tt; sg = &segs[t << 4];
    DISPLAY_OFF; HIDE_WIN;
    track_pal(t);
    for(k = 0; k <= NRIV; k++){ tm[k] = (UINT16)(k * 5 + KX_T0); tf[k] = 0; vel[k] = 0; lx[k] = k ? lane[k - 1] : 0; stun[k] = 0; aicd[k] = 120 + ((UINT16)krnd() << 1); }   /* the start grid: you at the back */
    head = 0; stripe = 0; stripe_acc = 0; tot_t = lap_t = best_run = 0; lapn = KX_L0; nextlap = LAPM * (KX_L0 + 1); bump_cd = 0; frame = 0;
    kart_air_reset(); hold = 0; boost_t = 0; drift_t = 0; got = 0; shl.life = 0; for(k = 0; k < 3; k++) oil[k].life = 0;
    rs ^= (UINT16)frames32() | 1;
#ifdef KX_ITEM
    hold = KX_ITEM;
#endif
#ifdef KX_OILSELF
    oil[0].m = tm[0] + 60; oil[0].lane = 0; oil[0].life = 900;
#endif
    memset(tab, 0, sizeof tab); flip(); set_stripes();
    scene(); flip(); me_draw(0);
    SHOW_BKG; SHOW_SPRITES; DISPLAY_ON;
#ifndef KX_NOISR
    road_start();
#endif
    banner(tt ? "TIME TRIAL!" : "GET READY!");
    while(!done){
        wait_vbl_done();
        input_poll(); frame++;
        flip(); set_stripes();
        if(HIT(J_START)){ place = 0; break; }
        if(state == 0){                                                        /* 3, 2, 1, GO */
            tcount++;
            if(tcount == 1 || tcount == 60 || tcount == 120){ char c[16]; c[0] = (char)('3' - tcount / 60); c[1] = '.'; c[2] = '.'; c[3] = '.'; c[4] = 0; banner(c); sfx(SFX_MOVE); }
            if(tcount == 180){ banner("GO GO GO!"); sfx(SFX_OK); state = 1; tcount = 0; }
            drive(0);
        } else if(state == 1){
            tcount++; if(tcount == 45) HIDE_WIN;
            tot_t++; lap_t++;
            drive(1);
#ifdef KX_JUMP
            if(tot_t == 40) air_t = 1;
#endif
            if(tm[0] >= nextlap && lapn < NLAPS){ nextlap += LAPM;                  /* across the line */
                if(!best_run || lap_t < best_run) best_run = lap_t;
                lap_t = 0; lapn++; got = 0;
                if(lapn < NLAPS){ char c[16]; c[0] = 'L'; c[1] = 'A'; c[2] = 'P'; c[3] = ' '; c[4] = (char)('1' + lapn); c[5] = '/'; c[6] = '3'; c[7] = 0; banner(c); tcount = 0; sfx(SFX_COIN); }
            }
            if(lapn >= NLAPS){
                for(k = 1; k <= NRIV; k++) sc[k] = tm[k]; sc[0] = LAPM * NLAPS;
                for(k = 0; k <= NRIV; k++) kart_rk[k] = rivals_on ? rank_of(k, sc) : 1;
                place = kart_rk[0]; state = 2; tcount = 0;
                if(best_run && (!EX_KART || best_run < EX_KART)){ EX_KART = best_run; saved = 1; }
                if(tt && (!ks.tt[trk] || tot_t < ks.tt[trk])){ ks.tt[trk] = tot_t; saved = 1; kart_ks_save(); }
                if(opt.autosave) slot_write(cur_slot);
                hide_from(0); kart_results(place, saved, gpn);
            }
        } else if(state == 2){
            drive(0);
            if(HIT(J_A) || HIT(J_B) || HIT(J_START)){ done = 1; }
#ifdef KX_AUTO
            if(++tcount > 40) done = 1;
#endif
        }
        dir = HELD(J_LEFT) ? -1 : HELD(J_RIGHT) ? 1 : 0;
#ifdef TEST_KFPS
        if(state < 2){ scene(); me_draw(dir); if(!(frame & 7)) hud(place_now()); }
        { static UINT32 f0; if(frame == 400) f0 = frames32(); if(frame == 520){ char pb[21], *pp; pp = u16s(pb, (UINT16)(frames32() - f0)); *pp = 0; HIDE_WIN; txt_line(0, 17, pb, 20); } }
#else
        if(state < 2){ scene(); me_draw(dir); if(!(frame & 7)) hud(place_now()); }
#endif
    }
    road_stop();
    hide_from(0); HIDE_WIN; SCX_REG = 0; SCY_REG = 0;
    return place;
}


void kart_setup(void) BANKED {                                                 /* the road, sky and sprites into video memory (the LCD is off) */
    VBK_REG = 0; txt_cls();
    VBK_REG = 1; set_bkg_data(0, KT_N, kt_bg);
    set_bkg_tiles(0, 2, 32, 16, kt_attr);
    VBK_REG = 0; set_bkg_tiles(0, 2, 32, 16, kt_map);
    set_bkg_palette(4, 3, kt_pal);
    SPRITES_8x16;
    set_sprite_data(128, KU_N * 2, kt_spr);
    set_sprite_palette(0, 8, kt_spal);
    set_sprite_palette_entry(5, 2, sv.look.col[BC_SHELL]);
    hide_from(0);
    SCX_REG = 0; SCY_REG = 0;
    track_pal(0);
    memset(tab, 0, sizeof tab); flip();
}
void kart_pal0(void) BANKED { track_pal(0); set_bkg_palette(4, 3, kt_pal); hide_from(0); }
