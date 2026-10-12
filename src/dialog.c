#include <gb/gb.h>
#include "input.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"

static UINT8 speed = 2;
static void (*blip)(void);
static UINT8 win_y;                                  /* where the window is right now */

void dlg_set_speed(UINT8 f){ speed = f; }
void dlg_set_blip(void (*fn)(void)){ blip = fn; }

UINT8 dlg_next_line(const char **pp, char *out){
    const char *s = *pp; UINT8 i, n, last = 0;
    while(*s == ' ') s++;
    if(!*s){ *pp = s; return 255; }
    for(i = 0; s[i] && s[i] != '\n' && i < DLG_W; i++) if(s[i] == ' ') last = i;
    if(s[i] && s[i] != '\n' && s[i] != ' ' && last) i = last;     /* would cut a word: back up to the last space */
    n = i;
    for(i = 0; i < n; i++) out[i] = s[i];
    while(n && out[n - 1] == ' ') n--;
    out[n] = 0;
    s += i;
    if(*s == '\n') s++;
    *pp = s;
    return n;
}

static UINT8 frame(void){                           /* one frame; 1 if A or B was just pressed */
    wait_vbl_done(); input_poll();
    return HIT(J_A | J_B) ? 1 : 0;
}

static void slide(UINT8 to){                        /* 8 pixels per frame */
    while(win_y != to){
        if(win_y < to) win_y = (to - win_y > 8) ? win_y + 8 : to;
        else           win_y = (win_y - to > 8) ? win_y - 8 : to;
        hud_pos(win_y);
        wait_vbl_done();
    }
}

static void put_tile(UINT8 x, UINT8 y, UINT8 tile){
    char c[2]; c[0] = (char)tile; c[1] = 0;
    txt_win(1); txt_put(x, y, c); txt_win(0);
}

static void type_line(UINT8 row, const char *s, UINT8 n, UINT8 *rush){
    UINT8 k, t;
    for(k = 0; k < n; k++){
        if(*rush){ txt_win(1); txt_put(1 + k, row, s + k); txt_win(0); return; }   /* the rest in one go */
        put_tile(1 + k, row, (UINT8)s[k]);
        if(s[k] != ' ' && blip) blip();
        for(t = 0; t < speed; t++) if(frame()){ *rush = 1; break; }
    }
}

static void wait_more(void){
    UINT8 t = 0;
    for(;;){
        if(!(t & 15)) put_tile(DLG_W, DLG_ROWS - 1, (t & 16) ? T_BOX_B : T_ARROW_DN);   /* blink on the bottom border */
        t++;
        if(frame()) break;
    }
    put_tile(DLG_W, DLG_ROWS - 1, T_BOX_B);
}

void dlg_say(const char *name, const char *text){
    char buf[DLG_W + 1]; const char *p = text;
    UINT8 had = hud_is_on(), cap = name ? 3 : 4, first = name ? 2 : 1, n, rows, rush, r;
    hud_pos(144); win_y = 144; SHOW_WIN;             /* park the window off screen, draw, then slide up */
    txt_win(1);
    txt_cls(); txt_box(0, 0, 20, DLG_ROWS);
    if(name){ txt_line(1, 1, name, DLG_W); txt_color(1, 1, DLG_W, 1, TXT_GOLD); }
    txt_win(0);
    slide(DLG_Y);
    for(;;){
        rows = 0; rush = 0;
        while(rows < cap){
            n = dlg_next_line(&p, buf);
            if(n == 255) break;
            type_line(first + rows, buf, n, &rush);
            rows++;
        }
        if(!rows) break;
        wait_more();
        while(*p == ' ' || *p == '\n') p++;
        if(!*p) break;
        txt_win(1);
        for(r = 0; r < cap; r++) txt_line(1, first + r, "", DLG_W);                  /* next page */
        txt_win(0);
    }
    slide(144);
    if(had){ hud_redraw(); slide(HUD_Y); }          /* HUD slides back in */
    else HIDE_WIN;
}
