/* BUGSTAR GBC - step 5: title, options, 3-slot save, text engine, HUD, dialogue, SYSTEM menu, bug creator. The game itself comes in later steps. See PLAN.md */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdio.h>
#include <string.h>
#include "input.h"
#include "title.h"
#include "save.h"
#include "menus.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"
#include "sysmenu.h"
#include "bug.h"
#include "bugspr.h"
#include "bugmake.h"

static UINT32 make_seed(void){
    UINT32 s = ((UINT32)title_frames << 16) ^ (frames32() << 3) ^ ((UINT32)DIV_REG << 20) ^ DIV_REG;
    s = s * 1664525UL + 1013904223UL;
    return s ? s : 0x5EED1234UL;
}

/* Placeholder "in game" screen: proves the save, HUD, dialogue and SYSTEM menu work together.
   UP/DOWN level, LEFT/RIGHT hp, SELECT gold (all test keys), A save, B talk, START = SYSTEM menu. */
static UINT8 g_dirty, g_shown;
static void game_draw(void){                         /* the static part of the screen; also the SYSTEM menu's repaint hook */
    char b[24], *d;
    put_line(1, 1, "BUGSTAR", 18);
    d = b; d = scat(d, "SLOT "); *d++ = '1' + cur_slot; *d++ = ' '; scat(d, sv.name); put_line(1, 3, b, 18);
    d = scat(b, "LEVEL "); u16s(d, sv.level); put_line(1, 4, b, 18);
    d = scat(b, "SEED "); fmt_hex32(d, sv.seed); put_line(1, 6, b, 18);
    d = scat(b, "SAVED "); d = u16s(d, sv.saves); scat(d, " TIMES"); put_line(1, 7, b, 18);
    put_line(1, 10, "UP/DOWN: LEVEL +-1", 18);
    put_line(1, 11, "LEFT/RIGHT: HP -+3", 18);
    put_line(1, 12, "SELECT: GOLD+12345", 18);
    put_line(1, 13, "A:SAVE   B:TALK", 18);
    put_line(1, 14, "START: SYSTEM", 18);
    g_shown = 255;                                    /* the clock line repaints too */
    g_dirty = 0;
}

static void game_screen(void){
    char b[24], *d; UINT8 msg = 0; UINT32 secs; UINT8 bx = 40; INT8 bdir = 1;
    play_reset(sv.secs);
    txt_cls();
    hud_init();
    g_dirty = 1;
    bugspr_load(&sv.look);
    for(;;){
        bx += bdir; if(bx > 100 || bx < 8){ bdir = -bdir; bx += bdir; }
        bugspr_show(bx, 60, bdir < 0, (UINT8)((bx >> 3) & 1));
        secs = play_secs();
        if(g_dirty) game_draw();
        if((UINT8)(secs & 0xFF) != g_shown){ g_shown = (UINT8)(secs & 0xFF); d = scat(b, "TIME "); fmt_time(d, secs); put_line(1, 9, b, 18); }
        hud_update();
        wait_vbl_done(); input_poll();
        if(HIT(J_START)){
            if(sysmenu_run(game_draw) == SYS_TITLE){ hud_hide(); bugspr_hide(); txt_cls(); return; }
            bugspr_load(&sv.look);
            g_dirty = 1;
        }
        if(HIT(J_B)){
            dlg_say("NARRATOR", "WELCOME TO BUGSTAR. THIS IS THE TYPEWRITER BOX. HOLD A TO RUSH THE TEXT, AND PRESS A AT THE ARROW FOR MORE.\nTHE HUD SLIDES AWAY WHILE WE TALK.");
            g_dirty = 1;
        }
        if(HIT(J_UP) && sv.level < 99){ sv.level++; g_dirty = 1; }
        if(HIT(J_DOWN) && sv.level > 1){ sv.level--; g_dirty = 1; }
        if(HIT(J_LEFT))  sv.hp = sv.hp > 3 ? sv.hp - 3 : 0;
        if(HIT(J_RIGHT)) sv.hp += 3;
        if(HIT(J_SELECT)) sv.gold += 12345UL;
        if(sv.hp > hud_hpmax(sv.level)) sv.hp = hud_hpmax(sv.level);   /* level down must not leave hp above max */
        if(sv.mp > hud_mpmax(sv.level)) sv.mp = hud_mpmax(sv.level);
        if(msg && !--msg) put_line(1, 15, "", 18);
        if(HIT(J_A)){ slot_write(cur_slot); put_line(1, 15, "SAVED!", 18); msg = 90; g_dirty = 1; }
    }
}

void main(void){
    if(_cpu != CGB_TYPE) { printf("NEEDS GAME BOY COLOR"); return; }
    cpu_fast();                       /* double speed: 8 MHz */
    save_init();
    DISPLAY_ON;
    for(;;){
        int choice, s;
        title_init();
        choice = title_run(first_good_slot() != 255);
        title_exit();
        if(choice == TI_OPT){ options_screen(); }
        else if(choice == TI_CONT){ cur_slot = first_good_slot(); if(cur_slot != 255 && slot_read(cur_slot)) game_screen(); }
        else if(choice == TI_LOAD){ s = slot_picker(0); if(s >= 0 && slot_read(s)){ cur_slot = s; game_screen(); } }
        else {                                                    /* TI_NEW */
            s = slot_picker(1);
            if(s >= 0){
                BugLook l; char nm[9];
                bug_default(&l); strcpy(nm, "PIP"); txt_cls();
                if(bugmake_run(&l, nm, 1)){ cur_slot = s; new_game(s, make_seed(), &l, nm); game_screen(); }
            }
        }
        txt_cls();
    }
}
