#pragma bank 3
/* The bug creator, version 2. See bugmake.h for the keys. Layout (20 x 18 tiles):
     row 0        title and START: DONE
     rows 1..12   the stage (x 0..13): the bug at double size (bigbug.c); the tab list and the bug at true size (x 14..19)
     rows 13..16  four rows of the current tab      row 17  a hint */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "sound.h"
#include "wram.h"
#include "bug.h"
#include "bugspr.h"
#include "bigbug.h"
#include "bugmake.h"

enum { TB_BODY, TB_HEAD, TB_GEAR, TB_PAINT, TB_ME, NTAB };
enum { R_COL0 = 32, R_MATCH = 48, R_NAME, R_LOOKS, R_DICE, R_STAGE, R_END = 255 };
#define NPART  BP_N
#define Y_OPT  13
#define N_OPT  3
#define HISTN  6
#define STAGE_N 6
#define RW_X   112                              /* the true-size bug: its box in the right column */
#define RW_Y   65
#define OAM_SW 16                               /* swatches (4) */
#define OAM_STAR 20                             /* twinkling stars (6) */
#define T_SW   236                              /* sprite tiles: a swatch block (236 top, 237 clear) and 3 stars (240, 242, 244) */
#define SPR_STAR 240

static const char *const tab_nm[NTAB] = { "BODY", "HEAD", "GEAR", "PAINT", "ME" };
static const UINT8 tab_rows[NTAB][11] = {
    { BP_LENGTH, BP_SHAPE, BP_FINISH, BP_PAT, BP_LEGS, BP_LEGST, R_END },
    { BP_EYE, BP_MOUTH, BP_ANT, BP_HORN, R_END },
    { BP_HAT, BP_WING, BP_TAIL, R_END },
    { R_COL0 + BC_SHELL, R_COL0 + BC_MARK, R_COL0 + BC_HEAD, R_COL0 + BC_EYE, R_COL0 + BC_HORN, R_COL0 + BC_WING, R_COL0 + BC_HAT, R_COL0 + BC_LINE, R_MATCH, R_END },
    { R_NAME, R_LOOKS, R_DICE, R_STAGE, R_END } };
static const char *const stage_nm[STAGE_N] = { "SKY", "MIDNIGHT", "DUSK", "MEADOW", "CAVE", "CANDY" };
static const UINT16 stage_col[STAGE_N] = { RGB(7,12,24), RGB(2,2,10), RGB(12,5,15), RGB(5,12,7), RGB(9,6,4), RGB(25,13,19) };

typedef struct { BugLook l; char nm[9]; } Snap;
#define HIST ((Snap *)WRAM_AT(WRAM_MAKE2, UINT8))
static BugLook cur;
static char nm[9];
static UINT16 lockp; static UINT8 lockc;
static UINT8 hn, hp;
static UINT8 tab, sel, top, look_i, scheme_i, stage_i;     /* 255 = none picked yet */
static UINT8 pose, flip, t_anim, g_first, shown, last_r, dirty, rebuild, recolor, hint_t;

/* ------------------------------------------------------------------ the things that move */
static const UINT8 star_x[6] = { 18, 40, 77, 96, 30, 90 }, star_y[6] = { 26, 48, 20, 52, 82, 78 };
static const UINT8 star_rows[3][8] = { { 0,0,0,0x10,0,0,0,0 }, { 0,0,0x10,0x38,0x10,0,0,0 }, { 0,0x10,0x10,0x7C,0x10,0x10,0,0 } };
static void stars_load(void){
    UINT8 t[32], r, k;
    memset(t, 0, sizeof t);
    for(r = 0; r < 8; r++) t[r * 2] = 0xFF;                                  /* the swatch: a solid block of colour 1 (top tile) */
    set_sprite_data(T_SW, 2, t);
    for(k = 0; k < 3; k++){
        memset(t, 0, sizeof t);
        for(r = 0; r < 8; r++) t[r * 2] = t[r * 2 + 1] = star_rows[k][r];  /* colour 3 */
        set_sprite_data(SPR_STAR + k * 2, 2, t);
    }
}
static void stars_tick(void){
    UINT8 i, k;
    for(i = 0; i < 6; i++){
        k = ((t_anim >> 3) + i * 5) & 15; k = k < 5 ? 0 : k < 9 ? 1 : k < 11 ? 2 : k < 14 ? 1 : 0;
        set_sprite_tile(OAM_STAR + i, SPR_STAR + k * 2);
        move_sprite(OAM_STAR + i, star_x[i] + 8, star_y[i] + 16);
    }
}
static void stars_setup(void){
    UINT8 i; palette_color_t p[4];
    for(i = 0; i < 6; i++) set_sprite_prop(OAM_STAR + i, 3 | 0x80);              /* palette 3, behind the bug */
    p[0] = 0; p[1] = 0; p[2] = RGB(31,31,31); p[3] = RGB(31,28,12);
    set_sprite_palette(3, 1, p);
}
static void swatch_hide(void){ UINT8 i; for(i = 0; i < 4; i++) move_sprite(OAM_SW + i, 0, 0); }
static void swatch_set(UINT8 i, UINT16 c, UINT8 y){
    palette_color_t p[4];
    p[0] = 0; p[1] = c; p[2] = i == 3 ? RGB(31,31,31) : 0; p[3] = i == 3 ? RGB(31,28,12) : 0;
    set_sprite_palette(i, 1, p);
    set_sprite_tile(OAM_SW + i, T_SW); set_sprite_prop(OAM_SW + i, i);
    move_sprite(OAM_SW + i, 10 * 8 + 8, y + 16);
}

static void show_pose(void){ big_show(pose, flip); shown = 1; }
static void anim(void){
    UINT8 w = bugspr_w(), r = big_ready();
    t_anim++;
    if(r != last_r){ last_r = r; if(r){ if(!(r & 2)) pose = 0; show_pose(); } }      /* a pose finished: put its map up */
    else if((t_anim & 15) == 0 && r == 3){ pose ^= 1; show_pose(); }
    if(t_anim == 0 && shown){ flip ^= 1; show_pose(); }
    bugspr_show(RW_X + (48 - w) / 2, RW_Y, flip, pose);
    stars_tick();
}
static void frame(void){ wait_vbl_done(); input_poll(); big_step(); anim(); }

/* ------------------------------------------------------------------ history, locks */
static void push(void){ Snap *s = &HIST[hp]; s->l = cur; strcpy(s->nm, nm); hp = (hp + 1) % HISTN; if(hn < HISTN) hn++; }
static UINT8 nrows(void){ UINT8 n = 0; while(tab_rows[tab][n] != R_END) n++; return n; }
static UINT8 is_col(UINT8 id){ return id >= R_COL0 && id < R_COL0 + BC_N; }
static UINT8 locked(UINT8 id){
    if(is_col(id)) return (lockc >> (id - R_COL0)) & 1;
    if(id < NPART) return (lockp >> id) & 1;
    return 0;
}
static UINT8 row_on(UINT8 id){ return is_col(id) ? bug_col_on(&cur, id - R_COL0) : 1; }

static void rebuild_now(void){ bugspr_load(&cur); big_start(&cur); big_colors(&cur, stage_col[stage_i]); rebuild = 0; recolor = 0; }
static void recolor_now(void){ bugspr_colors(&cur); big_colors(&cur, stage_col[stage_i]); recolor = 0; }

/* ------------------------------------------------------------------ drawing */
static void draw_tabs(void){
    UINT8 i; char b[8];
    for(i = 0; i < NTAB; i++){
        b[0] = i == tab ? '\012' : ' '; strcpy(b + 1, tab_nm[i]);
        txt_line(14, 2 + i, b, 6); txt_color(14, 2 + i, 6, 1, i == tab ? TXT_GOLD : TXT_WHITE);
    }
}
static void draw_rows(void){
    UINT8 i, id, n = nrows(), y, c, k, on; char b[22], v[12], *d;
    swatch_hide();
    for(i = 0; i < N_OPT; i++){
        y = Y_OPT + i; k = top + i; on = k == sel;
        txt_line(0, y, "", 20); txt_color(0, y, 20, 1, on ? TXT_GOLD : TXT_WHITE);
        if(k >= n) continue;
        id = tab_rows[tab][k];
        d = b; *d++ = on ? '\012' : locked(id) ? '*' : ' '; *d = 0;
        if(id < NPART){ bug_part_label(id, v); d = scat(d, v); }
        else if(is_col(id)){ bug_col_label(id - R_COL0, v); d = scat(d, v); }
        else d = scat(d, id == R_MATCH ? "MATCH" : id == R_NAME ? "NAME" : id == R_LOOKS ? "LOOKS" : id == R_DICE ? "DICE" : "STAGE");
        txt_put(0, y, b);
        d = b;
        if(id < NPART){ *d++ = '<'; bug_part_name(id, cur.pt[id], v); d = scat(d, v); }
        else if(is_col(id)){
            if(!row_on(id)) d = scat(d, "(OFF)");
            else {
                c = bug_swatch_of(cur.col[id - R_COL0]);
                *d++ = '<'; *d++ = ' '; *d++ = ' ';
                if(c == 255) d = scat(d, "CUSTOM"); else { bug_swatch_name(c, v); d = scat(d, v); }
                swatch_set(i, cur.col[id - R_COL0], y * 8);
            }
        }
        else if(id == R_MATCH){ *d++ = '<'; if(scheme_i == 255) d = scat(d, "PICK ONE"); else { bug_scheme_name(scheme_i, v); d = scat(d, v); } }
        else if(id == R_NAME){ *d++ = '<'; d = scat(d, nm); }
        else if(id == R_LOOKS){ *d++ = '<'; if(look_i == 255) d = scat(d, "PICK ONE"); else { bug_preset_name(look_i, v); d = scat(d, v); } }
        else if(id == R_DICE){ d = scat(d, "A: ROLL"); }
        else { *d++ = '<'; d = scat(d, stage_nm[stage_i]); }
        if(b[0] == '<'){ c = (UINT8)(d - b); while(c < 9){ *d++ = ' '; c++; } *d++ = '>'; *d = 0; }
        txt_put(9, y, b);
    }
    if(top > 0) txt_put(19, Y_OPT, S_ARROW_UP);
    if(top + N_OPT < n) txt_put(19, Y_OPT + N_OPT - 1, S_ARROW_DN);
}
static const char *hint_for(UINT8 id, UINT8 page){
    if(page == 2) return "START: ALL DONE";
    if(id < NPART) return page ? "SEL+A: LOCK ROW" : "L/R: CHANGE  B:UNDO";
    if(is_col(id)) return page ? "SEL+A: LOCK ROW" : "L/R: BROWSE  A: MIX";
    switch(id){
        case R_MATCH: return page ? "MAKES COLOURS GO" : "L/R: MATCH SHELL";
        case R_NAME:  return page ? "SELECT: NEW NAME" : "A: TYPE A NAME";
        case R_LOOKS: return "L/R: READY-MADE";
        case R_DICE:  return page ? "KEEPS LOCKED ROWS" : "A: ROLL THE DICE";
    }
    return "L/R: PICK A STAGE";
}
static void draw_hint(void){
    txt_line(0, 17, hint_for(tab_rows[tab][sel], (hint_t >> 7) % 3), 20); txt_color(0, 17, 20, 1, TXT_GREY);
}
static void draw_static(void){
    UINT8 y;
    txt_cls();
    txt_line(0, 0, g_first ? " NEW BUG" : " THE MIRROR", 20); txt_color(0, 0, 20, 1, TXT_GOLD);
    txt_box(0, 1, 14, 12); txt_color(0, 1, 14, 12, TXT_GOLD);
    for(y = 11; y < 13; y++) txt_line(14, y, "", 6);
    draw_tabs(); draw_rows(); draw_hint();
}

/* ------------------------------------------------------------------ name entry, the colour mixer, the last look */
static void clear_opts(void){ UINT8 k; for(k = 0; k < 5; k++){ txt_line(0, Y_OPT + k, "", 20); txt_color(0, Y_OPT + k, 20, 1, TXT_WHITE); } }

static void name_entry(void){
    static const char kb[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-.!?";
    char b[9], t[2]; UINT8 n, k = 0, i, rep = 0, j, blink = 0;
    strcpy(b, nm); n = (UINT8)strlen(b);
    t[1] = 0;
    for(;;){
        clear_opts();
        txt_put(1, Y_OPT, "NAME:"); txt_color(1, Y_OPT, 5, 1, TXT_GREY);
        txt_put(7, Y_OPT, b); if(n < 8 && (blink & 16)) txt_put(7 + n, Y_OPT, "_");
        for(i = 0; i < 30; i++){ t[0] = kb[i]; txt_put((i % 10) * 2, Y_OPT + 1 + i / 10, t); txt_color((i % 10) * 2, Y_OPT + 1 + i / 10, 1, 1, i == k ? TXT_GOLD : TXT_WHITE); }
        txt_line(0, 17, "A:ADD B:DEL START:OK", 20); txt_color(0, 17, 20, 1, TXT_GREY);
        frame(); blink++;
        if(HELD(J_LEFT) || HELD(J_RIGHT) || HELD(J_UP) || HELD(J_DOWN)){
            rep++;
            if(rep == 1 || (rep > 14 && !(rep & 3))){
                j = k % 10; i = k / 10;
                if(HELD(J_RIGHT)) j = (j + 1) % 10; else if(HELD(J_LEFT)) j = (j + 9) % 10;
                if(HELD(J_DOWN)) i = (i + 1) % 3; else if(HELD(J_UP)) i = (i + 2) % 3;
                k = i * 10 + j; sfx(SFX_MOVE);
            }
        } else rep = 0;
        if(HIT(J_A) && n < 8){ b[n++] = kb[k]; b[n] = 0; sfx(SFX_TYPE); }
        if(HIT(J_SELECT)){ bug_random_name(b); n = (UINT8)strlen(b); sfx(SFX_OK); }
        if(HIT(J_B) && n){ b[--n] = 0; sfx(SFX_BACK); }
        if(HIT(J_START)){ if(!n) strcpy(b, "PIP"); strcpy(nm, b); return; }
    }
}

static void mixer(UINT16 *c, UINT8 cid){
    UINT16 old = *c; UINT8 ch = 0, v[3], k, rep = 0; char b[8], lab[10];
    static const char *const nmc[3] = { "RED", "GREEN", "BLUE" };
    v[0] = old & 31; v[1] = (old >> 5) & 31; v[2] = (old >> 10) & 31;
    bug_col_label(cid, lab);
    for(;;){
        clear_opts();
        txt_put(1, Y_OPT, "MIX:"); txt_put(6, Y_OPT, lab); txt_color(1, Y_OPT, 12, 1, TXT_GOLD);
        for(k = 0; k < 3; k++){
            txt_put(1, Y_OPT + 1 + k, k == ch ? S_CURSOR : " "); txt_put(2, Y_OPT + 1 + k, nmc[k]);
            txt_bar(8, Y_OPT + 1 + k, 10, v[k], 31); txt_color(8, Y_OPT + 1 + k, 10, 1, k == ch ? TXT_GOLD : TXT_WHITE);
            u16s(b, v[k]); txt_line(18, Y_OPT + 1 + k, b, 2);
        }
        txt_line(0, 17, "A: OK   B: CANCEL", 20); txt_color(0, 17, 20, 1, TXT_GREY);
        *c = v[0] | (v[1] << 5) | (v[2] << 10); recolor_now();
        { palette_color_t p[4]; p[0] = 0; p[1] = *c; p[2] = p[3] = 0; set_sprite_palette(0, 1, p); set_sprite_tile(OAM_SW, T_SW); set_sprite_prop(OAM_SW, 0); move_sprite(OAM_SW, 17 * 8 + 8, Y_OPT * 8 + 16); }
        frame();
        if(HIT(J_UP) && ch) ch--;
        if(HIT(J_DOWN) && ch < 2) ch++;
        if(HELD(J_LEFT) || HELD(J_RIGHT)){
            rep++;
            if(rep == 1 || (rep > 12 && !(rep & 1))){ if(HELD(J_RIGHT)){ if(v[ch] < 31) v[ch]++; } else if(v[ch]) v[ch]--; }
        } else rep = 0;
        if(HIT(J_A) || HIT(J_START)){ swatch_hide(); return; }
        if(HIT(J_B)){ *c = old; recolor_now(); swatch_hide(); return; }
    }
}

static UINT8 confirm(void){
    char b[22], *d;
    for(;;){
        clear_opts();
        d = scat(b, " READY, "); d = scat(d, nm); scat(d, "?"); txt_put(0, Y_OPT + 1, b); txt_color(0, Y_OPT + 1, 20, 1, TXT_GOLD);
        txt_put(1, Y_OPT + 3, "A: YES!   B: BACK");
        frame();
        if(HIT(J_A) || HIT(J_START)) return 1;
        if(HIT(J_B)) return 0;
    }
}

/* ------------------------------------------------------------------ changing a row */
static void change(UINT8 id, UINT8 up, UINT8 fresh){
    UINT8 k, n;
    if(id < NPART){
        n = bug_part_cnt(id); if(fresh) push();
        cur.pt[id] = (cur.pt[id] + (up ? 1 : n - 1)) % n; look_i = 255; rebuild = 1;
    } else if(is_col(id)){
        k = id - R_COL0; if(!row_on(id)) return;
        if(fresh) push();
        n = bug_swatch_of(cur.col[k]); n = n == 255 ? 0 : (n + (up ? 1 : BUG_NSWATCH - 1)) % BUG_NSWATCH;
        cur.col[k] = bug_swatch(n); look_i = 255; recolor = 1;
    } else if(id == R_LOOKS){
        push(); look_i = look_i == 255 ? (up ? 0 : BUG_NPRESET - 1) : (look_i + (up ? 1 : BUG_NPRESET - 1)) % BUG_NPRESET;
        bug_preset(&cur, look_i, lockp, lockc); rebuild = 1;
    } else if(id == R_MATCH){
        push(); scheme_i = scheme_i == 255 ? (up ? 0 : BUG_NSCHEME - 1) : (scheme_i + (up ? 1 : BUG_NSCHEME - 1)) % BUG_NSCHEME;
        bug_scheme(&cur, BC_SHELL, lockc, scheme_i); look_i = 255; recolor = 1;
    } else if(id == R_STAGE){
        stage_i = (stage_i + (up ? 1 : STAGE_N - 1)) % STAGE_N; recolor = 1;
    }
}
static void act(UINT8 id){
    UINT16 old;
    if(id < NPART) change(id, 1, 1);
    else if(id == R_NAME){ push(); name_entry(); }
    else if(is_col(id)){
        if(row_on(id)){ old = cur.col[id - R_COL0]; push(); mixer(&cur.col[id - R_COL0], id - R_COL0); if(cur.col[id - R_COL0] == old){ hp = (hp + HISTN - 1) % HISTN; hn--; } look_i = 255; }
    }
    else if(id == R_DICE){ push(); bug_surprise(&cur, lockp, lockc); look_i = scheme_i = 255; rebuild = 1; sfx(SFX_OK); }
    else change(id, 1, 1);
}

UINT8 bugmake_run(BugLook *look, char *name, UINT8 first) BANKED {
    UINT8 k, n, rep = 0, sel_down = 0, chord = 0, id, d;
    cur = *look; strcpy(nm, name);
    g_first = first; hn = hp = 0; lockp = 0; lockc = 0; look_i = scheme_i = 255; stage_i = 0;
    tab = 0; sel = 0; top = 0; pose = 0; flip = 0; t_anim = 0; shown = 0; last_r = 0; hint_t = 0; dirty = 0;
    bug_rand_seed((UINT16)(frames32() ^ 0x5A5A));
    DISPLAY_OFF;
    for(k = 0; k < 40; k++) move_sprite(k, 0, 0);
    txt_pal_reset(); draw_static();
    SPRITES_8x16; stars_load(); stars_setup();
    rebuild_now(); big_clear();
    for(k = 0; k < 200 && big_ready() != 3; k++) big_step();     /* both poses are drawn before the screen comes on */
    pose = 0; last_r = big_ready(); show_pose();
    SHOW_SPRITES; DISPLAY_ON;
    for(;;){
        n = nrows();
        if(sel < top) top = sel;
        if(sel >= top + N_OPT) top = sel - N_OPT + 1;
        if(dirty){ dirty = 0; draw_tabs(); draw_rows(); draw_hint(); }
        if(rebuild) rebuild_now(); else if(recolor) recolor_now();
        frame();
        if(((++hint_t) & 127) == 0) draw_hint();
        if(HIT(J_UP)){ sel = sel ? sel - 1 : n - 1; dirty = 1; sfx(SFX_MOVE); }
        if(HIT(J_DOWN)){ sel = (sel + 1) % n; dirty = 1; sfx(SFX_MOVE); }
        if(HIT(J_SELECT)){ sel_down = 1; chord = 0; }
        if(sel_down && HIT(J_A)){
            chord = 1; id = tab_rows[tab][sel];
            if(id < NPART) lockp ^= (UINT16)1 << id; else if(is_col(id)) lockc ^= 1 << (id - R_COL0);
            dirty = 1; continue;
        }
        if(sel_down && !HELD(J_SELECT)){
            sel_down = 0;
            if(!chord){ tab = (tab + 1) % NTAB; sel = 0; top = 0; dirty = 1; sfx(SFX_MOVE); }
            continue;
        }
        if(sel_down) continue;
        id = tab_rows[tab][sel]; d = 0;
        if(HELD(J_LEFT) || HELD(J_RIGHT)){
            rep++;
            if(rep == 1) d = 1; else if(rep > 20 && !(rep & 7)) d = 2;
        } else rep = 0;
        if(d){ change(id, HELD(J_RIGHT) ? 1 : 0, d == 1); dirty = 1; sfx(SFX_MOVE); }
        else if(HIT(J_A)){ act(id); dirty = 1; }
        if(HIT(J_START)){ if(confirm()) break; dirty = 1; }
        if(HIT(J_B)){
            if(hn){ hp = (hp + HISTN - 1) % HISTN; hn--; cur = HIST[hp].l; strcpy(nm, HIST[hp].nm); look_i = scheme_i = 255; rebuild = 1; dirty = 1; sfx(SFX_BACK); }
            else { bugspr_hide(); swatch_hide(); txt_pal_reset(); txt_cls(); return 0; }
        }
    }
    *look = cur; strcpy(name, nm);
    bugspr_hide(); swatch_hide();
    for(k = 0; k < 6; k++) move_sprite(OAM_STAR + k, 0, 0);
    txt_pal_reset(); txt_cls();
    return 1;
}
