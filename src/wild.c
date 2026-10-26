#pragma bank 27
/* WILD, part 1: the BESTIARY. Each of the 6 kinds of world has its own 4 creatures and a queen (names from the GBA game's world rosters):
   they use the 4 dungeon shapes, but with that world's colours, health and bite, and every kill is counted. The count lives in a small
   block of the slot (SRAM offset 480), read into WRAM bank 2 whenever it is needed (wild_load) and written back (wild_save). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "wram.h"
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "wild.h"

void wild_load(void) BANKED {
    UINT8 *p = (UINT8 *)XAP, i;
    wram_bank(2);
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof(XA); i++) p[i] = *(volatile UINT8 *)(0xA000U + XA_OFF + i);
    DISABLE_RAM;
    if(XAP->seed != sv.seed){ memset(p, 0, sizeof(XA)); XAP->seed = sv.seed; }
    wram_bank(1);
}
void wild_save(void) BANKED {
    UINT8 *p = (UINT8 *)XAP, i;
    wram_bank(2);
    XAP->seed = sv.seed;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof(XA); i++) *(volatile UINT8 *)(0xA000U + XA_OFF + i) = p[i];
    DISABLE_RAM;
    wram_bank(1);
}

/* ---- the creatures: [world][0 spider, 1 beetle, 2 wasp, 3 slime, 4 queen] */
static const char *const nm[30] = {
    "EARWIG", "WEEVIL", "BUMBLEBEE", "SNAIL", "GARDEN QUEEN",
    "SOLIFUGE", "SCARAB", "ANTLION", "SANDWORM", "DUNE QUEEN",
    "FROSTMITE", "YETI", "ICICLE", "SNOWBALL", "FROST QUEEN",
    "TARANTULA", "BULLETANT", "MANTIS", "TREEFROG", "JUNGLE QUEEN",
    "CRAYFISH", "URCHIN", "PIRANHA", "JELLYFISH", "TIDE QUEEN",
    "ZOMBUG", "MUMMY", "BANSHEE", "WRAITH", "DUSK QUEEN" };
static const char *const wn[6] = { "GARDEN", "DESERT", "ICE", "JUNGLE", "OCEAN", "DUSK" };
/* health in eighths of the usual, and extra bite, for each of the 4 */
static const UINT8 hpm[24] = { 7, 9, 6, 11,   6, 14, 8, 12,   4, 14, 7, 10,   8, 9, 8, 7,   12, 11, 5, 8,   9, 13, 5, 10 };
static const UINT8 dm[24]  = { 0, 0, 0, 0,    1, 0, 1, 0,     0, 1, 1, 0,     1, 1, 1, 0,   1, 1, 1, 0,    0, 1, 2, 0 };
#define C3(a,b,c,d,e,f,g,h,i) RGB(0,0,0), RGB(a,b,c), RGB(d,e,f), RGB(g,h,i)
static const palette_color_t pal[6][16] = {
    { C3(4,2,6, 16,6,22, 28,20,31),  C3(2,6,4, 6,18,10, 18,30,16),  C3(6,4,2, 30,24,4, 28,30,31),  C3(2,8,12, 8,24,22, 24,31,30) },
    { C3(8,4,1, 22,14,4, 31,26,12),  C3(4,3,2, 14,12,10, 28,24,18), C3(10,3,0, 28,14,2, 31,26,10), C3(6,5,1, 20,18,6, 30,29,14) },
    { C3(2,4,10, 10,18,28, 26,30,31), C3(4,6,10, 16,22,28, 28,31,31), C3(6,8,12, 20,26,31, 31,31,31), C3(2,8,14, 8,22,31, 22,30,31) },
    { C3(1,6,2, 4,20,6, 18,31,12),   C3(8,2,6, 24,8,18, 31,22,28),  C3(10,6,0, 31,18,2, 31,28,10), C3(0,8,8, 2,26,18, 18,31,26) },
    { C3(10,2,2, 30,8,6, 31,22,18),  C3(4,2,8, 16,8,22, 26,20,31),  C3(2,8,10, 6,22,26, 22,31,31), C3(6,6,12, 22,22,31, 31,31,31) },
    { C3(6,2,8, 18,6,24, 31,16,31),  C3(6,5,3, 18,14,10, 28,24,18), C3(8,0,8, 24,2,24, 31,18,31),  C3(2,2,6, 10,10,22, 24,24,31) } };

void bst_pals(UINT8 wk) BANKED { set_sprite_palette(0, 4, pal[wk]); }
UINT8 bst_hp(UINT8 hp, UINT8 k, UINT8 wk) BANKED {
    UINT16 h = hp; UINT8 p = weird_pct(weird_of(sv.star), WP_FOE);
    if(k < 4) h = h * hpm[wk * 4 + k] / 8;
    h += h * p / 100;
    return h > 255 ? 255 : h ? (UINT8)h : 1;
}
UINT8 bst_dmg(UINT8 k, UINT8 wk) BANKED { return (k < 4 ? dm[wk * 4 + k] : 0) + weird_pct(weird_of(sv.star), WP_HIT); }
void bst_kill(UINT8 k, UINT8 wk, UINT8 depth) BANKED {
    UINT8 i = k == 4 ? 24 + wk : wk * 4 + k, p = weird_pct(weird_of(sv.star), WP_GOLD);
    wild_load();
    wram_bank(2); if(XAP->kills[i] < 255) XAP->kills[i]++; wram_bank(1);
    wild_save();
    if(p){ UINT16 g = (UINT16)(3 + k * 2 + depth) * p / 100; sv.gold += g ? g : 1; }
}
void bst_name(char *out, UINT8 wk, UINT8 k) BANKED { scat(out, nm[wk * 5 + k]); }

/* ---- the book */
static void bk_page(UINT8 pg, const UINT8 *kl){
    UINT8 i; char b[21], *d;
    DISPLAY_OFF; txt_win(0); txt_cls(); txt_box(0, 0, 20, 18);
    txt_line(2, 1, "BESTIARY", 8); txt_color(2, 1, 8, 1, TXT_GOLD);
    d = u16s(b, pg + 1); d = scat(d, "/7"); *d = 0; txt_line(15, 1, b, 3); txt_color(15, 1, 3, 1, TXT_GREY);
    if(pg < 6){
        UINT16 all = 0;
        txt_line(2, 3, wn[pg], 10); txt_color(2, 3, 10, 1, TXT_WHITE);
        for(i = 0; i < 5; i++){
            UINT8 n = kl[i < 4 ? pg * 4 + i : 24 + pg], y = 5 + i * 2;
            all += n;
            txt_line(2, y, n ? nm[pg * 5 + i] : "?????", 12); txt_color(2, y, 12, 1, n ? (i == 4 ? TXT_GOLD : TXT_WHITE) : TXT_GREY);
            d = u16s(b, n); *d = 0; txt_line(14, y, b, 4); txt_color(14, y, 4, 1, n ? TXT_WHITE : TXT_GREY);
        }
        d = scat(b, "KILLED HERE "); d = u16s(d, all); *d = 0; txt_line(2, 15, b, 16); txt_color(2, 15, 16, 1, TXT_GREY);
    } else {
        txt_line(2, 3, "WEIRD WORLDS", 12); txt_color(2, 3, 12, 1, TXT_WHITE);
        weird_screen(5);
    }
    txt_line(2, 16, "L/R PAGE  B: BACK", 16); txt_color(2, 16, 16, 1, TXT_GREY);
    txt_pal_reset(); SHOW_BKG; DISPLAY_ON;
}
void bst_screen(void) BANKED {
    UINT8 kl[30], pg = 0, i;
    wild_load();
    wram_bank(2); for(i = 0; i < 30; i++) kl[i] = XAP->kills[i]; wram_bank(1);
    bk_page(pg, kl);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) return;
        if(HIT(J_RIGHT)){ pg = pg < 6 ? pg + 1 : 0; sfx(SFX_MOVE); bk_page(pg, kl); }
        if(HIT(J_LEFT)){ pg = pg ? pg - 1 : 6; sfx(SFX_MOVE); bk_page(pg, kl); }
    }
}
