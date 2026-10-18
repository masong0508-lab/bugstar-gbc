#pragma bank 3
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "bug.h"
#include "bugspr.h"
#include "bugmake.h"

#define R_NAME  0
#define R_LOOKS 1
#define R_PART0 2
#define R_MATCH (R_PART0 + BP_N)
#define R_COL0  (R_MATCH + 1)
#define NROWS   (R_COL0 + BC_N)
#define LIST_Y  8
#define LIST_N  6
#define HISTN   8
#define PREV_Y  20                       /* canvas top inside the preview box */

typedef struct { BugLook l; char nm[9]; } Snap;
static BugLook cur;
static char nm[9];
static UINT16 lockp; static UINT8 lockc;
static Snap hist[HISTN]; static UINT8 hn, hp;
static UINT8 look_i, scheme_i;             /* 255 = none picked yet */
static UINT8 t_anim, facing, g_first;

static void anim(void){
    UINT8 w = bugspr_w();
    t_anim++;
    if(t_anim == 0) facing ^= 1;
    bugspr_show((160 - w) / 2, PREV_Y, facing, (t_anim >> 4) & 1);
}
static void frame(void){ wait_vbl_done(); input_poll(); anim(); }

static void push(void){ memcpy(&hist[hp].l, &cur, sizeof cur); strcpy(hist[hp].nm, nm); hp = (hp + 1) & (HISTN - 1); if(hn < HISTN) hn++; }
static UINT8 locked(UINT8 row){
    if(row >= R_COL0) return (lockc >> (row - R_COL0)) & 1;
    if(row >= R_PART0 && row < R_MATCH) return (lockp >> (row - R_PART0)) & 1;
    return 0;
}
static UINT8 row_on(UINT8 row){ return row < R_COL0 || bug_col_on(&cur, row - R_COL0); }

static void swatch_pal(UINT8 j, UINT16 c){ palette_color_t p[4]; p[0] = p[1] = p[2] = p[3] = c; set_bkg_palette(2 + j, 1, p); }

static void draw_list(UINT8 sel, UINT8 top){
    UINT8 j, i, y; char b[22], *d;
    for(j = 0; j < LIST_N; j++){
        i = top + j; y = LIST_Y + j;
        txt_line(0, y, "", 20);
        txt_color(0, y, 20, 1, i == sel ? TXT_GOLD : TXT_WHITE);
        if(i >= NROWS) continue;
        d = b; *d++ = i == sel ? T_CURSOR : locked(i) ? '*' : ' ';
        if(i == R_NAME) d = scat(d, "NAME");
        else if(i == R_LOOKS) d = scat(d, "LOOKS");
        else if(i == R_MATCH) d = scat(d, "MATCH");
        else d = scat(d, i < R_MATCH ? bug_part_label[i - R_PART0] : bug_col_label[i - R_COL0]);
        *d = 0; txt_put(0, y, b);
        d = b; *d++ = '<';
        if(i == R_NAME) d = scat(d, nm);
        else if(i == R_LOOKS) d = scat(d, look_i == 255 ? "PICK ONE" : bug_preset_name(look_i));
        else if(i == R_MATCH) d = scat(d, scheme_i == 255 ? "PICK ONE" : bug_scheme_name[scheme_i]);
        else if(i < R_MATCH) d = scat(d, bug_part_names[i - R_PART0][cur.pt[i - R_PART0]]);
        else if(row_on(i)){
            UINT8 c = i - R_COL0, k; const char *nmx = "CUSTOM";
            for(k = 0; k < BUG_NSWATCH; k++) if(bug_swatch[k] == cur.col[c]) nmx = bug_swatch_name[k];
            *d++ = ' '; *d++ = ' '; *d++ = ' '; *d++ = '>'; *d = 0; txt_put(10, y, b);
            txt_put(11, y, S_BLOCK S_BLOCK); txt_color(11, y, 2, 1, 2 + j); swatch_pal(j, cur.col[c]);
            txt_put(15, y, nmx);
            continue;
        } else { d = b; d = scat(d, "(OFF)"); *d = 0; txt_put(10, y, b); continue; }
        *d++ = '>'; *d = 0;
        txt_put(10, y, b);
    }
}

static void note_line(const char *s){ txt_line(0, 14, s, 20); txt_color(0, 14, 20, 1, TXT_GOLD); }
static void draw_all(UINT8 sel, UINT8 top){
    char b[22], *d;
    txt_line(0, 0, g_first ? " MAKE YOUR BUG" : " THE MIRROR", 20);
    d = u16s(b, sel + 1); *d++ = '/'; u16s(d, NROWS); txt_put(14, 0, b);
    txt_box(0, 1, 20, 7);
    draw_list(sel, top);
    note_line(sel == R_NAME ? " A: TYPE A NAME" : sel == R_LOOKS ? " L/R: READY-MADE LOOK" : sel == R_MATCH ? " L/R: MATCHING COLOURS" :
              sel >= R_COL0 ? (row_on(sel) ? " L/R: BROWSE  A: MIX" : " TURN THE PART ON") : " L/R: CHANGE IT");
    txt_color(0, 14, 20, 1, TXT_WHITE);
    txt_line(0, 15, " SELECT: SURPRISE ME", 20);
    txt_line(0, 16, " SELECT+A: LOCK ROW", 20);
    d = b; if(hn){ d = scat(d, " B:UNDO "); *d++ = '0' + hn; *d++ = ' '; *d = 0; } else d = scat(d, g_first ? " B:BACK " : " B:CLOSE ");
    scat(d, "START:DONE"); txt_line(0, 17, b, 20);
}

static void name_entry(void){
    static const char cs[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    UINT8 n = (UINT8)strlen(nm), cur_i = 0, k;
    char b[9];
    strcpy(b, nm); if(!n){ strcpy(b, "PIP"); n = 3; }
    for(;;){
        for(k = 0; k < LIST_N; k++){ txt_line(0, LIST_Y + k, "", 20); txt_color(0, LIST_Y + k, 20, 1, TXT_WHITE); }
        txt_put(1, LIST_Y, "WHAT IS YOUR NAME?"); txt_color(1, LIST_Y, 18, 1, TXT_GOLD);
        txt_line(6, LIST_Y + 2, b, 8);
        txt_put(6 + cur_i, LIST_Y + 3, S_ARROW_UP);
        txt_put(1, LIST_Y + 5, "UP/DOWN LETTER  A:OK");
        frame();
        if(HIT(J_UP) || HIT(J_DOWN)){
            if(cur_i >= n){ b[cur_i] = 'A'; n = cur_i + 1; b[n] = 0; }
            else { for(k = 0; k < 37 && cs[k] != b[cur_i]; k++){} if(k == 37) k = 0; k = HIT(J_UP) ? (k + 1) % 37 : (k + 36) % 37; b[cur_i] = cs[k]; }
        }
        if(HIT(J_RIGHT) && cur_i < 7){ cur_i++; if(cur_i >= n){ b[cur_i] = 'A'; n = cur_i + 1; b[n] = 0; } }
        if(HIT(J_LEFT) && cur_i) cur_i--;
        txt_put(6, LIST_Y + 3, "        ");
        if(HIT(J_A) || HIT(J_START) || HIT(J_B)){
            while(n && b[n - 1] == ' ') n--;
            b[n] = 0; if(!n) strcpy(b, "PIP");
            strcpy(nm, b); return;
        }
    }
}

static void mixer(UINT16 *c){
    UINT16 old = *c; UINT8 ch = 0, v[3], k, rep = 0; char b[8]; palette_color_t p[4];
    static const palette_color_t bar[3][4] = {
        {RGB(1,1,6), RGB(10,2,2), RGB(24,6,6), RGB(31,14,14)}, {RGB(1,1,6), RGB(2,10,2), RGB(6,24,6), RGB(14,31,14)}, {RGB(1,1,6), RGB(2,2,12), RGB(8,8,28), RGB(18,18,31)} };
    static const char *const nmc[3] = { "RED", "GREEN", "BLUE" };
    (void)p;
    for(k = 0; k < 3; k++) set_bkg_palette(4 + k, 1, bar[k]);
    v[0] = old & 31; v[1] = (old >> 5) & 31; v[2] = (old >> 10) & 31;
    for(;;){
        for(k = 0; k < LIST_N; k++){ txt_line(0, LIST_Y + k, "", 20); txt_color(0, LIST_Y + k, 20, 1, TXT_WHITE); }
        txt_put(1, LIST_Y, "MIX YOUR OWN"); txt_color(1, LIST_Y, 12, 1, TXT_GOLD);
        for(k = 0; k < 3; k++){
            txt_line(0, LIST_Y + 1 + k, k == ch ? S_CURSOR : " ", 1); txt_put(1, LIST_Y + 1 + k, nmc[k]);
            txt_bar(6, LIST_Y + 1 + k, 10, v[k], 31); txt_color(6, LIST_Y + 1 + k, 10, 1, 4 + k);
            u16s(b, v[k]); txt_line(17, LIST_Y + 1 + k, b, 2);
        }
        txt_put(1, LIST_Y + 5, "A:OK  B:CANCEL");
        *c = v[0] | (v[1] << 5) | (v[2] << 10); bugspr_colors(&cur);
        frame();
        if(HIT(J_UP) && ch) ch--;
        if(HIT(J_DOWN) && ch < 2) ch++;
        if(HELD(J_LEFT) || HELD(J_RIGHT)){
            rep++;
            if(rep == 1 || (rep > 12 && !(rep & 1))){ if(HELD(J_RIGHT)){ if(v[ch] < 31) v[ch]++; } else if(v[ch]) v[ch]--; }
        } else rep = 0;
        if(HIT(J_A) || HIT(J_START)){ txt_pal_reset(); return; }
        if(HIT(J_B)){ *c = old; bugspr_colors(&cur); return; }
    }
}

UINT8 bugmake_run(BugLook *look, char *name, UINT8 first) BANKED {
    BugLook before; char bnm[9];
    UINT8 sel = 0, top = 0, dirty = 1, rep = 0, sel_down = 0, chord = 0, note_t = 0, k, d, fresh, rebuild;
    UINT16 old;
    memcpy(&before, look, sizeof before); strcpy(bnm, name);
    cur = *look; strcpy(nm, name);
    g_first = first; hn = hp = 0; lockp = 0; lockc = 0; look_i = scheme_i = 255; t_anim = 0; facing = 0;
    bug_rand_seed((UINT16)(frames32() ^ 0x5A5A));
    txt_cls(); bugspr_load(&cur);
    for(;;){
        if(dirty){
            dirty = 0;
            if(sel < top) top = sel;
            if(sel >= top + LIST_N) top = sel - LIST_N + 1;
            draw_all(sel, top);
        }
        frame();
        if(note_t && !--note_t) dirty = 1;
        if(HIT(J_UP)){ sel = sel ? sel - 1 : NROWS - 1; dirty = 1; }
        if(HIT(J_DOWN)){ sel = (sel + 1) % NROWS; dirty = 1; }
        /* SELECT: tap = surprise, SELECT + A = lock */
        if(HIT(J_SELECT)){ sel_down = 1; chord = 0; }
        if(sel_down && HIT(J_A)){
            chord = 1;
            if(sel >= R_PART0 && sel < R_MATCH) lockp ^= (UINT16)1 << (sel - R_PART0);
            else if(sel >= R_COL0) lockc ^= 1 << (sel - R_COL0);
            dirty = 1;
            continue;
        }
        if(sel_down && !HELD(J_SELECT)){
            sel_down = 0;
            if(!chord){ push(); bug_surprise(&cur, lockp, lockc); look_i = scheme_i = 255; bugspr_load(&cur); dirty = 1; }
            continue;
        }
        if(sel_down) continue;
        d = 0; fresh = 0; rebuild = 0;
        if(HELD(J_LEFT) || HELD(J_RIGHT)){
            rep++; fresh = rep == 1;
            if(fresh || (rep > 14 && !(rep & 3) && sel >= R_COL0)) d = HELD(J_RIGHT) ? 1 : 255;
        } else rep = 0;
        if(HIT(J_A) && sel >= R_PART0 && sel < R_MATCH){ d = 1; fresh = 1; }
        if(d){
            if(sel >= R_PART0 && sel < R_MATCH){
                k = sel - R_PART0; push();
                cur.pt[k] = (cur.pt[k] + (d == 1 ? 1 : bug_part_n[k] - 1)) % bug_part_n[k]; look_i = 255; rebuild = 1;
            } else if(sel == R_LOOKS){
                push(); look_i = look_i == 255 ? (d == 1 ? 0 : BUG_NPRESET - 1) : (look_i + (d == 1 ? 1 : BUG_NPRESET - 1)) % BUG_NPRESET;
                bug_preset(&cur, look_i, lockp, lockc); rebuild = 1;
            } else if(sel == R_MATCH){
                push(); scheme_i = scheme_i == 255 ? (d == 1 ? 0 : BUG_NSCHEME - 1) : (scheme_i + (d == 1 ? 1 : BUG_NSCHEME - 1)) % BUG_NSCHEME;
                bug_scheme(&cur, BC_SHELL, lockc, scheme_i); look_i = 255; bugspr_colors(&cur);
            } else if(sel >= R_COL0 && row_on(sel)){
                k = sel - R_COL0; if(fresh) push();
                for(d = 0; d < BUG_NSWATCH; d++) if(bug_swatch[d] == cur.col[k]) break;
                d = d == BUG_NSWATCH ? 0 : (d + (HELD(J_RIGHT) ? 1 : BUG_NSWATCH - 1)) % BUG_NSWATCH;
                cur.col[k] = bug_swatch[d]; look_i = 255; bugspr_colors(&cur);
            }
            if(rebuild) bugspr_load(&cur);
            dirty = 1;
        }
        if(HIT(J_A)){
            if(sel == R_NAME){ push(); name_entry(); dirty = 1; }
            else if(sel >= R_COL0 && row_on(sel)){ push(); old = cur.col[sel - R_COL0]; mixer(&cur.col[sel - R_COL0]); if(cur.col[sel - R_COL0] == old){ hp = (hp + HISTN - 1) & (HISTN - 1); hn--; } look_i = 255; dirty = 1; }
        }
        if(HIT(J_START)) break;
        if(HIT(J_B)){
            if(hn){ hp = (hp + HISTN - 1) & (HISTN - 1); hn--; cur = hist[hp].l; strcpy(nm, hist[hp].nm); look_i = scheme_i = 255; bugspr_load(&cur); dirty = 1; }
            else { bugspr_hide(); txt_pal_reset(); txt_cls(); return 0; }
        }
    }
    *look = cur; strcpy(name, nm);
    bugspr_hide(); txt_pal_reset(); txt_cls();
    (void)before; (void)bnm; (void)note_t;
    return 1;
}
