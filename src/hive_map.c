#pragma bank 46
/* BUGSTAR GBC - BUG HIVE, the map (hive_int.h). The colony map is the 32x32 background map itself: the screen scrolls over it (SCX/SCY), nothing is redrawn
   when the view moves. Workers, the cursor and the weather are sprites. A three-row window at the bottom is the HUD. Time runs here: a tick every 6 seconds. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hive_int.h"
#include "hive_art.h"

#define VIEW_W 20
#define VIEW_H 15
#define HUD_WY 120
#define TICKF  360
static UINT8 cux, cuy, cam_x, cam_y;                  /* cursor cell, view corner (cells) */
static UINT8 wdx[8], wdy[8], wspd;
static const UINT8 ctile[C_N] = { TB_SOIL, TB_TUNNEL, TB_ROCK, TB_ORE, TB_CRYS, TB_SKY, TB_GRASS, TB_FLOWER, TB_ENTRY, TB_WATER, TB_SITE, TB_QUEEN,
                                  TB_NURS, TB_FARM, TB_STORE, TB_BARR, TB_WORK, TB_SMELT, TB_WALL, TB_GATE, TB_TOWER, TB_SHRINE, TB_MARKET };
static const UINT8 ekind[12][3] = {                    /* the colour of the ground on each kind of world (5-bit rgb of the middle shade) */
    { 14, 9, 5 }, { 20, 15, 8 }, { 14, 17, 22 }, { 8, 10, 5 }, { 15, 15, 11 }, { 11, 7, 14 },
    { 20, 8, 5 }, { 15, 8, 16 }, { 8, 12, 20 }, { 13, 5, 4 }, { 10, 12, 6 }, { 22, 12, 16 } };
static const UINT8 ssky[4][3] = { { 14, 24, 31 }, { 12, 22, 31 }, { 20, 20, 26 }, { 22, 26, 30 } };
static const UINT8 sgr[4][3] = { { 8, 26, 8 }, { 12, 24, 5 }, { 26, 16, 4 }, { 26, 28, 30 } };

static palette_color_t mix(UINT8 r, UINT8 g, UINT8 b, UINT8 dk){
    UINT8 m = 8 - dk;
    r = r * m / 8; g = g * m / 8; b = b * m / 8 + dk;
    if(r > 31) r = 31; if(g > 31) g = 31; if(b > 31) b = 31;
    return RGB(r, g, b);
}
static UINT8 darkness(void){
    UINT8 t = HV->tod;
    if(t >= 14 && t < 18) return 4;
    if(t == 12 || t == 13 || t == 18 || t == 19) return 2;
    return 0;
}
void hm_pals(void) BANKED {
    palette_color_t p[4]; UINT8 k = HV->kind, s = (UINT8)((HV->day / 6) & 3), dk = darkness(), mr = ekind[k][0], mg = ekind[k][1], mb = ekind[k][2];
    p[0] = mix(mr / 3, mg / 3, mb / 3, dk); p[1] = mix(mr, mg, mb, dk); p[2] = mix(mr + mr / 2, mg + mg / 2, mb + mb / 2, dk); p[3] = mix(mr * 2, mg * 2, mb * 2, dk);
    set_bkg_palette(4, 1, p);
    p[2] = mix(21, 22, 24, dk); p[3] = mix(31, 31, 31, dk); set_bkg_palette(2, 1, p);                /* iron: mid shades from above */
    p[2] = mix(8, 25, 30, dk); p[3] = mix(26, 31, 31, dk); set_bkg_palette(3, 1, p);                 /* crystal and water */
    p[0] = mix(ssky[s][0], ssky[s][1], ssky[s][2], dk); p[1] = mix(sgr[s][0], sgr[s][1], sgr[s][2], dk);
    p[2] = mix(sgr[s][0] / 2, sgr[s][1] / 2, sgr[s][2] / 2, dk); p[3] = mix(31, 29, 8, dk); set_bkg_palette(5, 1, p);
    p[0] = mix(3, 2, 2, dk); p[1] = mix(18, 12, 6, dk); p[2] = mix(21, 21, 22, dk); p[3] = mix(31, 29, 22, dk); set_bkg_palette(6, 1, p);
    p[1] = mix(25, 25, 28, dk); p[2] = mix(31, 25, 6, dk); p[3] = mix(31, 31, 31, dk); set_bkg_palette(7, 1, p);
    {   /* sprites: idle, forager, builder, nurse, guard, miner, enemy, white */
        static const UINT8 sc[8][3] = { { 28, 26, 6 }, { 8, 22, 6 }, { 30, 16, 4 }, { 30, 18, 24 }, { 28, 5, 5 }, { 13, 17, 26 }, { 20, 6, 22 }, { 31, 31, 31 } };
        UINT8 j;
        for(j = 0; j < 8; j++){
            p[0] = RGB(0, 0, 0); p[1] = RGB(3, 2, 2); p[2] = RGB(sc[j][0], sc[j][1], sc[j][2]); p[3] = RGB(31, 31, 26);
            set_sprite_palette(j, 1, p);
        }
    }
}
/* ---------------------------------------------------------------- cells */
static UINT8 cell_pal(UINT8 c){
    UINT8 t = CT(c);
    if(t <= C_ROCK || t == C_TUNNEL) return 4;
    if(t == C_ORE) return 2;
    if(t == C_CRYS || t == C_WATER) return 3;
    if(t <= C_ENTRY) return 5;
    if(t == C_QUEEN) return 7;
    if(t == C_SITE) return 6;
    return CLV(c) >= 3 ? 7 : 6;
}
static UINT8 cell_tile(UINT8 x, UINT8 y, UINT8 c){
    UINT8 t = CT(c), n = ctile[t];
    if((c & MARKD) && t <= C_CRYS) return t == C_SOIL ? TB_SOILM : t == C_ORE ? TB_OREM : TB_CRYM;
    if(t == C_SOIL && !((x * 7 + y * 13) & 3)) return TB_SOIL2;
    if(t == C_TUNNEL && !((x + y * 3) & 7)) return TB_TUNNEL2;
    return n;
}
void hm_cell(UINT8 x, UINT8 y) BANKED {
    UINT8 c = MAP[CI(x, y)], a = 0x08 | cell_pal(c), t = cell_tile(x, y, c);
    VBK_REG = VBK_BANK_1; set_bkg_tile_xy(x, y, a);
    VBK_REG = VBK_BANK_0; set_bkg_tile_xy(x, y, t);
}
void hm_fullmap(void) BANKED {
    UINT8 x, y, t[MW], a[MW], c;
    for(y = 0; y < MH; y++){
        for(x = 0; x < MW; x++){ c = MAP[CI(x, y)]; t[x] = cell_tile(x, y, c); a[x] = 0x08 | cell_pal(c); }
        VBK_REG = VBK_BANK_1; set_bkg_tiles(0, y, MW, 1, a);
        VBK_REG = VBK_BANK_0; set_bkg_tiles(0, y, MW, 1, t);
    }
}
static void flush_dirty(void){
    UINT8 n = HV->stat[S_DQN], i;
    if(HV->stat[S_FULL]){ HV->stat[S_FULL] = 0; HV->stat[S_DQN] = 0; hm_fullmap(); return; }
    for(i = 0; i < n; i++){ UINT16 q = DQ[i]; hm_cell((UINT8)(q & 31), (UINT8)(q >> 5)); }
    HV->stat[S_DQN] = 0;
}
/* ---------------------------------------------------------------- view and sprites */
void hm_look(UINT8 x, UINT8 y) BANKED {
    if(x < cam_x + 2) cam_x = x > 2 ? x - 2 : 0;
    if(x > cam_x + VIEW_W - 3) cam_x = x + 3 - VIEW_W;
    if(y < cam_y + 2) cam_y = y > 2 ? y - 2 : 0;
    if(y > cam_y + VIEW_H - 3) cam_y = y + 3 - VIEW_H;
    if(cam_x > MW - VIEW_W) cam_x = MW - VIEW_W;
    if(cam_y > MH - VIEW_H) cam_y = MH - VIEW_H;
    SCX_REG = (UINT8)(cam_x << 3); SCY_REG = (UINT8)(cam_y << 3);
}
UINT8 hm_cx(void) BANKED { return cux; }
UINT8 hm_cy(void) BANKED { return cuy; }
void hm_cursor_to(UINT8 x, UINT8 y) BANKED { cux = x; cuy = y; hm_look(x, y); }
void hm_sprites_off(void) BANKED { UINT8 i; for(i = 0; i < 40; i++) move_sprite(i, 0, 0); }
void hm_put_sprite(UINT8 oam, INT16 px, INT16 py, UINT8 tile, UINT8 pal, UINT8 flip) BANKED {
    INT16 sx = px - ((INT16)cam_x << 3), sy = py - ((INT16)cam_y << 3);
    if(sx < -7 || sx > 159 || sy < -7 || sy > HUD_WY - 1){ move_sprite(oam, 0, 0); return; }
    set_sprite_tile(oam, tile); set_sprite_prop(oam, 0x08 | pal | flip);
    move_sprite(oam, (UINT8)(sx + 8), (UINT8)(sy + 16));
}
static void worker_sprites(UINT8 f){
    UINT8 i; Wk *w;
    for(i = 0; i < NWK; i++){
        if(i >= HV->pop){ move_sprite(1 + i, 0, 0); continue; }
        w = &HV->wk[i];
        if(w->st == 2){ move_sprite(1 + i, 0, 0); continue; }
        {
            INT16 px = (INT16)w->x * 8, py = (INT16)w->y * 8; UINT8 horiz = w->dir & 1, t, fl = 0;
            if(w->sub){ if(w->dir == 0) py += w->sub; else if(w->dir == 2) py -= w->sub; else if(w->dir == 1) px -= w->sub; else px += w->sub; }
            if(horiz){ t = (w->carry ? TS_ANTHC : (w->sub && (f & 8)) ? TS_ANTH1 : TS_ANTH0); if(w->dir == 3) fl = 0x20; }
            else { t = (w->carry ? TS_ANTVC : (w->sub && (f & 8)) ? TS_ANTV1 : TS_ANTV0); if(w->dir == 2) fl = 0x40; }
            hm_put_sprite(1 + i, px, py, t, w->job, fl);
        }
    }
}
static void weather_sprites(UINT8 f){
    UINT8 k, on = HV->wx == W_RAIN || HV->wx == W_STORM || HV->wx == W_SNOW;
    for(k = 0; k < 8; k++){
        if(!on){ move_sprite(26 + k, 0, 0); continue; }
        if(!wdy[k] && !wdx[k]){ wdx[k] = (UINT8)(k * 37 + 11); wdy[k] = k * 3; }
        wdy[k] += HV->wx == W_SNOW ? 1 : 3; if(wdy[k] > 22){ wdy[k] = 0; wdx[k] = (UINT8)(wdx[k] * 5 + 41 + k); }
        if(HV->wx == W_SNOW && (f & 7) == 0) wdx[k] += (k & 1) ? 1 : 255;
        hm_put_sprite(26 + k, (INT16)((UINT16)wdx[k] * 31 / 8 % 244 + 8), (INT16)wdy[k], HV->wx == W_SNOW ? TS_SPARK : TS_RAIN, 7, 0);
    }
}
static void cursor_sprite(UINT8 f){
    if(f & 32){ move_sprite(0, 0, 0); return; }
    hm_put_sprite(0, (INT16)cux * 8, (INT16)cuy * 8, TS_CURS, 7, 0);
}
/* ---------------------------------------------------------------- the hud */
static void hud_on(void){
    txt_win(1); WX_REG = 7; WY_REG = HUD_WY; SHOW_WIN; SHOW_SPRITES; SHOW_BKG;
}
static void restore_view(void){
    txt_pal_reset(); hm_pals(); hud_on(); SCX_REG = (UINT8)(cam_x << 3); SCY_REG = (UINT8)(cam_y << 3);
}
/* ---------------------------------------------------------------- the loop */
static UINT8 do_menu(void){
    UINT8 r;
    hm_sprites_off(); txt_pal_reset(); WY_REG = 0;
    r = hu_main_menu();
    HV->last = play_secs();
    DISPLAY_OFF; hm_fullmap(); restore_view(); DISPLAY_ON;
    return r;
}
static UINT8 n_budget(void){ return HV->pop > 12 ? 2 : 3; }
void hm_run(void) BANKED {
    UINT8 f = 0, bdown = 0, bmoved = 0, i; UINT16 tf = 0; UINT8 hudf = 0;
    DISPLAY_OFF; HIDE_SPRITES; hv_art_load(); txt_pal_reset(); hm_pals(); SHOW_BKG;
    cux = ENTX; cuy = 7; cam_x = 6; cam_y = 0; hm_look(cux, cuy);
    memset(wdx, 0, sizeof wdx); memset(wdy, 0, sizeof wdy);
    hm_fullmap(); hud_on(); txt_soft(); hu_hud(0); txt_soft_end();
    hs_note(M_HELP); HV->notet = 240;
    DISPLAY_ON;
    for(;;){
        wait_vbl_done(); input_poll(); f++;
        wspd = HV->stat[S_SPEED] ? 3 : 1;
        /* keys */
        if(HITR(J_UP) && cuy > 0){ cuy--; hudf = 1; }
        if(HITR(J_DOWN) && cuy < MH - 1){ cuy++; hudf = 1; }
        if(HITR(J_LEFT) && cux > 0){ cux--; hudf = 1; }
        if(HITR(J_RIGHT) && cux < MW - 1){ cux++; hudf = 1; }
        if(hudf){
            hm_look(cux, cuy);
            if(HELD(J_B) && (HITR(J_UP | J_DOWN | J_LEFT | J_RIGHT))){
                UINT16 i2 = CI(cux, cuy); bmoved = 1;
                if(hs_can_dig(i2) == 1 && !(MAP[i2] & MARKD)){ MAP[i2] |= MARKD; hm_cell(cux, cuy); sfx(SFX_STEP); }
            }
        }
        if(HIT(J_B)){ bdown = 1; bmoved = 0; }
        if(bdown && !HELD(J_B)){ bdown = 0; if(!bmoved){ if(do_menu()) break; hudf = 1; } }
        if(HIT(J_SELECT)){ HV->stat[S_SPEED] ^= 1; hs_note(HV->stat[S_SPEED] ? M_FAST : M_NORMAL); hudf = 1; sfx(SFX_MOVE); }
        if(HIT(J_START)){ if(do_menu()) break; hudf = 1; }
        if(HIT(J_A)){
            UINT16 i2 = CI(cux, cuy); UINT8 t = CT(MAP[i2]), d;
            if(t == C_SOIL || t == C_ORE || t == C_CRYS){
                d = hs_can_dig(i2);
                if(d == 1){ MAP[i2] ^= MARKD; hm_cell(cux, cuy); hs_note((MAP[i2] & MARKD) ? M_MARKED : M_UNMARK); sfx(SFX_PICK); }
                else { hs_note(d == 2 ? M_DEEP : M_NOCELL); sfx(SFX_HURT); }
            } else if(t == C_TUNNEL){
                hm_sprites_off(); txt_pal_reset(); WY_REG = 0;
                hu_build_menu(cux, cuy);
                DISPLAY_OFF; HV->last = play_secs(); hm_fullmap(); restore_view(); DISPLAY_ON;
            } else if(t >= C_QUEEN){
                hm_sprites_off(); txt_pal_reset(); WY_REG = 0;
                hu_cell_menu(cux, cuy);
                DISPLAY_OFF; HV->last = play_secs(); hm_fullmap(); restore_view(); DISPLAY_ON;
            } else { hs_note(M_NOCELL); sfx(SFX_HURT); }
            hudf = 1;
        }
        /* time */
        tf += wspd;
        if(tf >= TICKF){
            tf = 0; hs_tick(); hm_pals(); hudf = 1;
            if(!HV->tod) hive_save();
        }
        /* workers: walking pixels, waiting, and a few decisions a frame */
        {
            UINT8 budget = n_budget(), n = HV->pop;
            for(i = 0; i < n; i++){
                Wk *w = &HV->wk[i];
                if(w->sub){ if(wspd > 1 || (f & 1)) w->sub--; }
                else if(w->tm){ w->tm = w->tm > wspd ? w->tm - wspd : 0; }
                else if(budget){ hs_think(i); budget--; }
            }
        }
        flush_dirty();
        worker_sprites(f); weather_sprites(f); cursor_sprite(f);
        if(HV->notet){ HV->notet--; if(!HV->notet) hudf = 1; }
        if(hudf || !(f & 31)){ txt_soft(); hu_hud(HV->notet ? 1 : 0); txt_soft_end(); hudf = 0; }
        /* the raiders arrive */
        if(HV->rpend){
            hm_sprites_off();
            txt_pal_reset(); WY_REG = 0;
            hu_raid_alert();
            if(HV->stat[S_DEF]){ UINT8 won = hd_run(); HV->stat[S_DEF] = 0; hs_raid_result(won); hu_report(won); }
            else { UINT8 lw = HV->rwon; hs_raid_auto(); hu_report(HV->rwon > lw); }
            HV->last = play_secs(); music(MUS_CAMP);
            DISPLAY_OFF; hm_fullmap(); restore_view(); DISPLAY_ON; hudf = 1; hs_recount();
        }
    }
    hm_sprites_off(); DISPLAY_OFF; HIDE_WIN; HIDE_SPRITES; WY_REG = 144; SCX_REG = 0; SCY_REG = 0;
}
