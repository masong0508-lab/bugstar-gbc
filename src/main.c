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
#include "guide.h"
#include "bug.h"
#include "bugspr.h"
#include "bugmake.h"
#include "planet.h"
#include "dungeon.h"
#include "kart.h"
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
#ifdef TEST_MAKE
    DISPLAY_OFF; txt_init(); DISPLAY_ON; { BugLook l; char nm[9]; bug_default(&l); strcpy(nm, "PIP"); bugmake_run(&l, nm, 1); }
    for(;;) wait_vbl_done();
#endif
#ifdef TEST_PLANET
    DISPLAY_OFF; txt_init(); cur_slot = 0; new_game(0, 0x2468ACE1UL, 0, 0); sv.story = ST_INTRO;
#ifdef TEST_STAR
    sv.star = TEST_STAR;
#endif
#ifdef TEST_RICH
    sv.gold = 9000; sv.level = 12; sv.up[SK_MIGHT] = 2; sv.up[UP_GUNS] = 2;
#endif
      /* (test builds: straight onto the planet) */
    play();
#endif
#ifdef TEST_DUNGEON
    DISPLAY_OFF; txt_init(); cur_slot = 0; new_game(0, 0x2468ACE1UL, 0, 0); sv.story = ST_INTRO;
#ifdef TEST_STAR
    sv.star = TEST_STAR;
#endif
#ifdef TEST_LVL
    sv.level = TEST_LVL; sv.hp = hud_hpmax(TEST_LVL); sv.mp = hud_mpmax(TEST_LVL); sv.zone = 0;
#endif
#ifdef TEST_KART
    cpu_fast(); kart_run();
#elif defined(TEST_MOON)
    cpu_fast(); dungeon_hell(1);
#elif defined(TEST_HELL)
    cpu_fast(); dungeon_hell(0);
#else
    cpu_fast(); dungeon_run();
#endif
    for(;;) wait_vbl_done();
#endif
#ifdef TEST_SPACE
    DISPLAY_OFF; txt_init(); cur_slot = 0; new_game(0, 0x2468ACE1UL, 0, 0);   /* (test builds: straight into space) */
#ifdef TEST_RICH
    sv.gold = 9000; sv.level = 12; sv.up[UP_GUNS] = 4; sv.up[UP_ENGINE] = 5; sv.up[UP_LASER] = 4; sv.el.fame = 500; sv.hp = hud_hpmax(12); sv.mp = hud_mpmax(12);
#endif
#ifdef TEST_ZONE
    sv.zone = TEST_ZONE; sv.level = 12; sv.hp = hud_hpmax(12);
#endif
#ifdef TEST_3D
    sv.el.rsv[0] = 3;
#endif
    { int r = space_run(); if(r == SP_LAND) play(); else if(r == SP_END) ending_run(); }
#endif
    for(;;){
        int choice, s;
        title_init();
        choice = title_run(first_good_slot() != 255);
        title_exit();
        if(choice == TI_OPT){ options_screen(); }
        else if(choice == TI_HELP){ guide_run(); }
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
