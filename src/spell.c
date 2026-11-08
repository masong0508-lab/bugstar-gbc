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
#include "wild.h"
#include "warship.h"
#include "kart.h"

/* ------------------------------------------------------------ spells: SPARK, FIREBALL, HEAL, SHIELD, PORTAL */
#define NSP 10
static const char *const sp_nm[NSP] = { "SPARK", "FIREBALL", "HEAL", "SHIELD", "PORTAL", "NOVA", "DRAIN", "QUAKE", "LIGHTNING", "STARFALL" };
static const UINT8 sp_cost[NSP] = { 2, 4, 6, 5, 7, 8, 7, 12, 10, 20 };
static const UINT8 sp_lv[NSP]   = { 1, 3, 5, 8, 10, 12, 15, 18, 22, 35 };
UINT8 spell_known(UINT8 s) BANKED { return s < NSP && sv.level >= sp_lv[s]; }
UINT8 spell_cost(UINT8 s) BANKED { return sp_cost[s % NSP]; }
UINT8 spell_next(UINT8 s) BANKED { UINT8 i, n; for(i = 1; i <= NSP; i++){ n = (s + i) % NSP; if(spell_known(n)) return n; } return s % NSP; }
void  spell_label(UINT8 s, char *out) BANKED {
    char *d = scat(out, sp_nm[s % NSP]); d = scat(d, "  "); d = u16s(d, sp_cost[s % NSP]); scat(d, " MP");
}
/* PORTAL: the door home. The floor you cast it on is kept in the WILD block; the next door down on that world opens on it. */
void portal_set(UINT8 fl) BANKED { wild_load(); wram_bank(2); XAP->p_star = sv.star; XAP->p_floor = fl; wram_bank(1); wild_save(); }
UINT8 portal_take(void) BANKED {
    UINT8 f = 0;
    wild_load(); wram_bank(2);
    if(XAP->p_floor && XAP->p_star == sv.star){ f = XAP->p_floor; XAP->p_floor = 0; }
    wram_bank(1);
    if(f) wild_save();
    return f;
}

UINT8 spell_pay(UINT8 s) BANKED {
    UINT16 mx = hud_hpmax(sv.level);
    if(sv.mp < sp_cost[s]){ sfx(SFX_HURT); dlg_say(0, "NOT ENOUGH MANA."); return 0; }
    sv.mp -= sp_cost[s]; if(EX_CASTS < 255) EX_CASTS++;
    if(s > 1 && s < 4) sfx(SFX_SPARK);
    if(s == 5 || s == 6) sfx(SFX_SPARK); else if(s == 7) sfx(SFX_BOOM); else if(s == 8) sfx(SFX_LASER); else if(s == 9) sfx(SFX_FIRE);
    if(s == 2){ sv.hp += mx / 2; if(sv.hp > mx) sv.hp = mx; }
    return 1;
}
UINT8 spell_dmg(UINT8 s, UINT8 face, INT16 dx, INT16 dy) BANKED {
    INT16 ah, la;
    if(s == 0) return (dx > -44 && dx < 44 && dy > -44 && dy < 44) ? CLAMP8(tr_spell(3 + sv.level / 2)) : 0;
    if(s >= 5){                                                  /* the later spells (version 6): areas, and a long bolt */
        INT16 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy; UINT16 p;
        if(s == 5) p = (ax < 56 && ay < 56) ? 6 + sv.level : 0;                 /* NOVA: a burst all round you */
        else if(s == 6) p = (ax < 64 && ay < 64) ? 4 + sv.level / 2 : 0;       /* DRAIN: what it takes, it gives back */
        else if(s == 7) p = (ax < 112 && ay < 88) ? 3 + sv.level / 2 : 0;      /* QUAKE: the whole screen */
        else if(s == 9) p = (ax < 120 && ay < 96) ? 10 + sv.level : 0;         /* STARFALL */
        else { ah = face == 0 ? dx : face == 1 ? -dx : face == 2 ? -dy : dy; la = face < 2 ? dy : dx; if(la < 0) la = -la; p = (ah > 0 && ah < 150 && la < 14) ? 8 + sv.level * 3 / 2 : 0; }   /* LIGHTNING */
        if(!p) return 0;
        p = tr_spell(p);
        if(s == 6){ UINT16 mx = hud_hpmax(sv.level); sv.hp += p / 2; if(sv.hp > mx) sv.hp = mx; }
        return CLAMP8(p);
    }
    ah = face == 0 ? dx : face == 1 ? -dx : face == 2 ? -dy : dy; la = face < 2 ? dy : dx; if(la < 0) la = -la;
    return (ah > 0 && ah < 96 && la < 20) ? CLAMP8(tr_spell(4 + sv.level)) : 0;
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
void hell_set(UINT8 depth, UINT8 moon) BANKED { hell_t = moon ? 1 + depth / 5 : 1 + depth / 4; if(hell_t > (moon ? 8 : 10)) hell_t = moon ? 8 : 10; }
UINT16 hell_hp(UINT16 hp) BANKED { return hp + hp * hell_t / 2 + hell_t * 3; }
void hell_end(UINT8 r, UINT8 depth, UINT8 moon) BANKED {
    char b[40], *d; UINT16 g = (UINT16)depth * depth * (moon ? 6 : 4);
    hell_t = 0;
    if(moon){ if(depth > EX_MOON) EX_MOON = depth; } else if(depth > EX_HELL) EX_HELL = depth;
    if(r == DG_FAINT) g /= 2;
    sv.gold += g;
    d = scat(b, "DEPTH "); d = u16s(d, depth); d = scat(d, ". +"); d = u16s(d, g); d = scat(d, "G. BEST "); d = u16s(d, moon ? EX_MOON : EX_HELL); *d = 0;
    dlg_say(0, b);
}

/* ------------------------------------------------------------ floor events */
extern UINT8 dg_depth;
#define depth dg_depth
static const char *const ev_tx[13] = { "", "TREASURE VAULT! CHESTS IN EVERY ROOM.", "GOLD RUSH! FOES PAY TRIPLE.", "AMBUSH! EXTRA MONSTERS HERE.",
    "BLESSED SHRINE! YOU FEEL HEALED.", "CURSED FLOOR! TOUGH FOES, DOUBLE GOLD.", "QUIET FLOOR. FEW MONSTERS.", "A LABYRINTH! FIND THE STAIRS.",
    "A PEDDLER LEFT A POTION HERE.", "A SPIRIT FILLS YOUR MANA.", "OLD CAMPFIRE. YOU REST A BIT.", "LUCKY COINS ON THE FLOOR!", "AN OLD SCROLL TEACHES YOU." };
UINT8 ev_roll(UINT8 depth) BANKED {
    UINT32 h = world_seed(sv.star) ^ ((UINT32)depth * 2654435761UL) ^ ((UINT32)sv.zone << 20);
    h ^= h >> 15; h *= 2246822519UL; h ^= h >> 13;
    if((h & 3) == 0) return 0;                       /* 1 floor in 4 has none */
    { UINT8 k = (UINT8)((h >> 8) % 11); return k < 6 ? 1 + k : 2 + k; }          /* 1..6, then 8..12 (7 is the labyrinth) */
}
void ev_name(UINT8 e, char *out) BANKED { scat(out, ev_tx[e > 12 ? 0 : e]); }
void floor_intro(UINT8 e) BANKED {
    if(e){ char b[40]; b[0] = 0; ev_name(e, b); if(e == 4){ sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level); }
      else if(e == 8){ if(sv.potions < 9) sv.potions++; }
      else if(e == 9) sv.mp = hud_mpmax(sv.level);
      else if(e == 10){ UINT16 m = hud_hpmax(sv.level); sv.hp += m / 3; if(sv.hp > m) sv.hp = m; }
      else if(e == 11){ sv.gold += LUCKY(20 + 10 * (UINT16)depth); }
      else if(e == 12){ gain_xp(10 + 6 * (UINT16)depth); }
      dlg_say(0, b); }
    if(!(EX_SPELL & 0x80)){ EX_SPELL |= 0x80; dlg_say(0, "SELECT CASTS YOUR SPELL. A+SELECT SWITCHES SPELL. B+SELECT DRINKS A POTION."); }
}

/* ------------------------------------------------------------ feats */
#define NFEAT 48
static const char *const ft_nm[NFEAT] = { "SLAYER", "ANGLER", "EXPLORER", "DEEP DIVER", "BRAVE", "HERO", "FRIEND", "CITIZEN",
    "PIT FIGHTER", "QUEEN BANE", "RICH BUG", "MAGE", "BOLT'S MIND", "JUMPER", "WORKER", "FAMOUS",
    "MASTER", "LEGEND", "MAX LEVEL", "SLAUGHTER", "MASSACRE", "ABYSS", "BOTTOMLESS", "ACE PILOT",
    "WAR PILOT", "HOPPER", "TRADER", "TYCOON", "STAR MAP", "COSMOS", "WORLD TOUR", "FISHER KING",
    "BREAKER", "ACE RACER", "BOARD SHARK", "AMBUSHED", "PIT KING", "FRIENDLY", "CASTER", "SAGE",
    "ALL CORES", "IDOL", "LEGENDARY", "GO-GETTER", "GOLD FISH", "VETERAN", "GRINDER", "COMPLETE" };
static const UINT16 ft_goal[NFEAT] = { 50, 10, 6, 6, 10, 20, 3, 3, 10, 3, 50, 25, 4, 20, 5, 100,
    100, 500, 999, 500, 999, 20, 40, 50,
    300, 100, 100, 100, 40, 100, 12, 50,
    3, 10, 5, 3, 30, 6, 200, 12,
    8, 300, 999, 25, 3, 60, 600, 47 };   /* (RICH BUG counts hundreds of gold: 50 = 5000; TRADER 100s of gold sold; MILLIONAIRE 10000s) */
static UINT8 pop(UINT8 *a, UINT8 n){ UINT8 i, c = 0, b; for(i = 0; i < n; i++) for(b = a[i]; b; b &= b - 1) c++; return c; }
static UINT8 ft_done(UINT8 i){ return i < 16 ? (&EX_FEAT)[i >> 3] & (1 << (i & 7)) : ex.xs[(i - 16) >> 3] & (1 << (i & 7)); }
static UINT16 ft_val(UINT8 i){
    switch(i){
        case 0: return EX_KILLS;     case 1: return EX_FISHN;      case 2: return pop(ex.seen, 32);  case 3: return sv.deepest;
        case 4: return sv.level;     case 5: return sv.level;      case 6: return sv.friends;        case 7: return pop(ex.city, 32);
        case 8: return ex.pit;       case 9: return pop((UINT8 *)&ex.tricks, 2);
        case 10: return sv.gold > 5000 ? 50 : (UINT16)sv.gold / 100;
        case 11: return EX_CASTS;    case 12: return sv.cores;     case 13: return sv.el.jumps;      case 14: return sv.el.jobs;
        case 15: return sv.el.fame;
        case 16: case 17: case 18: return sv.level;
        case 19: case 20: return EX_KILLS;
        case 21: case 22: return sv.deepest;
        case 23: case 24: return sv.el.kills;
        case 25: return sv.el.jumps;
        case 26: return sv.el.sold > 10000UL ? 100 : (UINT16)(sv.el.sold / 100);
        case 27: return sv.gold > 1000000UL ? 100 : (UINT16)(sv.gold / 10000UL);
        case 28: case 29: return pop(ex.seen, 32);
        case 30: return pop(ex.city, 32);
        case 31: return EX_FISHN;
        case 32: return warship_count(1);
        case 33: return kart_count(1);
        case 34: return warship_count(2);
        case 35: return warship_count(3);
        case 36: return ex.pit;
        case 37: return sv.friends;
        case 38: case 39: return EX_CASTS;
        case 40: return sv.cores;
        case 41: case 42: return sv.el.fame;
        case 43: return sv.el.jobs;
        case 44: return ex.rare;
        case 45: case 46: { UINT32 m = play_secs() / 60; return m > 999 ? 999 : (UINT16)m; }
        default: { UINT8 n = 0, k; for(k = 0; k < 47; k++) if(ft_done(k)) n++; return n; }     /* COMPLETE: every other feat */
    }
}
void feats_check(void) BANKED {
    UINT8 i;
    bounty_tick();                                              /* the bounty office pays a finished job the moment anything moves */
    for(i = 0; i < NFEAT; i++){
        if(ft_done(i) || ft_val(i) < ft_goal[i]) continue;
        if(i < 16) (&EX_FEAT)[i >> 3] |= 1 << (i & 7); else ex.xs[(i - 16) >> 3] |= 1 << (i & 7);
        { UINT16 g = 100 + 50 * (UINT16)i; char b[48], *d = scat(b, "FEAT: "); d = scat(d, ft_nm[i]); d = scat(d, "! +"); d = u16s(d, g); scat(d, " GOLD, +5 FAME");
          sv.gold += g; sv.el.fame += 5; sfx(SFX_FANFARE); dlg_say(0, b); }
    }
}
static void feats_page(UINT8 p){
    UINT8 i, n = 0, k; char b[24], *d;
    txt_cls();
    d = scat(b, "FEATS  PAGE "); *d++ = (char)(49 + p); d = scat(d, "/3  < >"); *d = 0; txt_line(1, 0, b, 18); txt_color(1, 0, 18, 1, TXT_GOLD);
    for(i = 0; i < 16; i++){
        UINT8 f = p * 16 + i; UINT8 L, dn = ft_done(f); char v[12], *e;
        e = u16s(v, ft_val(f) > ft_goal[f] ? ft_goal[f] : ft_val(f)); e = scat(e, "/"); e = u16s(e, ft_goal[f]);
        d = scat(b, dn ? "*" : " "); d = scat(d, ft_nm[f]); L = (UINT8)(d - b);
        while(L + (e - v) < 18){ *d++ = ' '; L++; } *d = 0; scat(d, v);
        txt_line(1, 1 + i, b, 18); txt_color(1, 1 + i, 18, 1, dn ? TXT_GOLD : TXT_WHITE);
    }
    for(k = 0; k < NFEAT; k++) if(ft_done(k)) n++;
    d = u16s(b, n); d = scat(d, "/48 DONE  B:BACK"); txt_line(1, 17, b, 18); txt_color(1, 17, 18, 1, TXT_GREY);
}
void feats_run(void) BANKED {
    UINT8 p = 0;
    feats_check(); HIDE_WIN;
    feats_page(p);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HITR(J_RIGHT)){ p = p == 2 ? 0 : p + 1; sfx(SFX_MOVE); feats_page(p); }
        if(HITR(J_LEFT)){ p = p ? p - 1 : 2; sfx(SFX_MOVE); feats_page(p); }
        if(HIT(J_B) || HIT(J_A) || HIT(J_START)) break;
    }
}
