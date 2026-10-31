#include <gb/gb.h>
#include "sound.h"
#include "save.h"
#include "menus.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"
#include "wram.h"

/* window map columns */
#define HP_BAR_X 1      /* 6 tiles incl. caps */
#define HP_NUM_X 7      /* 3 digits */
#define LV_X     11     /* "LV 99" */
#define GOLD_X   1      /* up to 9 digits */
#define MP_ICON  10
#define MP_BAR_X 11     /* 6 tiles incl. caps */
#define MP_NUM_X 17     /* 3 digits */
#define BAR_W    6
#define GOLD_MAX 999999999UL

static UINT16 l_hp, l_mp, l_lv; static UINT32 l_gold;

UINT16 hud_hpmax(UINT16 level){ return 5 + 5 * level + 4 * UPG(SK_GRIT); }
UINT16 hud_mpmax(UINT16 level){ return 2 + 3 * level + 8 * UPG(UP_SHIELD); }   /* (in space the MP bar is the ship's shield) */

static UINT8 on;
void hud_pos(UINT8 y){ move_win(7, y); }
void hud_show(void){ hud_pos(HUD_Y); SHOW_WIN; on = 1; }
void hud_hide(void){ HIDE_WIN; on = 0; }
UINT8 hud_is_on(void){ return on; }

void hud_redraw(void){
    txt_win(1);
    txt_cls();                                       /* clears the window map and its attributes */
    txt_put(0, 0, S_HEART);
    txt_put(0, 1, S_COIN);
    txt_put(MP_ICON, 1, S_STAR);
    txt_color(MP_ICON, 1, 1, 1, TXT_GOLD);
    txt_win(0);
    l_hp = l_mp = l_lv = 0xFFFF; l_gold = 0xFFFFFFFFUL;   /* impossible values: everything redraws once */
    hud_update();
}
void hud_init(void){ hud_redraw(); hud_show(); }

void hud_update(void){
    char b[12]; UINT16 lvmax; UINT8 hp_changed, lv_changed;
    lv_changed = sv.level != l_lv;
    hp_changed = sv.hp != l_hp || lv_changed;
    txt_win(1);
    if(hp_changed){
        lvmax = hud_hpmax(sv.level);
        txt_bar(HP_BAR_X, 0, BAR_W, sv.hp, lvmax);
        u16s(b, sv.hp); txt_line(HP_NUM_X, 0, b, 3);
        txt_color(0, 0, HP_NUM_X + 3, 1, ((UINT32)sv.hp * 4UL < lvmax) ? TXT_RED : TXT_WHITE);   /* low HP: red */
        l_hp = sv.hp;
    }
    if(lv_changed){
        char *d = scat(b, "LV "); u16s(d, sv.level); txt_line(LV_X, 0, b, 5);
    }
    if(sv.mp != l_mp || lv_changed){
        txt_bar(MP_BAR_X, 1, BAR_W, sv.mp, hud_mpmax(sv.level));
        u16s(b, sv.mp); txt_line(MP_NUM_X, 1, b, 3);
        l_mp = sv.mp;
    }
    l_lv = sv.level;
    if(sv.gold != l_gold){
        char g[12]; UINT32 v = sv.gold > GOLD_MAX ? GOLD_MAX : sv.gold;
        u32s(g, v); txt_line(GOLD_X, 1, g, 9);
        l_gold = sv.gold;
    }
    txt_win(0);
}

void gain_xp(UINT16 n){
    char *b = WRAM_AT(WRAM_FADE + 160, char), *d; UINT16 need;   /* (not on the stack: it overflowed into cur_slot) */
    { static const UINT8 xm[3] = {1, 2, 5}; n *= xm[opt.xpm]; }               /* OPTIONS > XP SPEED: x1 / x2 / x5 */
    if(!opt.diff) n += n;                                                     /* BABY doubles */
    sv.xp += n;
    while(sv.level < 99 && sv.xp >= (need = sv.level * 12)){
        sv.xp -= need; sv.level++; sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level);
        sfx(SFX_LEVEL); d = scat(b, "LEVEL UP! YOU ARE LEVEL "); d = u16s(d, sv.level); d = scat(d, (sv.level % 5) ? ". +1 SKILL POINT! " : ". +3 SKILL POINTS! "); scat(d, "SPEND IT IN SYSTEM > SKILLS."); dlg_say(0, b);
    }
}

INT8 shake_x, shake_y; static UINT8 shake_t;
void shake(UINT8 n){ if(opt.shake && n > shake_t) shake_t = n; }
void shake_tick(void){
    if(shake_t){ shake_t--; shake_x = (shake_t & 1) ? 2 : -2; shake_y = (shake_t & 2) ? 1 : -1; if(shake_t < 4){ shake_x >>= 1; shake_y = 0; } }
    else shake_x = shake_y = 0;
}
