#pragma bank 21
#include <gb/gb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"
#include "menus.h"
#include "spell.h"
#include "dungeon.h"

/* ------------------------------------------------------------ spells: SPARK, FIREBALL, HEAL, SHIELD */
static const char *const sp_nm[4] = { "SPARK", "FIREBALL", "HEAL", "SHIELD" };
static const UINT8 sp_cost[4] = { 2, 4, 6, 5 };
static const UINT8 sp_lv[4]   = { 1, 3, 5, 8 };
UINT8 spell_known(UINT8 s) BANKED { return sv.level >= sp_lv[s & 3]; }
UINT8 spell_cost(UINT8 s) BANKED { return sp_cost[s & 3]; }
UINT8 spell_next(UINT8 s) BANKED { UINT8 i; for(i = 1; i <= 4; i++) if(spell_known(s + i)) return (s + i) & 3; return s & 3; }
void  spell_label(UINT8 s, char *out) BANKED {
    char *d = scat(out, sp_nm[s & 3]); d = scat(d, "  "); d = u16s(d, sp_cost[s & 3]); scat(d, " MP");
}

UINT8 spell_pay(UINT8 s) BANKED {
    UINT16 mx = hud_hpmax(sv.level);
    if(sv.mp < sp_cost[s]){ sfx(SFX_HURT); dlg_say(0, "NOT ENOUGH MANA."); return 0; }
    sv.mp -= sp_cost[s]; if(EX_CASTS < 255) EX_CASTS++;
    if(s > 1) sfx(SFX_SPARK);
    if(s == 2){ sv.hp += mx / 2; if(sv.hp > mx) sv.hp = mx; }
    return 1;
}
UINT8 spell_dmg(UINT8 s, UINT8 face, INT16 dx, INT16 dy) BANKED {
    INT16 ah, la;
    if(s == 0) return (dx > -44 && dx < 44 && dy > -44 && dy < 44) ? 3 + sv.level / 2 : 0;
    ah = face == 0 ? dx : face == 1 ? -dx : face == 2 ? -dy : dy; la = face < 2 ? dy : dx; if(la < 0) la = -la;
    return (ah > 0 && ah < 96 && la < 20) ? 4 + sv.level : 0;
}
/* ------------------------------------------------------------ difficulty: BABY EASY NORMAL HARD BRUTAL NIGHTMARE */
UINT8 hell_t;
static UINT8 base_dmg(UINT8 n){
    switch(opt.diff){
        case 0: return 1;
        case 1: return n > 1 ? n / 2 : 1;
        case 3: return n + n / 2;
        case 4: return n * 2;
        case 5: return n * 3;
        default: return n;
    }
}
UINT8 diff_dmg(UINT8 n) BANKED {
    UINT16 r = base_dmg(n);
    if(hell_t && opt.diff) r += r * hell_t / 4;                   /* the deeper, the harder they bite (BABY stays gentle) */
    return r > 255 ? 255 : (UINT8)r;
}
/* ------------------------------------------------------------ the HELL DEPTHS: an endless stair down, tougher every four floors */
void hell_set(UINT8 depth) BANKED { hell_t = 1 + depth / 4; if(hell_t > 10) hell_t = 10; }
UINT16 hell_hp(UINT16 hp) BANKED { return hp + hp * hell_t / 2 + hell_t * 3; }
void hell_end(UINT8 r, UINT8 depth) BANKED {
    char b[40], *d; UINT16 g = (UINT16)depth * depth * 4;
    hell_t = 0;
    if(depth > EX_HELL) EX_HELL = depth;
    if(r == DG_FAINT) g /= 2;
    sv.gold += g;
    d = scat(b, "DEPTH "); d = u16s(d, depth); d = scat(d, ". +"); d = u16s(d, g); d = scat(d, "G. BEST "); d = u16s(d, EX_HELL); *d = 0;
    dlg_say(0, b);
}

/* ------------------------------------------------------------ floor events */
static const char *const ev_tx[8] = { "", "TREASURE VAULT! CHESTS IN EVERY ROOM.", "GOLD RUSH! FOES PAY TRIPLE.", "AMBUSH! EXTRA MONSTERS HERE.",
    "BLESSED SHRINE! YOU FEEL HEALED.", "CURSED FLOOR! TOUGH FOES, DOUBLE GOLD.", "QUIET FLOOR. FEW MONSTERS.", "A LABYRINTH! FIND THE STAIRS." };
UINT8 ev_roll(UINT8 depth) BANKED {
    UINT32 h = world_seed(sv.star) ^ ((UINT32)depth * 2654435761UL) ^ ((UINT32)sv.zone << 20);
    h ^= h >> 15; h *= 2246822519UL; h ^= h >> 13;
    if((h & 3) == 0) return 0;                       /* 1 floor in 4 has none */
    return 1 + (UINT8)((h >> 8) % 6);
}
void ev_name(UINT8 e, char *out) BANKED { scat(out, ev_tx[e > 7 ? 0 : e]); }
void floor_intro(UINT8 e) BANKED {
    if(e){ char b[40]; b[0] = 0; ev_name(e, b); if(e == 4){ sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level); } dlg_say(0, b); }
    if(!(EX_SPELL & 0x80)){ EX_SPELL |= 0x80; dlg_say(0, "SELECT CASTS YOUR SPELL. A+SELECT SWITCHES SPELL. B+SELECT DRINKS A POTION."); }
}

/* ------------------------------------------------------------ feats */
#define NFEAT 16
static const char *const ft_nm[NFEAT] = { "SLAYER", "ANGLER", "EXPLORER", "DEEP DIVER", "BRAVE", "HERO", "FRIEND", "CITIZEN",
    "PIT FIGHTER", "QUEEN BANE", "RICH BUG", "MAGE", "BOLT'S MIND", "JUMPER", "WORKER", "FAMOUS" };
static const UINT16 ft_goal[NFEAT] = { 50, 10, 6, 6, 10, 20, 3, 3, 10, 3, 50, 25, 4, 20, 5, 100 };   /* (RICH BUG counts hundreds of gold: 50 = 5000) */
static UINT8 pop(UINT8 *a, UINT8 n){ UINT8 i, c = 0, b; for(i = 0; i < n; i++) for(b = a[i]; b; b &= b - 1) c++; return c; }
static UINT16 ft_val(UINT8 i){
    switch(i){
        case 0: return EX_KILLS;     case 1: return EX_FISHN;      case 2: return pop(ex.seen, 32);  case 3: return sv.deepest;
        case 4: return sv.level;     case 5: return sv.level;      case 6: return sv.friends;        case 7: return pop(ex.city, 32);
        case 8: return ex.pit;       case 9: return pop(&ex.tricks, 1);
        case 10: return sv.gold > 5000 ? 50 : (UINT16)sv.gold / 100;
        case 11: return EX_CASTS;    case 12: return sv.cores;     case 13: return sv.el.jumps;      case 14: return sv.el.jobs;
        default: return sv.el.fame;
    }
}
static UINT8 ft_done(UINT8 i){ return (&EX_FEAT)[i >> 3] & (1 << (i & 7)); }
void feats_check(void) BANKED {
    UINT8 i;
    for(i = 0; i < NFEAT; i++){
        if(ft_done(i) || ft_val(i) < ft_goal[i]) continue;
        (&EX_FEAT)[i >> 3] |= 1 << (i & 7);
        { UINT16 g = 100 + 50 * (UINT16)i; char b[48], *d = scat(b, "FEAT: "); d = scat(d, ft_nm[i]); d = scat(d, "! +"); d = u16s(d, g); scat(d, " GOLD, +5 FAME");
          sv.gold += g; sv.el.fame += 5; sfx(SFX_FANFARE); dlg_say(0, b); }
    }
}
void feats_run(void) BANKED {
    UINT8 i, n = 0;
    feats_check(); HIDE_WIN;
    txt_line(1, 0, "FEATS", 18); txt_color(1, 0, 18, 1, TXT_GOLD);
    for(i = 0; i < NFEAT; i++){
        char b[24], *d; UINT8 k = ft_done(i), L;
        d = scat(b, k ? "*" : " "); d = scat(d, ft_nm[i]); L = (UINT8)(d - b);
        while(L < 13){ *d++ = ' '; L++; } *d = 0;
        { UINT16 v = ft_val(i); if(v > ft_goal[i]) v = ft_goal[i]; d = u16s(d, v); d = scat(d, "/"); u16s(d, ft_goal[i]); }
        txt_line(1, 1 + i, b, 18); txt_color(1, 1 + i, 18, 1, k ? TXT_GOLD : TXT_WHITE);
        n += k ? 1 : 0;
    }
    { char b[24], *d = u16s(b, n); d = scat(d, "/16 DONE   B: BACK"); txt_line(1, 17, b, 18); txt_color(1, 17, 18, 1, TXT_GREY); }
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_B) || HIT(J_A) || HIT(J_START)) break; }
}
