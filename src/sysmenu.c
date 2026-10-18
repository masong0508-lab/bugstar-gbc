#include <gb/gb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "menus.h"
#include "sysmenu.h"

#define NITEM 4
#define ROW0  3                              /* first item row inside the box */
#define OFFX  167                            /* window X with the box just off the right edge */
static const char *const item[NITEM] = { "RESUME", "SAVE", "OPTIONS", "TO TITLE" };

static void slide_x(UINT8 from, UINT8 to){   /* 8 pixels per frame */
    UINT8 x = from;
    while(x != to){
        if(x > to) x = (x - to > 8) ? x - 8 : to; else x = (to - x > 8) ? x + 8 : to;
        move_win(x, SM_WY);
        wait_vbl_done();
    }
}

static void draw_items(UINT8 sel){
    UINT8 i; char b[12];
    txt_win(1);
    for(i = 0; i < NITEM; i++){
        char *d = scat(b, i == sel ? S_CURSOR : " ");
        scat(d, item[i]);
        txt_line(1, ROW0 + i, b, SM_W - 2);
    }
    txt_win(0);
}

static void status(const char *s){
    txt_win(1); txt_line(1, 2, s, SM_W - 2); txt_color(1, 2, SM_W - 2, 1, TXT_GOLD); txt_win(0);
}

static void cores(void){ char b[12], *d = scat(b, "CORES "); d = u16s(d, sv.cores); scat(d, "/8"); status(b); }
static UINT8 confirm_title(void){
    UINT8 i;
    txt_win(1);
    for(i = 0; i < NITEM; i++) txt_line(1, ROW0 + i, "", SM_W - 2);
    txt_line(1, ROW0 + 1, "SURE?", SM_W - 2); txt_line(1, ROW0 + 2, " A: YES", SM_W - 2); txt_line(1, ROW0 + 3, " B: NO", SM_W - 2);
    txt_win(0);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_A)) return 1;
        if(HIT(J_B)) return 0;
    }
}

int sysmenu_run(void (*redraw)(void)){
    UINT8 sel = 0, had = hud_is_on();
    int result = SYS_RESUME;
    sfx(SFX_OK);
    move_win(OFFX, SM_WY); SHOW_WIN;                 /* the HUD vanishes: this box takes over the window */
    txt_win(1);
    txt_cls(); txt_box(0, 0, SM_W, SM_H);
    txt_line(1, 1, "SYSTEM", SM_W - 2); txt_color(1, 1, SM_W - 2, 1, TXT_GOLD);
    txt_win(0);
    draw_items(sel); cores();
    slide_x(OFFX, SM_WX);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : NITEM - 1; cores(); draw_items(sel); sfx(SFX_MOVE); }
        if(HIT(J_DOWN)) { sel = (sel + 1) % NITEM;         cores(); draw_items(sel); sfx(SFX_MOVE); }
        if(HIT(J_B) || HIT(J_START)) break;
        if(HIT(J_A)){
            if(sel == 0) break;
            if(sel == 1){ slot_write(cur_slot); status("SAVED!"); sfx(SFX_COIN); }
            else if(sel == 2){
                HIDE_WIN;
                { UINT8 sx = SCX_REG, sy = SCY_REG, sp = LCDC_REG & LCDCF_OBJON;   /* the options page is drawn unscrolled, without sprites over it */
                  SCX_REG = 0; SCY_REG = 0; HIDE_SPRITES; txt_cls(); options_screen(); txt_cls(); SCX_REG = sx; SCY_REG = sy; if(sp) SHOW_SPRITES; }
                if(redraw) redraw();
                SHOW_WIN; cores();
            } else {
                if(confirm_title()){ if(opt.autosave) slot_write(cur_slot); result = SYS_TITLE; break; }
                draw_items(sel);
            }
        }
    }
    slide_x(SM_WX, OFFX);
    if(had){ hud_redraw(); hud_pos(HUD_Y); }          /* hud_pos puts the window back at X = 7 */
    else HIDE_WIN;
    return result;
}
