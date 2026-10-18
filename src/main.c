/* BUGSTAR GBC: title, options, 3-slot save, bug creator, then the game: a world (planet.c, with its dungeon) and space between
   the worlds (space.c). See PLAN.md */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdio.h>
#include <string.h>
#include "sound.h"
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
#include "space.h"
#include "ending.h"

static UINT32 make_seed(void){
    UINT32 s = ((UINT32)title_frames << 16) ^ (frames32() << 3) ^ ((UINT32)DIV_REG << 20) ^ DIV_REG;
    s = s * 1664525UL + 1013904223UL;
    return s ? s : 0x5EED1234UL;
}

static void play(void){                                       /* land, fly, land... until the person quits to the title */
    for(;;){
        if(planet_run() == PL_TITLE) return;
        { int r = space_run();
          if(r == SP_TITLE) return;
          if(r == SP_END){ ending_run(); return; } }
    }
}

void main(void){
    if(_cpu != CGB_TYPE) { printf("NEEDS GAME BOY COLOR"); return; }
    cpu_fast();                       /* double speed: 8 MHz */
    save_init();
    sound_init(); dlg_set_blip(sfx_type);
    DISPLAY_ON;
#ifdef TEST_SPACE
    DISPLAY_OFF; txt_init(); cur_slot = 0; new_game(0, 0x2468ACE1UL, 0, 0);   /* (test builds: straight into space) */
    { int r = space_run(); if(r == SP_LAND) play(); else if(r == SP_END) ending_run(); }
#endif
    for(;;){
        int choice, s;
        title_init();
        choice = title_run(first_good_slot() != 255);
        title_exit();
        if(choice == TI_OPT){ options_screen(); }
        else if(choice == TI_CONT){ cur_slot = first_good_slot(); if(cur_slot != 255 && slot_read(cur_slot)) play(); }
        else if(choice == TI_LOAD){ s = slot_picker(0); if(s >= 0 && slot_read(s)){ cur_slot = s; play(); } }
        else {                                                    /* TI_NEW */
            s = slot_picker(1);
            if(s >= 0){
                BugLook l; char nm[9];
                bug_default(&l); strcpy(nm, "PIP"); txt_cls();
                if(bugmake_run(&l, nm, 1)){ cur_slot = s; new_game(s, make_seed(), &l, nm); play(); }
            }
        }
        txt_cls();
    }
}
