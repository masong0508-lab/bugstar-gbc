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
#define LAPM   2048UL
#define LAPF   (LAPM << 8)
#define VMAX   256
#define NLAPS  3
#define NRIV   3
#define HZ     64                              /* first road scanline */
#define NSCEN  52                              /* trees and rocks along a lap, one every 40 metres */

static const INT8 seg[16] = { 0, 0, 10, 28, 28, 10, 0, -14, -30, -30, -14, 0, 18, 18, 0, -8 };      /* the bend of each 128 m of track (+ right) */
static const INT8 wob[8] = { 0, 10, 18, 10, 0, -10, -18, -10 };
static const INT8 lane[NRIV] = { -70, 0, 70 };
static const UINT8 vrv[NRIV] = { 232, 224, 214 };

static UINT32 tot[1 + NRIV];                   /* distance driven (1/256 m); 0 is you */
static INT16  vel[1 + NRIV], lx[1 + NRIV];     /* speed (1/256 m a frame) and the sideways spot (128 = the road's edge) */
static INT8   bend[80];
static UINT8  tab[144];
static UINT8  stripe, sky_x;
static UINT16 stripe_acc, tot_t, lap_t, best_run, frame;
static INT16  head;
static UINT8  lapn, bump_cd;

static INT16 curve_at(UINT32 p){
    UINT8 i = (UINT8)(p >> 15) & 15, f = (UINT8)(p >> 7);
    INT8 a = seg[i], b = seg[(i + 1) & 15];
    return a + (INT16)((((INT16)b - a) * (INT16)f) >> 8);
}

static void set_stripes(void){
    set_bkg_palette(4, 1, &kt_pal[stripe ? 4 : 0]);
    set_bkg_palette(5, 1, &kt_pal[stripe ? 0 : 4]);
}

static void put_obj(UINT8 slot, UINT8 unit, UINT8 cols, UINT8 rows, UINT8 pal, INT16 cx, INT16 by){
    UINT8 c, r;
    for(c = 0; c < cols; c++) for(r = 0; r < rows; r++){
        INT16 x = cx - 4 * cols + 8 * c, y = by - 16 * rows + 16 * r;
        UINT8 s = slot + c * rows + r;
        if(x < -7 || x > 159 || y < 16 || y > 143){ move_sprite(s, 0, 0); continue; }
        set_sprite_tile(s, 128 + 2 * (unit + c * rows + r));
        set_sprite_prop(s, pal);
        move_sprite(s, (UINT8)(x + 8), (UINT8)(y + 16));
    }
}
static void hide_from(UINT8 s){ for(; s < 40; s++) move_sprite(s, 0, 0); }

static void banner(const char *s){
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 3); txt_line(2, 1, s, 16); txt_color(2, 1, 16, 1, TXT_GOLD); txt_win(0);
    move_win(7, 144 - 24); SHOW_WIN;
}

static void hud(UINT8 place){
    char b[21], *d; UINT16 s = tot_t / 60;
    d = scat(b, "LAP "); d = u16s(d, lapn < NLAPS ? lapn + 1 : NLAPS); d = scat(d, "/3   PLACE "); d = u16s(d, place); d = scat(d, "/4"); *d = 0;
    txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_GOLD);
    d = scat(b, "SPEED "); d = u16s(d, (UINT16)vel[0] * 3 / 4); d = scat(d, "  "); d = u16s(d, s / 60); d = scat(d, ":");
    if(s % 60 < 10) d = scat(d, "0");
    d = u16s(d, s % 60); d = scat(d, "."); d = u16s(d, (tot_t % 60) / 6); *d = 0;
    txt_line(0, 1, b, 20); txt_color(0, 1, 20, 1, TXT_WHITE);
}

static UINT8 place_now(void){
    UINT8 k, p = 1;
    for(k = 1; k <= NRIV; k++) if(tot[k] > tot[0]) p++;
    return p;
}

/* one frame of the race: you, then the three rivals */
static void drive(UINT8 go){
    INT16 v = vel[0], cur = curve_at(tot[0]), cap = (lx[0] > 150 || lx[0] < -150) ? 70 : VMAX;
    INT8 dir = 0; UINT8 k;
    if(go){
        if(HELD(J_A)) v += 4; else v -= 2;
        if(HELD(J_B) || HELD(J_DOWN)) v -= 8;
        if(HELD(J_LEFT)) dir = -1;
        if(HELD(J_RIGHT)) dir = 1;
    } else v = 0;
    if(v > cap){ v -= (v - cap > 8) ? 8 : (v - cap); }
    if(v < 0) v = 0;
    lx[0] += dir * ((v >> 6) + (v > 0 ? 1 : 0));
    lx[0] -= (cur * v) >> 12;
    if(lx[0] > 210) lx[0] = 210; if(lx[0] < -210) lx[0] = -210;
    vel[0] = v; tot[0] += (UINT16)v;
    head += (cur * v) >> 10;
    stripe_acc += (UINT16)v;
    if(stripe_acc >= 1536){ stripe_acc -= 1536; stripe ^= 1; }
    for(k = 1; k <= NRIV; k++){
        INT16 gap = (INT16)((INT32)(tot[k] - tot[0]) >> 8), want = vrv[k - 1] - (curve_at(tot[k]) < 0 ? -curve_at(tot[k]) : curve_at(tot[k])) / 2;
        INT16 tl = lane[k - 1] + wob[(UINT8)(tot[k] >> 14) & 7];
        if(gap > 120) want -= 24; else if(gap < -120) want += 20;
        if(!go) want = 0;
        if(vel[k] < want) vel[k] += 3; else if(vel[k] > want) vel[k] -= 3;
        tot[k] += (UINT16)vel[k];
        if(lx[k] < tl) lx[k] += 2; else if(lx[k] > tl) lx[k] -= 2;
        { INT16 dx = lx[k] - lx[0];                                         /* a bump */
          if(gap > -7 && gap < 7 && dx > -26 && dx < 26){
              if(gap >= 0){ if(vel[0] > vel[k] - 30) vel[0] = vel[k] > 30 ? vel[k] - 30 : 0; } else if(vel[k] > 40) vel[k] -= 40;
              lx[0] += dx > 0 ? -3 : 3;
              if(!bump_cd){ bump_cd = 20; sfx(SFX_HURT); } } }
    }
    if(bump_cd) bump_cd--;
}

/* the picture: the sideways slide of every road line, then the sprites */
static void scene(void){
    UINT8 i, k, n = 12; INT16 cur = curve_at(tot[0] + 24UL * 256UL), px = lx[0];
    UINT32 pm = tot[0] >> 8;
    for(i = 0; i < 80; i++){
        bend[i] = (INT8)((cur * kt_qa[i]) >> 6);
        tab[HZ + i] = (UINT8)(((px * kt_hw[i]) >> 7) - bend[i]);
    }
    memset(tab, 0, 16); memset(&tab[16], sky_x = (UINT8)(head >> 4), HZ - 16);
    for(k = 1; k <= NRIV; k++){                                             /* the rivals ahead of you, bigger as they near */
        INT16 dz = (INT16)((INT32)(tot[k] - tot[0]) >> 8);
        UINT8 y, o = 3 * (k - 1) + 3;
        if(dz < 1 || dz > 90){ for(i = 0; i < 3; i++) move_sprite(o + i, 0, 0); continue; }
        y = kt_y[dz]; i = y - HZ;
        { INT16 x = 80 + (((lx[k] - px) * kt_hw[i]) >> 7) + bend[i];
          if(dz < 18) put_obj(o, KU_AI2, 3, 1, 1 + k, x, y);
          else if(dz < 44){ put_obj(o, KU_AI1, 2, 1, 1 + k, x, y); move_sprite(o + 2, 0, 0); }
          else { put_obj(o, KU_AI0, 1, 1, 1 + k, x, y); move_sprite(o + 1, 0, 0); move_sprite(o + 2, 0, 0); } }
    }
    for(k = 0; k < NSCEN && n < 38; k++){                                   /* trees and rocks along the road */
        INT16 dz = (INT16)(((UINT32)k * 40UL + LAPM - (pm & (LAPM - 1))) & (LAPM - 1));
        UINT8 y, rk = (k % 3) == 0;
        INT16 lat = (k & 1) ? 150 + ((k * 37) & 31) : -150 - ((k * 37) & 31);
        if(dz < 2 || dz > 90) continue;
        y = kt_y[dz]; i = y - HZ;
        { INT16 x = 80 + (((lat - px) * kt_hw[i]) >> 7) + bend[i];
          if(dz < 20){ put_obj(n, rk ? KU_ROCK2 : KU_TREE2, 2, 2, rk, x, y); n += 4; }
          else if(dz < 48){ put_obj(n, rk ? KU_ROCK1 : KU_TREE1, 2, 1, rk, x, y); n += 2; }
          else { put_obj(n, rk ? KU_ROCK0 : KU_TREE0, 1, 1, rk, x, y); n += 1; } }
    }
    hide_from(n);
}

static void me_draw(INT8 dir){
    put_obj(0, KU_ME0 + 3 * (UINT8)(dir + 1), 3, 1, 5, 80, 141 - ((frame >> 2) & (vel[0] > 0 ? 1 : 0)));
}

static void flip(void){
    memcpy(road_scx, tab, 144);
}

static void results(UINT8 place, UINT8 saved){
    static const UINT16 prize[4] = { 200, 100, 50, 20 };
    char b[21], *d; UINT16 s = tot_t / 60;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 14);
    txt_line(5, 1, "RACE OVER!", 10); txt_color(5, 1, 10, 1, TXT_GOLD);
    d = scat(b, "YOU CAME "); d = u16s(d, place); d = scat(d, place == 1 ? " (WIN!)" : " OF 4"); *d = 0; txt_line(3, 3, b, 15);
    d = scat(b, "TIME "); d = u16s(d, s / 60); d = scat(d, ":"); if(s % 60 < 10) d = scat(d, "0"); d = u16s(d, s % 60); *d = 0; txt_line(3, 5, b, 15);
    d = scat(b, "BEST LAP "); d = u16s(d, best_run / 60); d = scat(d, "."); d = u16s(d, (best_run % 60) * 10 / 60); *d = 0; txt_line(3, 7, b, 15);
    if(saved) txt_line(3, 8, "NEW RECORD!", 15), txt_color(3, 8, 15, 1, TXT_GOLD);
    d = scat(b, "PRIZE +"); d = u16s(d, prize[place - 1]); d = scat(d, " G"); *d = 0; txt_line(3, 10, b, 15); txt_color(3, 10, 15, 1, TXT_GOLD);
    txt_line(3, 12, "A: OK", 15); txt_color(3, 12, 15, 1, TXT_GREY);
    txt_win(0); move_win(7, 8); SHOW_WIN;
    sv.gold += prize[place - 1];
    sfx(place == 1 ? SFX_FANFARE : SFX_COIN);
}

void kart_run(void) BANKED {
    UINT8 k, done = 0, state = 0, place = 1, was = LCDC_REG & LCDCF_OBJ16, saved = 0;
    INT8 dir = 0; UINT16 t = 0;
    DISPLAY_OFF; HIDE_WIN;
    VBK_REG = 0; txt_cls();
    VBK_REG = 1; set_bkg_data(0, KT_N, kt_bg);
    set_bkg_tiles(0, 2, 32, 16, kt_attr);
    VBK_REG = 0; set_bkg_tiles(0, 2, 32, 16, kt_map);
    set_bkg_palette(4, 3, kt_pal);
    SPRITES_8x16;
    set_sprite_data(128, KU_N * 2, kt_spr);
    set_sprite_palette(0, 6, kt_spal);
    set_sprite_palette_entry(5, 2, sv.look.col[BC_SHELL]);
    hide_from(0);
    SCX_REG = 0; SCY_REG = 0;
    for(k = 0; k <= NRIV; k++){ tot[k] = (UINT32)(4 - k) * 5UL * 256UL; vel[k] = 0; lx[k] = k ? lane[k - 1] : 0; }   /* the start grid: you at the back */
    head = 0; stripe = 0; stripe_acc = 0; tot_t = lap_t = best_run = 0; lapn = 0; bump_cd = 0; frame = 0;
    memset(tab, 0, sizeof tab); flip(); set_stripes();
    scene(); flip(); me_draw(0);
    SHOW_BKG; SHOW_SPRITES; DISPLAY_ON;
    road_start();
    banner("GET READY!");
    while(!done){
        wait_vbl_done(); input_poll(); frame++;
        flip(); set_stripes();
        if(HIT(J_START)){ done = 1; state = 9; break; }
        if(state == 0){                                                        /* 3, 2, 1, GO */
            t++;
            if(t == 1 || t == 60 || t == 120){ char c[16]; c[0] = (char)('3' - t / 60); c[1] = '.'; c[2] = '.'; c[3] = '.'; c[4] = 0; banner(c); sfx(SFX_MOVE); }
            if(t == 180){ banner("GO GO GO!"); sfx(SFX_OK); state = 1; t = 0; }
            drive(0);
        } else if(state == 1){
            t++; if(t == 45) HIDE_WIN;
            tot_t++; lap_t++;
            drive(1);
            if((UINT8)(tot[0] / LAPF) > lapn && lapn < NLAPS){                  /* across the line */
                if(!best_run || lap_t < best_run) best_run = lap_t;
                lap_t = 0; lapn++;
                if(lapn < NLAPS){ char c[16]; c[0] = 'L'; c[1] = 'A'; c[2] = 'P'; c[3] = ' '; c[4] = (char)('1' + lapn); c[5] = '/'; c[6] = '3'; c[7] = 0; banner(c); t = 0; sfx(SFX_COIN); }
            }
            if(lapn >= NLAPS){
                UINT8 r = 1; for(k = 1; k <= NRIV; k++) if(tot[k] >= LAPF * NLAPS) r++;
                place = r; state = 2; t = 0;
                if(best_run && (!EX_KART || best_run < EX_KART)){ EX_KART = best_run; saved = 1; }
                if(opt.autosave) slot_write(cur_slot);
                results(place, saved);
            }
        } else if(state == 2){
            drive(0);
            if(HIT(J_A) || HIT(J_B) || HIT(J_START)){ done = 1; }
        }
        dir = HELD(J_LEFT) ? -1 : HELD(J_RIGHT) ? 1 : 0;
        scene(); me_draw(dir);
        if(state < 2 && !(frame & 7)) hud(place_now());
    }
    road_stop();
    hide_from(0); HIDE_SPRITES; HIDE_WIN; if(!was) SPRITES_8x8;
    txt_win(0); VBK_REG = 0; txt_cls();
    SCX_REG = 0; SCY_REG = 0;
}
