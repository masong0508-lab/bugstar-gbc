#pragma bank 32
/* BUGSTAR GBC - FLEET (see fleet.h): the ship builder, wingmen, drone outposts and the FLEET menu at a station. The ship is drawn by code:
   a body (4 shapes), wings (4), guns (3) on a 16x16 canvas facing up, turned into the 8 flying directions, in 4 colour themes... */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "wram.h"
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "econ.h"
#include "sysmenu.h"
#include "space_int.h"
#include "space.h"
#include "fleet.h"

FHot fh;                                            /* (the rest of the block, and the wingmen, sit in WRAM bank 7: the always-there RAM is full) */
#define FSV WRAM_AT(0xD000, FSave)
#define fsv (*FSV)
typedef struct { UINT16 x[3], y[3]; UINT8 cool[3], init, props; } WState;
#define WNG WRAM_AT(0xD100, WState)
#ifdef __SDCC
#define wram_now() SVBK_REG
#else
#define wram_now() 1
#endif
#define FB() wram_bank(7)
static UINT8 fs_sum(void){ const UINT8 *p = (const UINT8 *)&fsv; UINT8 i, s = 0x5A; for(i = 0; i < sizeof(FSave) - 1; i++) s ^= (UINT8)(p[i] + i * 5); return s; }
static void fleet_load(void) {
    UINT8 *p, i, o = wram_now();
    wram_bank(7); p = (UINT8 *)&fsv;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof(FSave); i++) p[i] = *(volatile UINT8 *)(0xA000U + FS_OFF + i);
    DISABLE_RAM;
    if(fsv.seed != sv.seed || fsv.chk != fs_sum()){ memset(&fsv, 0, sizeof(FSave)); fsv.seed = sv.seed; }
    fh.body = fsv.body; fh.wing = fsv.wing; fh.gun = fsv.gun; fh.col = fsv.col; fh.wm = fsv.wm;
    wram_bank(o);
}
static void fleet_save(void) {
    UINT8 *p, i, o = wram_now();
    wram_bank(7); p = (UINT8 *)&fsv;
    fsv.body = fh.body; fsv.wing = fh.wing; fsv.gun = fh.gun; fsv.col = fh.col; fsv.wm = fh.wm;
    fsv.seed = sv.seed; fsv.chk = fs_sum();
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof(FSave); i++) *(volatile UINT8 *)(0xA000U + FS_OFF + i) = p[i];
    DISABLE_RAM;
    wram_bank(o);
    if(opt.autosave) slot_write(cur_slot);
}

/* ---------------------------------------------------------------- the ship, drawn by code */
static const palette_color_t themes[6 * 4] = {
    RGB(0, 0, 0), RGB(6, 4, 8),  RGB(26, 6, 8),   RGB(31, 24, 20),     /* CLASSIC (the original colours) */
    RGB(0, 0, 0), RGB(2, 5, 14), RGB(6, 16, 28),  RGB(20, 28, 31),     /* SKY */
    RGB(0, 0, 0), RGB(2, 8, 3),  RGB(8, 22, 8),   RGB(24, 31, 14),     /* LIME */
    RGB(0, 0, 0), RGB(8, 5, 1),  RGB(26, 18, 3),  RGB(31, 30, 14),     /* GOLD */
    RGB(0, 0, 0), RGB(6, 2, 12), RGB(18, 8, 26),  RGB(28, 20, 31),     /* VIOLET */
    RGB(0, 0, 0), RGB(6, 7, 12), RGB(20, 22, 26), RGB(31, 31, 31)      /* SNOW */
};
static void fleet_theme(UINT8 col, palette_color_t *out4) { UINT8 i; if(col > 5) col = 0; for(i = 0; i < 4; i++) out4[i] = themes[col * 4 + i]; }

static const INT8 body_hw[4][16] = {                 /* half width of each row (the middle pair of pixels is 0), -1: nothing */
    { -1, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 0, -1 },        /* CLASSIC: slim */
    { -1, -1, 0, 0, 1, 1, 1, 2, 2, 2, 1, 1, 1, 0, 0, -1 },       /* NIMBLE: a dart */
    { -1, 0, 2, 3, 3, 4, 4, 4, 4, 4, 4, 3, 3, 2, 1, -1 },        /* ARMORED: fat */
    { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 2, 2, 1, 0 }           /* STRIKER: long and blunt */
};
static const INT8 wing_hw[4][16] = {
    { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },   /* NONE */
    { -1, -1, -1, -1, -1, -1, -1, -1, -1, 5, 6, 6, 5, -1, -1, -1 },       /* SHORT */
    { -1, -1, -1, -1, -1, -1, -1, -1, 6, 7, 7, 7, 7, 6, -1, -1 },         /* LONG */
    { -1, -1, -1, -1, -1, -1, 3, 4, 5, 5, 6, 6, 7, 7, -1, -1 }            /* SWEPT */
};
static const char *const body_nm[4] = { "CLASSIC", "NIMBLE", "ARMORED", "STRIKER" };
static const char *const wing_nm[4] = { "NONE", "SHORT", "LONG", "SWEPT" };
static const char *const gun_nm[3]  = { "NONE", "NOSE", "TWIN" };
static const char *const col_nm[6]  = { "CLASSIC", "SKY", "LIME", "GOLD", "VIOLET", "SNOW" };
static const INT16 cs[8] = { 256, 181, 0, -181, -256, -181, 0, 181 };
static const INT16 sn[8] = { 0, 181, 256, 181, 0, -181, -256, -181 };
#define SRC WRAM_AT(0xD000, UINT8)                  /* the ship facing up, 16x16 (WRAM bank 2) */
#define ROT WRAM_AT(0xD100, UINT8)                  /* ... turned */
#define TIL WRAM_AT(0xD200, UINT8)                  /* ... as 4 tiles */

static void draw_up(void){
    INT8 ext[16]; UINT8 y; INT8 k;
    UINT8 *p = SRC;
    for(y = 0; y < 16; y++){ INT8 a = body_hw[fh.body & 3][y], b = wing_hw[fh.wing & 3][y]; ext[y] = a > b ? a : b; }
    if(fh.gun == 1){ if(ext[0] < 0) ext[0] = 0; if(ext[1] < 0) ext[1] = 0; }
    memset(p, 0, 256);
    for(y = 0; y < 16; y++){
        INT8 e = ext[y], up = y ? ext[y - 1] : -1, dn = y < 15 ? ext[y + 1] : -1;
        for(k = 0; k <= e; k++){
            UINT8 c = (k == e || up < k || dn < k) ? 1 : 2;       /* outline, else the shaded inside */
            if(c == 2) c = 3;                                      /* left half light... */
            p[y * 16 + 7 - k] = c;
            p[y * 16 + 8 + k] = (c == 3) ? 2 : c;                  /* ...right half mid */
        }
    }
    for(y = 4; y < 7; y++){ if(ext[y] >= 0){ p[y * 16 + 7] = 1; p[y * 16 + 8] = 1; } }   /* the cockpit */
    if(fh.gun == 2){ for(y = 6; y < 9; y++){ p[y * 16 + 4] = 1; p[y * 16 + 11] = 1; } }  /* twin guns */
    p[14 * 16 + 7] = 3; p[14 * 16 + 8] = 3; p[15 * 16 + 7] = 3; p[15 * 16 + 8] = 3;     /* the engine glow */
}
static void turn(UINT8 d){
    INT8 x, y; INT16 X, Y, sx, sy, c = cs[d], s = sn[d]; UINT8 *src = SRC, *dst = ROT;
    for(y = 0; y < 16; y++) for(x = 0; x < 16; x++){
        X = 2 * x - 15; Y = 2 * y - 15;
        sx = (INT16)((c * X + s * Y) >> 8); sy = (INT16)((c * Y - s * X) >> 8);
        sx = (sx + 15) >> 1; sy = (sy + 15) >> 1;
        dst[y * 16 + x] = (sx >= 0 && sx < 16 && sy >= 0 && sy < 16) ? src[sy * 16 + sx] : 0;
    }
}
static void to_tiles(const UINT8 *px){                /* the 16x16 pixels into 4 tiles: left column top, bottom, right column top, bottom */
    UINT8 t, r, c, lo, hi, v, xo, yo; UINT8 *o = TIL;
    for(t = 0; t < 4; t++){
        xo = (t >> 1) * 8; yo = (t & 1) * 8;
        for(r = 0; r < 8; r++){
            lo = hi = 0;
            for(c = 0; c < 8; c++){ v = px[(yo + r) * 16 + xo + c]; lo |= (UINT8)((v & 1) << (7 - c)); hi |= (UINT8)(((v >> 1) & 1) << (7 - c)); }
            *o++ = lo; *o++ = hi;
        }
    }
}
static UINT8 *fleet_ship_tiles(UINT8 dir) {
    wram_bank(2);
    draw_up(); turn(dir & 7); to_tiles(ROT);
    return TIL;
}
static UINT8 is_default(void){ return !fh.body && !fh.wing && !fh.gun; }
static void apply_ship(void){                          /* every direction into space's ship tiles (VRAM bank 1); LCD off or at least no hurry */
    UINT8 d, o = wram_now();
    VBK_REG = VBK_BANK_1;
    for(d = 0; d < 8; d++){
        if(is_default()) space_spr_copy(SPR0 + SP_T_SHIP(d), SP_T_SHIP(d), 4);
        else { UINT8 *t = fleet_ship_tiles(d); set_sprite_data(SPR0 + SP_T_SHIP(d), 4, t); wram_bank(o); }
    }
    VBK_REG = VBK_BANK_0;
}
void fleet_ship_gfx(void) BANKED {
    palette_color_t c[4];
    fleet_load();
    if(!is_default()){ VBK_REG = VBK_BANK_1; apply_ship(); }
    fleet_theme(fh.col, c);
    set_sprite_palette(0, 1, c);
    VBK_REG = VBK_BANK_0;
}

/* ---------------------------------------------------------------- wingmen */
static const INT8 woff_x[3] = { -24, 24, 0 };
static const INT8 woff_y[3] = { 16, 16, 34 };
void fleet_wing_hide(void) BANKED { UINT8 i, o = wram_now(); for(i = 0; i < 3; i++) hide2(14 + i * 2); wram_bank(7); WNG->init = 0; wram_bank(o); }
void fleet_wing(UINT8 frame, UINT16 sx, UINT16 sy, UINT8 dir, UINT16 cx, UINT16 cy) BANKED {
    UINT8 i, o = wram_now(); WState *w;
    if(!fh.wm){ if(!(frame & 15)) for(i = 0; i < 3; i++) hide2(14 + i * 2); return; }
    wram_bank(7); w = WNG;
    for(i = 0; i < 3; i++){
        INT16 dx, dy;
        if(i >= fh.wm){ hide2(14 + i * 2); continue; }
        if(!w->init){ w->x[i] = (sx + woff_x[i] * 2) & 4095; w->y[i] = (sy + woff_y[i] * 2) & 4095; }
        dx = wrapd((sx + woff_x[i]) & 4095, w->x[i]); dy = wrapd((sy + woff_y[i]) & 4095, w->y[i]);
        dx >>= 3; dy >>= 3;
        if(dx > 3) dx = 3; if(dx < -3) dx = -3; if(dy > 3) dy = 3; if(dy < -3) dy = -3;
        w->x[i] = (w->x[i] + dx) & 4095; w->y[i] = (w->y[i] + dy) & 4095;
        if(!(frame & 31) || !w->props){ set_sprite_prop(14 + i * 2, 0x08); set_sprite_prop(15 + i * 2, 0x08); }
        spr16(14 + i * 2, SP_T_SHIP(dir), wrapd(w->x[i], cx), wrapd(w->y[i], cy));
        if(w->cool[i]) w->cool[i]--;
        else {
            INT16 tx, ty;
            if(foes_target(w->x[i], w->y[i], &tx, &ty)){
                INT16 m = (tx < 0 ? -tx : tx) > (ty < 0 ? -ty : ty) ? (tx < 0 ? -tx : tx) : (ty < 0 ? -ty : ty);
                if(m){ if(space_laser_add(w->x[i], w->y[i], (INT8)(tx * 5 / m), (INT8)(ty * 5 / m))) w->cool[i] = 36 + i * 9; }
            }
        }
    }
    w->init = 1; w->props = 1; wram_bank(o);
}

/* ---------------------------------------------------------------- the FLEET menu */
static UINT8 flat;                                    /* 1 in the 2D view (the builder and battles need it) */
static UINT8 outpost_at(UINT8 s){ return s && ((UINT16)s * 73 + (UINT8)sv.seed) % 5 == 0; }
static UINT8 captured(UINT8 s){ return (fsv.out[s >> 3] >> (s & 7)) & 1; }
static UINT8 n_outposts(void){ UINT8 i, n = 0; for(i = 0; i < 32; i++){ UINT8 b = fsv.out[i]; while(b){ n += b & 1; b >>= 1; } } return n; }
static UINT16 income(void){
    UINT8 n = n_outposts(); UINT32 now = play_secs(), dt;
    if(!n) return 0;
    dt = now > fsv.coll ? (now - fsv.coll) / 10 : 0; if(dt > 100) dt = 100;
    return (UINT16)(dt * n);
}
static UINT16 wing_price(void){ return fh.wm == 0 ? 150 : fh.wm == 1 ? 300 : 600; }
static void frame_clear(void){ txt_win(1); txt_cls(); txt_box(0, 0, 20, 18); txt_win(0); move_win(7, 0); SHOW_WIN; }
static void put(UINT8 y, const char *s, UINT8 pal){ txt_line(1, y, s, 18); txt_color(1, y, 18, 1, pal); }
static void key_wait(void){ for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B) || HIT(J_START)) return; } }

/* the SHIP BUILDER */
static void builder_stats(void){
    char b[21], *d;
    d = scat(b, "SPEED +"); d = u16s(d, FL_SPEED()); *d = 0; txt_line(1, 10, b, 9); txt_color(1, 10, 9, 1, TXT_WHITE);
    d = scat(b, "ARMOR +"); d = u16s(d, FL_ARMOR()); *d = 0; txt_line(1, 11, b, 9); txt_color(1, 11, 9, 1, TXT_WHITE);
    d = scat(b, "LASER +"); d = u16s(d, FL_GUN()); *d = 0; txt_line(1, 12, b, 9); txt_color(1, 12, 9, 1, TXT_WHITE);
}
static void builder_rows(UINT8 sel){
    static const char *const lab[4] = { "BODY", "WINGS", "GUNS", "COLOUR" };
    UINT8 i; char b[21], *d; const char *v;
    for(i = 0; i < 4; i++){
        v = i == 0 ? body_nm[fh.body] : i == 1 ? wing_nm[fh.wing] : i == 2 ? gun_nm[fh.gun] : col_nm[fh.col];
        d = scat(b, i == sel ? S_CURSOR : " "); d = scat(d, lab[i]);
        while((UINT8)(d - b) < 8) *d++ = ' ';
        *d++ = '<'; d = scat(d, v); *d++ = '>'; *d = 0;
        txt_line(1, 4 + i, b, 18); txt_color(1, 4 + i, 18, 1, i == sel ? TXT_GOLD : TXT_WHITE);
    }
    builder_stats();
}
static void preview(void){                            /* the ship facing up, into the first ship tiles, and the colours */
    palette_color_t c[4]; UINT8 *t, o = wram_now();
    fleet_theme(fh.col, c); set_sprite_palette(0, 1, c);
    VBK_REG = VBK_BANK_1;
    if(is_default()) space_spr_copy(SPR0 + SP_T_SHIP(0), SP_T_SHIP(0), 4);
    else { t = fleet_ship_tiles(0); set_sprite_data(SPR0 + SP_T_SHIP(0), 4, t); wram_bank(o); }
    VBK_REG = VBK_BANK_0;
}
static void builder(void){
    UINT8 sel = 0, sp = LCDC_REG & LCDCF_OBJON;
    frame_clear();
    txt_win(1); put(1, "SHIP BUILDER", TXT_GOLD);
    txt_line(1, 14, "UP/DOWN: PART", 18); txt_color(1, 14, 18, 1, TXT_GREY);
    txt_line(1, 15, "LEFT/RIGHT: CHANGE", 18); txt_color(1, 15, 18, 1, TXT_GREY);
    txt_line(1, 16, "B: DONE", 18); txt_color(1, 16, 18, 1, TXT_GREY);
    builder_rows(sel); txt_win(0);
    preview();
    set_sprite_prop(0, 0x08); set_sprite_prop(1, 0x08);
    spr16(0, SP_T_SHIP(0), 124, 90);
    SHOW_SPRITES;
    for(;;){
        UINT8 ch = 0;
        wait_vbl_done(); input_poll();
        if(HIT(J_UP)){ sel = sel ? sel - 1 : 3; sfx(SFX_MOVE); ch = 2; }
        if(HIT(J_DOWN)){ sel = sel < 3 ? sel + 1 : 0; sfx(SFX_MOVE); ch = 2; }
        if(HIT(J_LEFT) || HIT(J_RIGHT)){
            INT8 dd = HIT(J_RIGHT) ? 1 : -1;
            if(sel == 0) fh.body = (UINT8)((fh.body + 4 + dd) & 3);
            else if(sel == 1) fh.wing = (UINT8)((fh.wing + 4 + dd) & 3);
            else if(sel == 2) fh.gun = (UINT8)((fh.gun + 3 + dd) % 3);
            else fh.col = (UINT8)((fh.col + 6 + dd) % 6);
            sfx(SFX_OK); ch = 1;
        }
        if(ch){ txt_win(1); builder_rows(sel); txt_win(0); if(ch == 1) preview(); }
        if(HIT(J_B) || HIT(J_START)) break;
    }
    hide2(0);
    if(!sp) HIDE_SPRITES;
    DISPLAY_OFF; apply_ship(); DISPLAY_ON;                 /* every direction, for space */
    sfx(SFX_BACK);
    fleet_save();
}

/* WINGMEN */
static void wing_screen(void){
    UINT8 cur = 0; char b[21], *d; const char *msg = 0;
    frame_clear();
    for(;;){
        txt_win(1);
        put(1, "WINGMEN", TXT_GOLD);
        d = scat(b, "HIRED "); d = u16s(d, fh.wm); d = scat(d, " OF 3"); *d = 0; put(3, b, TXT_WHITE);
        if(fh.wm < 3){ d = scat(b, "NEXT COSTS "); d = u16s(d, wing_price()); d = scat(d, " G"); *d = 0; put(4, b, TXT_GOLD); }
        else put(4, "A FULL WING", TXT_GOLD);
        d = scat(b, "GOLD "); d = u16s(d, sv.gold > 65000UL ? 65000U : (UINT16)sv.gold); *d = 0; put(6, b, TXT_GREY);
        put(8, "ESCORTS FLY BESIDE YOU", TXT_GREY); put(9, "IN 2D SPACE AND SHOOT", TXT_GREY); put(10, "PIRATES. THEY FIGHT IN", TXT_GREY); put(11, "FLEET BATTLES TOO.", TXT_GREY);
        put(13, "A: HIRE   SELECT: DISMISS", TXT_WHITE);
        put(14, "B: BACK", TXT_WHITE);
        put(16, msg ? msg : "", TXT_RED);
        txt_win(0); msg = 0; cur = 0;
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A)){
                if(fh.wm >= 3){ msg = "ALREADY A FULL WING"; sfx(SFX_BACK); }
                else if(sv.gold < wing_price()){ msg = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
                else { sv.gold -= wing_price(); fh.wm++; fleet_save(); msg = "A WINGMAN JOINS YOU!"; sfx(SFX_FANFARE); }
                break;
            }
            if(HIT(J_SELECT) && fh.wm){ fh.wm--; fleet_save(); msg = "A WINGMAN LEAVES"; sfx(SFX_BACK); break; }
        }
    }
}

/* DRONE OUTPOSTS and FLEET BATTLES */
static UINT8 battle(UINT8 tier, UINT8 boss, const char *what, UINT16 *gold_out){
    UINT8 kills = 0, won; UINT8 sp = LCDC_REG & LCDCF_OBJON, o = wram_now();
    DISPLAY_OFF;
    won = fleet_battle(tier, boss, &kills);
    wram_bank(o);
    DISPLAY_OFF;
    HIDE_SPRITES;
    { UINT8 k; for(k = 0; k < 40; k++) move_sprite(k, 0, 0); }
    space_restore();                                    /* space's pictures, sprite colours and flags again */
    (void)what; (void)sp;
    *gold_out = won ? (UINT16)(60 + 40 * tier + kills * 6) : 0;
    DISPLAY_ON;
    return won;
}
static void outposts_screen(UINT8 star){
    UINT8 n, here_op; char b[21], *d; const char *msg = 0; UINT16 g, inc;
    frame_clear();
    for(;;){
        n = n_outposts(); inc = income(); here_op = outpost_at(star) && !captured(star);
        txt_win(1);
        put(1, "DRONE OUTPOSTS", TXT_GOLD);
        d = scat(b, "YOURS "); d = u16s(d, n); *d = 0; put(3, b, TXT_WHITE);
        d = scat(b, "INCOME WAITING "); d = u16s(d, inc); *d = 0; put(4, b, inc ? TXT_GOLD : TXT_GREY);
        put(5, "(A GOLD COIN PER 10 SEC", TXT_GREY); put(6, "PER OUTPOST, UP TO 100)", TXT_GREY);
        if(outpost_at(star)){ put(8, captured(star) ? "THIS STAR: YOURS" : "THIS STAR HAS AN OUTPOST", captured(star) ? TXT_GOLD : TXT_RED); }
        else put(8, "NO OUTPOST AT THIS STAR", TXT_GREY);
        put(10, inc ? "A: COLLECT" : "A: -", inc ? TXT_WHITE : TXT_GREY);
        put(11, here_op ? (flat ? "SELECT: ATTACK IT" : "(2D VIEW TO ATTACK)") : "", here_op && flat ? TXT_WHITE : TXT_GREY);
        put(13, "B: BACK", TXT_WHITE);
        put(15, msg ? msg : "", TXT_RED);
        txt_win(0); msg = 0;
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A) && inc){ sv.gold += inc; fsv.coll = play_secs(); fleet_save(); msg = "COLLECTED!"; sfx(SFX_COIN); break; }
            if(HIT(J_SELECT) && here_op && flat){
                UINT8 tier = 2 + (sv.zone > 5 ? 5 : sv.zone), won = battle(tier, 1, "OUTPOST", &g);
                frame_clear();
                if(won){
                    if(!n_outposts()) fsv.coll = play_secs();
                    fsv.out[star >> 3] |= 1 << (star & 7); fsv.wins++; sv.gold += g + 150; fame_add(5); fleet_save();
                    msg = "OUTPOST TAKEN! +GOLD"; sfx(SFX_FANFARE);
                } else { msg = "THE DRONES HELD. TRY AGAIN"; sfx(SFX_BACK); }
                break;
            }
        }
    }
}
static void battle_screen(void){
    UINT8 tier = 1 + (fsv.wins > 21 ? 7 : fsv.wins / 3), won; UINT16 g; char b[21], *d; const char *msg = 0;
    frame_clear();
    for(;;){
        tier = 1 + (fsv.wins > 21 ? 7 : fsv.wins / 3);
        txt_win(1);
        put(1, "FLEET BATTLE", TXT_GOLD);
        d = scat(b, "WON "); d = u16s(d, fsv.wins); *d = 0; put(3, b, TXT_WHITE);
        d = scat(b, "FOES TIER "); d = u16s(d, tier); *d = 0; put(4, b, TXT_WHITE);
        d = scat(b, "WINGMEN "); d = u16s(d, fh.wm); *d = 0; put(5, b, TXT_WHITE);
        put(7, "THREE WAVES OF DRONES.", TXT_GREY); put(8, "WIN FOR GOLD AND FAME.", TXT_GREY);
        put(9, "A: FIRE  START: RETREAT", TXT_GREY);
        put(11, flat ? "A: FIGHT!" : "(2D VIEW TO FIGHT)", flat ? TXT_WHITE : TXT_GREY);
        put(13, "B: BACK", TXT_WHITE);
        put(15, msg ? msg : "", TXT_RED);
        txt_win(0); msg = 0;
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A) && flat){
                won = battle(tier, 0, "RAID", &g);
                frame_clear();
                if(won){ fsv.wins++; sv.gold += g; fame_add(2); fleet_save(); msg = "VICTORY! GOLD WON"; sfx(SFX_FANFARE); }
                else { msg = "YOU FELL BACK"; sfx(SFX_BACK); }
                break;
            }
        }
    }
}

static void hq_run(UINT8 star){
    UINT8 sel = 0, i; char b[21], *d; const char *msg = 0;
    static const char *const item[5] = { "SHIP BUILDER", "WINGMEN", "OUTPOSTS", "FLEET BATTLE", "BACK" };
    flat = !V3_ON;
    frame_clear();
    for(;;){
        txt_win(1);
        put(1, "FLEET HQ", TXT_GOLD);
        d = scat(b, "WINGMEN "); d = u16s(d, fh.wm); d = scat(d, "/3  WON "); d = u16s(d, fsv.wins); *d = 0; put(3, b, TXT_WHITE);
        d = scat(b, "OUTPOSTS "); d = u16s(d, n_outposts()); d = scat(d, "  +"); d = u16s(d, income()); *d = 0; put(4, b, TXT_WHITE);
        for(i = 0; i < 5; i++){
            d = scat(b, i == sel ? S_CURSOR : " "); d = scat(d, item[i]); *d = 0;
            put(7 + i * 2, b, i == sel ? TXT_GOLD : TXT_WHITE);
        }
        put(17, msg ? msg : "", TXT_RED);
        txt_win(0); msg = 0;
        for(;;){
            UINT8 ch = 0;
            wait_vbl_done(); input_poll();
            if(HIT(J_UP)){ sel = sel ? sel - 1 : 4; sfx(SFX_MOVE); ch = 1; }
            if(HIT(J_DOWN)){ sel = sel < 4 ? sel + 1 : 0; sfx(SFX_MOVE); ch = 1; }
            if(HIT(J_B) || HIT(J_START)){ sfx(SFX_BACK); return; }
            if(HIT(J_A)){
                sfx(SFX_OK);
                if(sel == 4){ sfx(SFX_BACK); return; }
                if(sel == 0){ if(!flat){ msg = "SWITCH TO THE 2D VIEW"; ch = 1; } else { builder(); frame_clear(); ch = 1; } }
                else if(sel == 1){ wing_screen(); frame_clear(); ch = 1; }
                else if(sel == 2){ outposts_screen(star); frame_clear(); ch = 1; }
                else { battle_screen(); frame_clear(); ch = 1; }
            }
            if(ch) break;
        }
    }
}
void fleet_hq(UINT8 star) BANKED {
    UINT8 o = wram_now();
    fleet_load(); FB();
    hq_run(star);
    wram_bank(o);
}
