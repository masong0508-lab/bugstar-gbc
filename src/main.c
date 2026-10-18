/* BUGSTAR GBC - step 6 (the planet): title, options, 3-slot save, text engine, HUD, dialogue, SYSTEM menu, bug creator. The game itself comes in later steps. See PLAN.md */
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
#include "planet.h"

static UINT32 make_seed(void){
    UINT32 s = ((UINT32)title_frames << 16) ^ (frames32() << 3) ^ ((UINT32)DIV_REG << 20) ^ DIV_REG;
    s = s * 1664525UL + 1013904223UL;
    return s ? s : 0x5EED1234UL;
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
        else if(choice == TI_CONT){ cur_slot = first_good_slot(); if(cur_slot != 255 && slot_read(cur_slot)) planet_run(); }
        else if(choice == TI_LOAD){ s = slot_picker(0); if(s >= 0 && slot_read(s)){ cur_slot = s; planet_run(); } }
        else {                                                    /* TI_NEW */
            s = slot_picker(1);
            if(s >= 0){
                BugLook l; char nm[9];
                bug_default(&l); strcpy(nm, "PIP"); txt_cls();
                if(bugmake_run(&l, nm, 1)){ cur_slot = s; new_game(s, make_seed(), &l, nm); planet_run(); }
            }
        }
        txt_cls();
    }
}
