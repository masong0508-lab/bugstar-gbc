#pragma bank 53
/* 3D Technobeast story encounter: rare boss events and HUD are deliberately
   outside the 16-KB real-time flight bank (19). This leaves normal ship
   steering and pirate hit logic in bank 19 and reuses its P3 actor slots.
   No additional VRAM sprites, save fields, framebuffers or heap allocations. */
#include <gb/gb.h>
#include "save.h"
#include "text.h"
#include "hud.h"
#include "menus.h"
#include "econ.h"
#include "space3d_int.h"

void s3_boss_spawn(void) BANKED {
    UINT8 i; P3 *p;
    if(story_boss_state || sv.cores < 8 || (sv.story & ST_DONE)) return;
    for(i = 0; i < NP3; i++) if(!pir[i].st) break;
    if(i == NP3) i = NP3 - 1;      /* reserve a real actor slot for mandatory boss */
    p = &pir[i];
    p->x = (UINT16)BEAST_X << 4; p->y = (UINT16)BEAST_Y << 4;
    p->z = 0; p->st = 1; p->hp = BEAST_HP; p->t = 0; p->kind = 2;
    story_boss_state = 1;
}

void s3_boss_arrow(void) BANKED {
    UINT8 i; edge_on = 0;
    for(i = 0; i < NP3; i++) if(pir[i].st == 1 && pir[i].kind == 2){
        edge_on = 1;
        ed_x = unit(pir[i].x, shx); ed_y = unit(pir[i].y, shy);
        ed_z = (pir[i].z - sz) >> 4;
        return;
    }
}

void s3_boss_hud(void) BANKED {
    char b[28], *d;
    if(story_boss_state == 2) scat(b, "TECHNOBEAST FALLS!");
    else {
        UINT8 i, hp = 0, open = 0;
        for(i = 0; i < NP3; i++) if(pir[i].st == 1 && pir[i].kind == 2){
            hp = pir[i].hp; open = (pir[i].t & 31) < 18; break;
        }
        d = scat(b, open ? "CORE OPEN HP " : "ARMOR CLOSED HP ");
        u16s(d, hp);
    }
    txt_win(1); txt_line(0, 2, b, 20);
    txt_color(0, 2, 20, 1, TXT_GOLD); txt_win(0);
}

void s3_boss_reward(void) BANKED {
    /* Exact parity with the classic 2D Technobeast ending, but the combat
       itself is played and rendered in the 3D cockpit. */
    sv.story |= ST_DONE;
    sv.gold += 5000;
    fame_add(300);
    gain_xp(200);
    space_say(SAY_DAWN);
}
