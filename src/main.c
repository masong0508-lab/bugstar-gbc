/* BUGSTAR GBC - step 4a: title, options, 3-slot save, own font + text engine. The game itself comes in later steps. See PLAN.md */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdio.h>
#include "input.h"
#include "title.h"
#include "save.h"
#include "menus.h"
#include "text.h"

static UINT32 make_seed(void){
    UINT32 s = ((UINT32)title_frames << 16) ^ (frames32() << 3) ^ ((UINT32)DIV_REG << 20) ^ DIV_REG;
    s = s * 1664525UL + 1013904223UL;
    return s ? s : 0x5EED1234UL;
}

/* Placeholder "in game" screen: proves the save works. UP = +1 level (test), A = save, B = back to title. */
static void game_screen(void){
    char b[24], *d; UINT8 dirty = 1, shown = 255, msg = 0; UINT32 secs;
    play_reset(sv.secs);
    txt_cls();
    for(;;){
        secs = play_secs();
        if(dirty){
            dirty = 0;
            put_line(1, 1, "BUGSTAR", 18);
            d = b; d = scat(d, "SLOT "); *d++ = '1' + cur_slot; *d++ = ' '; scat(d, sv.name); put_line(1, 3, b, 18);
            d = scat(b, "LEVEL "); u16s(d, sv.level); put_line(1, 4, b, 18);
            d = scat(b, "SEED "); fmt_hex32(d, sv.seed); put_line(1, 6, b, 18);
            d = scat(b, "SAVED "); d = u16s(d, sv.saves); scat(d, " TIMES"); put_line(1, 7, b, 18);
            put_line(1, 12, "UP: LEVEL +1 (TEST)", 18);
            put_line(1, 13, "A: SAVE", 18);
            put_line(1, 14, "B: TITLE", 18);
        }
        if((UINT8)(secs & 0xFF) != shown){ shown = (UINT8)(secs & 0xFF); d = scat(b, "TIME "); fmt_time(d, secs); put_line(1, 9, b, 18); }
        wait_vbl_done(); input_poll();
        if(HIT(J_UP) && sv.level < 99){ sv.level++; dirty = 1; }
        if(msg && !--msg) put_line(1, 16, "", 18);
        if(HIT(J_A)){ slot_write(cur_slot); put_line(1, 16, "SAVED!", 18); msg = 90; dirty = 1; }
        if(HIT(J_B)){ if(opt.autosave) slot_write(cur_slot); txt_cls(); return; }
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
            if(s >= 0){ cur_slot = s; new_game(s, make_seed()); game_screen(); }
        }
        txt_cls();
    }
}
