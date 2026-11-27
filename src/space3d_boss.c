#pragma bank 53
/* 3D Technobeast story encounter: rare boss events and HUD are deliberately
   outside the 16-KB real-time flight bank (19). This leaves normal ship
   steering and pirate hit logic in bank 19 and reuses its P3 actor slots.
   No additional VRAM sprites, save fields, framebuffers or heap allocations. */
#include <gb/gb.h>
#include "save.h"
#include "text.h"
#include "hud.h"
#include "sound.h"
#include "dialog.h"
#include "menus.h"
#include "econ.h"
#include "space3d_int.h"

void s3_boss_spawn(void) BANKED {
    UINT8 i; P3 *p;
    if(story_boss_state || sv.cores < 8 || (sv.story & ST_DONE)) return;
    /* A finale should begin clearly: retire straggling pirate actors and
       their old volleys rather than mixing the mandatory boss with an
       accidental swarm near the four-way galaxy junction. */
    for(i = 0; i < NP3; i++) pir[i].st = 0;
    for(i = 0; i < NO3; i++) orb[i].life = 0;
    p = &pir[0];
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

/* Flight status is refreshed once every eight frames, not every scanline.
   Bank it out of the near-full real-time cockpit code. */
void s3_target_hud(void) BANKED {           /* keep actionable controls in view instead of truncating them after a long planet name */
    char b[28], name[24], *d;
    UINT8 s = near_pl != STAR_NONE ? near_pl : near_st != STAR_NONE ? near_st : tgt;
    if(story_boss_state){ s3_boss_hud(); return; }
    if(near_pl != STAR_NONE || near_st != STAR_NONE){
        d = scat(b, vel > 12 ? "B:BRAKE " : near_pl != STAR_NONE ? "A:LAND B:BRK " : "A:DOCK B:BRK ");
        world_name(name, world_seed(s));
        /* Count the cells left after the controls explicitly. This keeps
           the destination visible in SDCC builds and reserves its NUL. */
        { UINT8 n = 0, left = 20 - (UINT8)(d - b);
          while(left && name[n]){ *d++ = name[n++]; left--; } }
        *d = 0;
    } else if(tgt_rear){
        d = scat(b, "BEHIND! "); d = u16s(d, tgt_d); *d = 0;
    } else {
        char *n;
        world_name(name, world_seed(s));
        d = b; n = name;
        while(*n && d < b + 12) *d++ = *n++;  /* reserve room for the distance */
        *d++ = ' '; d = u16s(d, tgt_d); *d = 0;
    }
    txt_win(1); txt_line(0, 2, b, 20); txt_color(0, 2, 20, 1, (near_pl != STAR_NONE || near_st != STAR_NONE) ? TXT_GOLD : TXT_WHITE); txt_win(0);
}


/* Slow-path world landing and defeat messages belong with the other UI
   events, not in the scarce real-time GBC flight ROM bank. */
void s3_wrecked(void) BANKED {
    UINT8 n; char b[96], *d;
    for(n = 0; n < 40; n++) move_sprite(n, 0, 0);
    sv.gold -= sv.gold / 4; sv.hp = hud_hpmax(sv.level) / 2; if(!sv.hp) sv.hp = 1;
    d = scat(b, "OUCH! THE PIRATES WRECKED THE SHIP. I LIMPED BACK TO "); d = world_name(d, world_seed(sv.star)); scat(d, ". SOME GOLD FELL OUT.");
    dlg_say("BOLT", b);
}
void s3_land(UINT8 s) BANKED {
    char b[64], *d;
    sv.star = s;
    d = scat(b, "LANDING ON "); d = world_name(d, world_seed(s)); d = scat(d, ", A "); d = scat(d, kind_name[star_k[s]]); scat(d, " WORLD.");
    sfx(SFX_STAIRS); dlg_say("BOLT", b);
    if(opt.autosave) slot_write(cur_slot);
}



void s3_boss_reward(void) BANKED {
    /* Exact parity with the classic 2D Technobeast ending, but the combat
       itself is played and rendered in the 3D cockpit. */
    sv.story |= ST_DONE;
    sv.gold += 5000;
    fame_add(300);
    gain_xp(200);
    /* Story dialogue is invoked by the flight caller after returning to bank 19. */
}
