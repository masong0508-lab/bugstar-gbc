#pragma bank 31
/* BUGSTAR GBC - WARSHIP RAID (see warship.h). One screen: the warship's hull is background tiles (guns and core are tiles too, so a hit just swaps a tile),
   your ship, your bolts, the enemy's orbs and the sparks are sprites. 8x8 sprites only: ship 4, bolts 4..7, orbs 8..15, sparks 16..19. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "fx.h"
#include "econ.h"
#include "bug.h"
#include "warship.h"

extern volatile UINT8 snd_song;

/* ---- the records (own SRAM block, offset 3584 of the slot's bank) */
typedef struct { UINT32 magic, seed; UINT8 clr, rot; UINT16 best[3]; UINT16 wins, tries, bwin, amb; UINT8 bd, rsv; UINT32 sum; } WS;   /* rot: which board; bd: its missions done (bits); bwin: board missions won; amb: ambushes survived */
#define WS_OFF   3584U
#define WS_MAGIC 0x32535742UL                                           /* "BWS2" */
static WS ws;
static void ws_save(void){
    const UINT8 *p = (const UINT8 *)&ws; UINT16 i;
    ws.magic = WS_MAGIC; ws.seed = sv.seed; ws.sum = fnv(&ws, sizeof ws - 4);
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof ws; i++) *(volatile UINT8 *)(0xA000U + WS_OFF + i) = p[i];
    DISABLE_RAM;
}
static void ws_load(void){
    UINT8 *p = (UINT8 *)&ws; UINT16 i;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof ws; i++) p[i] = *(volatile UINT8 *)(0xA000U + WS_OFF + i);
    DISABLE_RAM;
    if(ws.magic != WS_MAGIC || ws.seed != sv.seed || ws.sum != fnv(&ws, sizeof ws - 4) || (ws.clr & ~7) || (ws.bd & ~7)) memset(&ws, 0, sizeof ws);
}
UINT16 warship_count(UINT8 wins) BANKED { ws_load(); return wins == 3 ? ws.amb : wins == 2 ? ws.bwin : wins ? ws.wins : ws.tries; }
void warship_reset(void) BANKED { memset(&ws, 0, sizeof ws); ws_save(); }
UINT8 warship_info(char *out) BANKED {
    UINT8 n = 0, i; ws_load();
    for(i = 0; i < 3; i++) if(ws.clr & (1 << i)) n++;
    out[0] = 'W'; out[1] = 'O'; out[2] = 'N'; out[3] = ' '; out[4] = '0' + n; out[5] = '/'; out[6] = '3'; out[7] = 0;
    return n;
}

/* ---- the art: 8 rows of 8 digits (0..3 = palette colours), turned into 2bpp tiles when the mode starts */
enum { WT_SPACE = 128, WT_STAR, WT_PLATE, WT_RIVET, WT_SLOPE, WT_WINDOW, WT_TURC, WT_TURO, WT_TURD, WT_CORE, WT_SHA, WT_SHB, WT_BOLT, WT_ORB, WT_SPARK, WT_LAST };
static void art(UINT8 id, const char *r){
    UINT8 t[16], y, x, lo, hi, c;
    for(y = 0; y < 8; y++){
        lo = hi = 0;
        for(x = 0; x < 8; x++){ c = r[y * 8 + x] - '0'; lo = (lo << 1) | (c & 1); hi = (hi << 1) | (c >> 1); }
        t[y * 2] = lo; t[y * 2 + 1] = hi;
    }
    set_sprite_data(id, 1, t);                                           /* (sprite and background tiles 128..255 share the same video memory) */
}
static void tiles(void){
    art(WT_SPACE,  "00000000" "00000000" "00000000" "00000000" "00000000" "00000000" "00000000" "00000000");
    art(WT_STAR,   "00000000" "00000000" "00000000" "00002000" "00000000" "00000000" "00000000" "00000000");
    art(WT_PLATE,  "22222222" "11111113" "11111113" "11111113" "11111113" "11111113" "11111113" "33333333");
    art(WT_RIVET,  "22222222" "11111113" "11111113" "11122113" "11122113" "11111113" "11111113" "33333333");
    art(WT_SLOPE,  "33222222" "33211113" "33211113" "33211113" "33211113" "33211113" "33211113" "33333333");
    art(WT_WINDOW, "22222222" "10011001" "10011001" "10011001" "10011001" "11111111" "11111113" "33333333");
    art(WT_TURC,   "00111100" "01222210" "12233221" "12233221" "01222210" "00122100" "00033000" "00033000");
    art(WT_TURO,   "00333300" "03222230" "32233223" "32233223" "03222230" "00322300" "00333300" "00333300");
    art(WT_TURD,   "00000000" "00100100" "01000010" "00011000" "01000100" "00100010" "00001000" "00000000");
    art(WT_CORE,   "00001111" "00112222" "01222222" "01222333" "12223333" "12233333" "12233333" "12233333");
    art(WT_SHA,    "00000001" "00000012" "00000012" "00000133" "00000133" "00001233" "00011233" "00112222");
    art(WT_SHB,    "11122222" "12222222" "12222222" "12222233" "11222233" "01122211" "00000211" "00000033");
    art(WT_BOLT,   "00011000" "00033000" "00033000" "00033000" "00033000" "00033000" "00011000" "00000000");
    art(WT_ORB,    "00000000" "00122100" "01233210" "01233210" "01233210" "00122100" "00000000" "00000000");
    art(WT_SPARK,  "00000000" "00020000" "00232000" "02333200" "00232000" "00020000" "00000000" "00000000");
}
static const UINT16 bpal[3][4] = {
    { RGB(6, 8, 14), RGB(12, 15, 24), RGB(19, 22, 28), RGB(4, 5, 9) },       /* 4: the hull */
    { RGB(3, 3, 6), RGB(26, 6, 4), RGB(31, 18, 4), RGB(31, 29, 10) },        /* 5: guns and the glowing core */
    { RGB(1, 1, 4), RGB(8, 8, 14), RGB(14, 15, 22), RGB(31, 31, 31) } };     /* 6: space */
static const UINT16 spal[3][4] = {
    { 0, RGB(31, 31, 31), RGB(8, 22, 31), RGB(4, 8, 20) },                   /* 0: your ship (entry 2 becomes your shell colour) */
    { 0, RGB(31, 31, 10), RGB(10, 31, 10), RGB(31, 31, 31) },                /* 1: your bolts */
    { 0, RGB(31, 8, 8), RGB(31, 20, 6), RGB(31, 31, 20) } };                 /* 2: orbs and sparks */

/* ---- the warship */
#define PB_SPACE 6                                                       /* background palettes */
#define PB_HULL  4
#define PB_GUN   5
typedef struct { UINT8 x, y; } XY;
static const XY tpos[6] = { {6, 7}, {13, 7}, {3, 5}, {16, 5}, {5, 3}, {14, 3} };    /* the guns' tiles, in the order they are used (2, 4 or 6 of them) */
static const UINT8 hrow[7][2] = { {6, 13}, {4, 15}, {3, 16}, {2, 17}, {2, 17}, {3, 16}, {5, 14} };   /* the hull's first and last column on rows 2..8 */
#define CX 9                                                             /* the core: tiles (9,4)..(10,5) */
#define CY 4
static void cell(UINT8 x, UINT8 y, UINT8 t, UINT8 a){ VBK_REG = 1; set_bkg_tiles(x, y, 1, 1, &a); VBK_REG = 0; set_bkg_tiles(x, y, 1, 1, &t); }
static void core_cells(UINT8 t, UINT8 pal){
    cell(CX, CY, t, pal); cell(CX + 1, CY, t, pal | 0x20); cell(CX, CY + 1, t, pal | 0x40); cell(CX + 1, CY + 1, t, pal | 0x60);
}
static void draw_ship(UINT8 size, UINT8 ng){
    UINT8 r, c, t[20], a[20], i;
    for(r = 1; r < 18; r++){
        for(c = 0; c < 20; c++){
            t[c] = (((c * 7 + r * 13) % 11) == 0) ? WT_STAR : WT_SPACE; a[c] = PB_SPACE;
            if(r >= 2 && r <= 8 && c >= hrow[r - 2][0] && c <= hrow[r - 2][1]){
                a[c] = PB_HULL;
                if(c == hrow[r - 2][0]) t[c] = WT_SLOPE;
                else if(c == hrow[r - 2][1]){ t[c] = WT_SLOPE; a[c] = PB_HULL | 0x20; }
                else t[c] = ((c + r) % 5 == 0) ? WT_RIVET : WT_PLATE;
            }
            if(r == 9 && c >= 8 && c <= 11){ t[c] = WT_WINDOW; a[c] = PB_HULL; }
        }
        VBK_REG = 1; set_bkg_tiles(0, r, 20, 1, a); VBK_REG = 0; set_bkg_tiles(0, r, 20, 1, t);
    }
    core_cells(WT_CORE, PB_HULL);                                         /* the core starts shielded */
    for(i = 0; i < ng; i++) cell(tpos[i].x, tpos[i].y, WT_TURC, PB_GUN);
    (void)size;
}

/* ---- one raid */
#define NORB 8
#define NBOL 4
typedef struct { INT16 x, y, vx, vy; UINT8 life; } Orb;
static Orb orb[NORB];
static INT16 bx[NBOL], by[NBOL]; static UINT8 bon[NBOL];
static UINT8 thp[6], tcd[6], tfl[6];
static INT16 spx[4], spy[4], spvx[4], spvy[4]; static UINT8 spt;
static UINT16 rng2 = 0x1357;
static UINT8 rn(void){ rng2 ^= rng2 << 7; rng2 ^= rng2 >> 9; rng2 ^= rng2 << 8; return (UINT8)rng2; }
static INT16 sabs(INT16 v){ return v < 0 ? -v : v; }

static void fire(INT16 sx, INT16 sy, INT16 tx, INT16 ty, INT16 spd){      /* an orb from (sx, sy) toward (tx, ty), spd in 1/16 pixel a frame */
    UINT8 i; INT16 dx = tx - sx, dy = ty - sy, m = sabs(dx), n = sabs(dy);
    if(n > m) m = n; if(!m) m = 1;
    for(i = 0; i < NORB; i++) if(!orb[i].life){ orb[i].x = sx << 4; orb[i].y = sy << 4; orb[i].vx = (dx * spd) / m; orb[i].vy = (dy * spd) / m; orb[i].life = 1; return; }
}
static void burst(INT16 x, INT16 y){                                      /* four sparks fly out of a blast */
    UINT8 i; for(i = 0; i < 4; i++){ spx[i] = x << 4; spy[i] = y << 4; spvx[i] = (i & 1) ? 22 : -22; spvy[i] = (i & 2) ? 22 : -22; if(rn() & 1) spvx[i] += (INT8)(rn() & 7) - 3; }
    spt = 24;
}
static void hud_line(UINT16 tleft, UINT8 lives, UINT8 guns, UINT8 core){
    char b[21], *d = b; UINT8 s = (UINT8)(tleft / 60), i;
    *d++ = 'T'; d = u16s(d, s / 60); *d++ = ':'; if(s % 60 < 10) *d++ = '0'; d = u16s(d, s % 60); *d++ = ' ';
    for(i = 0; i < lives && i < 5; i++) *d++ = T_HEART;
    *d++ = ' ';
    if(core){ d = scat(d, "CORE"); } else { d = scat(d, "GUNS"); d = u16s(d, guns); }
    txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, tleft < 900 && (tleft & 32) ? TXT_RED : TXT_WHITE);
}

static UINT8 sec_tick(UINT16 t){ while(t >= 60) t -= 60; return !t; }      /* (no division in a per-frame test) */
static UINT16 tlim(UINT8 size, UINT8 mod){ UINT16 t = (size == 0 ? 75 : size == 1 ? 100 : 130); return mod == 1 ? t * 7 / 10 : t; }   /* seconds on the clock */
static UINT8 raid(UINT8 size, UINT8 mod, UINT16 *took){                              /* 1 won, 0 lost (the ship or the clock) */
    UINT8 ng = size == 0 ? 2 : size == 1 ? 4 : 6, i, j, gleft, lives, inv = 0, cool = 0, core_open = 0, cdone = 0, ccd = 0, spd, dmg, done = 0, res = 0, state = 0, oldsong = snd_song, shown = 0;
    INT16 px = 80, py = 120;
    UINT16 tleft = tlim(size, mod) * 60, el = 0, hs = 0xFFFF; UINT8 core_hp = 6 + 4 * size, hl = 255, hg = 255, hc = 255;
    UINT8 ghp = 4 + 2 * size + (mod == 4 ? 3 : 0), fcool = 14 - UPG(UP_GUNS); UINT8 tbase = 150 - 25 * size;
    if(fcool < 7) fcool = 7;
    if(mod == 3){ tbase = tbase * 65 / 100; if(tbase < 40) tbase = 40; }
    if(mod == 4) core_hp += 3;
    lives = mod == 2 ? 1 : 3 + UPG(UP_HULL) / 2 + (UPG(UP_SHIELD) ? 1 : 0);
#ifdef TEST_WAR_GOD
    lives = 200;
#endif
    dmg = 1 + UPG(UP_LASER) / 2;
#ifdef TEST_WAR_GOD
    dmg = 99;
#endif
    spd = 2 + UPG(UP_ENGINE) / 3;
    DISPLAY_OFF; HIDE_WIN; SCX_REG = 0; SCY_REG = 0;
    txt_cls(); tiles();
    set_bkg_palette(4, 3, bpal[0]); set_sprite_palette(0, 3, spal[0]); set_sprite_palette_entry(0, 2, sv.look.col[BC_SHELL]);
    draw_ship(size, ng);
    for(i = 0; i < ng; i++){ thp[i] = ghp; tcd[i] = tbase / 2 + (UINT8)(i * 17) + (rn() & 31); tfl[i] = 0; }
    for(i = 0; i < NORB; i++) orb[i].life = 0;
    for(i = 0; i < NBOL; i++) bon[i] = 0;
    spt = 0; gleft = ng; rng2 ^= (UINT16)DIV_REG << 8 | 1;
    HIDE_SPRITES; SPRITES_8x8; SHOW_BKG; SHOW_SPRITES;
    for(i = 0; i < 20; i++) move_sprite(i, 0, 0);
    for(i = 0; i < 4; i++) set_sprite_tile(i, i & 1 ? WT_SHB : WT_SHA);
    set_sprite_prop(0, 0); set_sprite_prop(1, 0); set_sprite_prop(2, 0x20); set_sprite_prop(3, 0x20);
    for(i = 4; i < 8; i++){ set_sprite_tile(i, WT_BOLT); set_sprite_prop(i, 1); }
    for(i = 8; i < 16; i++){ set_sprite_tile(i, WT_ORB); set_sprite_prop(i, 2); }
    for(i = 16; i < 20; i++){ set_sprite_tile(i, WT_SPARK); set_sprite_prop(i, 2); }
    music(MUS_WAR);
    DISPLAY_ON;
    while(!done){
        wait_vbl_done(); input_poll();
        shake_tick(); SCX_REG = (UINT8)shake_x;
        /* the text (only when it changed) */
        { UINT8 sec = (UINT8)(tleft / 60);
          if(sec != hl || lives != hg || gleft != hc || (tleft < 900 && !(tleft & 31)) || !shown){ hl = sec; hg = lives; hc = gleft; hud_line(tleft, lives, gleft, core_open); shown = 1; } }
        if(state == 0 && tleft && tleft <= 600 && sec_tick(tleft)) sfx(SFX_COUNT);     /* the last ten seconds tick */
        if(state == 0){                                                   /* the fight */
            UINT8 k = keys;
            if(k & J_LEFT)  px -= spd;
            if(k & J_RIGHT) px += spd;
            if(k & J_UP)    py -= spd;
            if(k & J_DOWN)  py += spd;
            if(px < 10) px = 10; if(px > 150) px = 150; if(py < 90) py = 90; if(py > 134) py = 134;
            if(cool) cool--;
            if((k & J_A) && !cool){                                        /* fire */
                for(i = 0; i < NBOL; i++) if(!bon[i]){ bon[i] = 1; bx[i] = px; by[i] = py - 8; cool = fcool; sfx(SFX_LASER); break; }
            }
            for(i = 0; i < NBOL; i++) if(bon[i]){
                by[i] -= 5;
                if(by[i] < 8){ bon[i] = 0; continue; }
                for(j = 0; j < ng; j++) if(thp[j] && sabs(bx[i] - (tpos[j].x * 8 + 4)) < 7 && sabs(by[i] - (tpos[j].y * 8 + 4)) < 7){       /* a gun is hit */
                    bon[i] = 0;
                    if(thp[j] > dmg) { thp[j] -= dmg; sfx(SFX_HIT); tfl[j] = 4; }
                    else { thp[j] = 0; gleft--; cell(tpos[j].x, tpos[j].y, WT_TURD, PB_GUN); burst(tpos[j].x * 8 + 4, tpos[j].y * 8 + 4); sfx(SFX_BOOM); shake(8);
                           if(!gleft){ core_open = 1; core_cells(WT_CORE, PB_GUN); sfx(SFX_FANFARE); } }
                    break;
                }
                if(bon[i] && core_open && !cdone && bx[i] > CX * 8 - 2 && bx[i] < CX * 8 + 18 && by[i] > CY * 8 - 2 && by[i] < CY * 8 + 18){
                    bon[i] = 0;
                    if(core_hp > dmg){ core_hp -= dmg; sfx(SFX_HIT); shake(2); }
                    else { core_hp = 0; cdone = 1; core_cells(WT_TURD, PB_GUN); burst(CX * 8 + 8, CY * 8 + 8); sfx(SFX_BOOM); shake(30); state = 1; hs = 60; }
                }
            }
            if(!core_open){                                               /* the guns fire, aimed at you */
                for(i = 0; i < ng; i++) if(thp[i]){
                    if(tfl[i]) tfl[i]--;
                    if(tcd[i]) tcd[i]--;
                    else {
                        INT16 gx = tpos[i].x * 8 + 4, gy = tpos[i].y * 8 + 8;
                        cell(tpos[i].x, tpos[i].y, WT_TURO, PB_GUN); tfl[i] = 12;
                        fire(gx, gy, px, py, 22 + 3 * size); sfx(SFX_FIRE);
                        tcd[i] = tbase + (rn() & 31);
                    }
                    if(tfl[i] == 1) cell(tpos[i].x, tpos[i].y, WT_TURC, PB_GUN);
                }
            } else if(!cdone){                                            /* the core spits three at a time */
                if(ccd) ccd--;
                else {
                    fire(CX * 8 + 8, CY * 8 + 16, px - 36, py, 24 + 3 * size); fire(CX * 8 + 8, CY * 8 + 16, px, py, 24 + 3 * size); fire(CX * 8 + 8, CY * 8 + 16, px + 36, py, 24 + 3 * size);
                    sfx(SFX_FIRE); ccd = 70 - 8 * size;
                }
            }
            if(inv) inv--;
            for(i = 0; i < NORB; i++) if(orb[i].life){                      /* the orbs fly */
                orb[i].x += orb[i].vx; orb[i].y += orb[i].vy;
                { INT16 ox = orb[i].x >> 4, oy = orb[i].y >> 4;
                  if(ox < -4 || ox > 164 || oy < -4 || oy > 148){ orb[i].life = 0; continue; }
                  if(!inv && sabs(ox - px) < 6 && sabs(oy - py) < 6){
                      orb[i].life = 0; sfx(SFX_HURT); shake(10); inv = 70;
                      if(lives) lives--;
                      if(!lives){ state = 2; hs = 60; sfx(SFX_DIE); burst(px, py); }
                  } }
            }
            if(tleft) tleft--; el++;
            if(!tleft && state == 0){ state = 3; hs = 90; sfx(SFX_BOOM); shake(40); }     /* the planet-buster fires */
        } else {                                                          /* the ending: sparks fly a moment, then the box */
            for(i = 0; i < NORB; i++) orb[i].life = 0;
            if(!--hs){ res = state == 1; done = 1; }
            if(state == 1 && !(hs & 7)){ burst(CX * 8 + 8 + (INT8)(rn() & 15) - 8, CY * 8 + 8 + (INT8)(rn() & 15) - 8); sfx(SFX_BOOM); shake(6); }
        }
        /* the sprites */
        if(state == 2 || (inv & 4 && state == 0)){ for(i = 0; i < 4; i++) move_sprite(i, 0, 0); }
        else { move_sprite(0, px - 8 + 8, py - 8 + 16); move_sprite(1, px - 8 + 8, py + 16); move_sprite(2, px + 8, py - 8 + 16); move_sprite(3, px + 8, py + 16); }
        for(i = 0; i < NBOL; i++){ if(bon[i]) move_sprite(4 + i, bx[i] + 4, by[i] + 12); else move_sprite(4 + i, 0, 0); }
        for(i = 0; i < NORB; i++){ if(orb[i].life) move_sprite(8 + i, (orb[i].x >> 4) + 4, (orb[i].y >> 4) + 12); else move_sprite(8 + i, 0, 0); }
        if(spt){
            spt--;
            for(i = 0; i < 4; i++){ spx[i] += spvx[i]; spy[i] += spvy[i]; if(spt) move_sprite(16 + i, (spx[i] >> 4) + 4, (spy[i] >> 4) + 12); else move_sprite(16 + i, 0, 0); }
        }
    }
    *took = el;
    HIDE_SPRITES; for(i = 0; i < 20; i++) move_sprite(i, 0, 0);
    music(oldsong == MUS_NONE ? MUS_SPACE : oldsong);
    (void)hs; (void)core_hp;
    return res;
}

/* ---- the menu and the results */
static const char *const sname[3] = { "SMALL", "BIG", "COLOSSAL" };
static const UINT8 sguns[3] = { 2, 4, 6 };
static char *mmss(char *d, UINT16 f){ UINT16 s = f / 60; d = u16s(d, s / 60); *d++ = ':'; if(s % 60 < 10) *d++ = '0'; d = u16s(d, s % 60); *d = 0; return d; }
static UINT8 maxsize(void){ return (ws.clr & 2) ? 2 : (ws.clr & 1) ? 1 : 0; }
static void draw_menu(UINT8 sel, UINT8 again){
    UINT8 i, n; char b[21], *d;
    txt_win(0);
    txt_soft(); txt_box(0, 0, 20, 18);                           /* (a redraw is a diff: only the changed cells are written) */
    txt_line(4, 1, "WARSHIP RAID", 12); txt_color(4, 1, 12, 1, TXT_GOLD);
    for(i = 0; i < 4; i++){
        d = b; *d++ = i == sel ? T_CURSOR : ' '; d = scat(d, i == 3 ? "WAR BOARD" : sname[i]); *d = 0;
        txt_line(2, 3 + i * 2, b, 12);
        if(i && i < 3 && !(ws.clr & (1 << (i - 1)))){ txt_line(5, 4 + i * 2, "", 12); txt_line(13, 3 + i * 2, "LOCKED", 6); txt_color(2, 3 + i * 2, 16, 1, TXT_GREY); }
        else if(i == 3){
            for(n = 0; n < 3; n++) if(!(ws.bd & (1 << n))) break;
            d = u16s(b, ws.bwin); d = scat(d, " DONE  RND "); d = u16s(d, ws.rot + 1); *d = 0; txt_line(5, 4 + i * 2, b, 14); txt_color(5, 4 + i * 2, 14, 1, TXT_GREY);
            if(!(ws.clr & 1)){ txt_line(13, 3 + i * 2, "LOCKED", 6); txt_color(2, 3 + i * 2, 16, 1, TXT_GREY); }
            else txt_color(2, 3 + i * 2, 16, 1, i == sel ? TXT_GOLD : TXT_WHITE);
        } else {
            d = u16s(b, sguns[i]); d = scat(d, " GUNS"); *d = 0; txt_line(13, 3 + i * 2, b, 6);
            if(ws.best[i]){ d = scat(b, "BEST "); mmss(d, ws.best[i]); txt_line(5, 4 + i * 2, b, 12); txt_color(5, 4 + i * 2, 12, 1, TXT_GREY); }
            else if(ws.clr & (1 << i)){ txt_line(5, 4 + i * 2, "BEATEN", 12); txt_color(5, 4 + i * 2, 12, 1, TXT_GREY); }
            else txt_line(5, 4 + i * 2, "", 12);
            txt_color(2, 3 + i * 2, 16, 1, i == sel ? TXT_GOLD : TXT_WHITE);
        }
    }
    txt_line(2, 12, "SHOOT EVERY GUN,", 17); txt_line(2, 13, "THEN THE CORE,", 17); txt_line(2, 14, "BEFORE TIME ENDS!", 17);
    txt_color(2, 12, 17, 3, TXT_GREY);
    txt_line(2, 15, "D-PAD FLY  A FIRE", 17); txt_color(2, 15, 17, 1, TXT_WHITE);
    txt_line(2, 16, "A: GO     B: BACK", 17); txt_color(2, 16, 17, 1, TXT_GREY);
    (void)again;
}

static const char *const mname[5] = { "STANDARD", "RUSH", "IRON HULL", "FLURRY", "ARMOURED" };
static const UINT8 mpay[5] = { 0, 40, 80, 50, 50 };                       /* percent more pay */
static void results(UINT8 size, UINT8 mod, UINT8 kind, UINT8 won, UINT16 took){        /* kind 0 a raid, 1 a board mission (bit 1<<2 of bd is set by the caller), 2 an ambush */
    char b[21], *d; UINT32 gold = 0; UINT16 tl = 0; UINT8 first = !(ws.clr & (1 << size));
    ws.tries++;
    DISPLAY_OFF; HIDE_SPRITES; SCX_REG = 0; SCY_REG = 0;
    if(won){
        UINT16 base = size == 0 ? 120 : size == 1 ? 320 : 800; UINT8 secs = (UINT8)(tlim(size, mod) - took / 60);
        tl = secs; gold = base + (UINT16)secs * 3; gold = gold * (100 + mpay[mod] + (kind == 2 ? 50 : 0)) / 100; gold = (UINT32)tr_gold((UINT16)(gold > 60000UL ? 60000UL : gold)) * GMULT(); sv.gold += gold;
        ws.wins++; ws.clr |= 1 << size; if(!mod && kind != 2 && (!ws.best[size] || took < ws.best[size])) ws.best[size] = took;
        if(kind == 1) ws.bwin++; if(kind == 2) ws.amb++;
        gain_xp(20 + 40 * size + (kind ? 20 : 0)); fame_add(2 + 3 * size + (kind ? 1 : 0));
    }
    ws_save(); if(opt.autosave) slot_write(cur_slot);
    txt_win(0); txt_cls(); txt_box(0, 0, 20, 15);
    if(won){
        txt_line(3, 2, "WARSHIP DOWN!", 16); txt_color(3, 2, 16, 1, TXT_GOLD);
        d = scat(b, sname[size]); d = scat(d, " WARSHIP"); *d = 0; txt_line(3, 4, b, 16);
        d = scat(b, kind == 2 ? "AMBUSH BEATEN" : mname[mod]); *d = 0; txt_line(3, 5, b, 16); txt_color(3, 5, 16, 1, TXT_GREY);
        d = scat(b, "TIME  "); mmss(d, took); txt_line(3, 6, b, 16);
        d = scat(b, "SPARE "); d = u16s(d, tl); d = scat(d, " SECONDS"); *d = 0; txt_line(3, 7, b, 16);
        d = scat(b, "PAID  "); d = u16s(d, gold > 65000UL ? 65000U : (UINT16)gold); d = scat(d, " GOLD"); *d = 0; txt_line(3, 9, b, 16); txt_color(3, 9, 16, 1, TXT_GOLD);
        d = scat(b, "+"); d = u16s(d, 20 + 40 * size + (kind ? 20 : 0)); d = scat(d, " XP  +"); d = u16s(d, 2 + 3 * size + (kind ? 1 : 0)); d = scat(d, " FAME"); *d = 0; txt_line(3, 10, b, 16);
        if(size < 2 && first) { txt_line(3, 12, size ? "COLOSSAL IS OPEN" : "BIG IS OPEN!", 16); txt_color(3, 12, 16, 1, TXT_GOLD); }
        sfx(SFX_LEVEL);
    } else {
        txt_line(3, 2, "RAID FAILED", 16); txt_color(3, 2, 16, 1, TXT_RED);
        txt_line(3, 4, "THE GUNS OR THE", 16); txt_line(3, 5, "CLOCK WON.", 16);
        txt_line(3, 7, "BOLT TOWED YOU", 16); txt_line(3, 8, "OUT. NO HARM.", 16);
        txt_line(3, 10, "UPGRADE THE SHIP", 16); txt_line(3, 11, "AND TRY AGAIN.", 16);
        sfx(SFX_HURT);
    }
    txt_line(7, 14, "A: OK", 6); txt_color(7, 14, 6, 1, TXT_GREY);
    DISPLAY_ON;
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B) || HIT(J_START)) break; }
}

/* ---- the war board: three missions a round (a size, a modifier, a world to hover over); finish all three and a new round is posted */
static void mission(UINT8 i, UINT8 *size, UINT8 *mod, UINT8 *star){
    UINT32 k[3], h; UINT8 m = maxsize();
    k[0] = sv.seed; k[1] = ws.rot; k[2] = i; h = fnv(k, 12);
    *size = i < m ? i : m;
    *mod = i == 0 && !ws.rot ? 0 : (UINT8)((h >> 8) % 5);
    *star = (UINT8)h;
}
static void board_draw(UINT8 sel, const char *msg){
    UINT8 i, size, mod, star, n; char b[21], *d; UINT16 pay;
    txt_win(0); txt_soft(); txt_box(0, 0, 20, 18);
    d = scat(b, "WAR BOARD  RND "); d = u16s(d, ws.rot + 1); *d = 0; txt_line(2, 1, b, 17); txt_color(2, 1, 17, 1, TXT_GOLD);
    for(i = 0; i < 3; i++){
        mission(i, &size, &mod, &star);
        d = b; *d++ = i == sel ? T_CURSOR : ' '; d = scat(d, sname[size]); d = scat(d, " OVER"); *d = 0; txt_line(2, 3 + i * 4, b, 17);
        d = world_name(b, world_seed(star)); *d = 0; txt_line(4, 4 + i * 4, b, 15); txt_color(4, 4 + i * 4, 15, 1, TXT_GREY);
        pay = size == 0 ? 120 : size == 1 ? 320 : 800; pay = (UINT16)((UINT32)pay * (100 + mpay[mod]) / 100);
        d = scat(b, mname[mod]); d = scat(d, " "); d = u16s(d, pay); d = scat(d, "G"); *d = 0; txt_line(4, 5 + i * 4, b, 15);
        txt_color(2, 3 + i * 4, 17, 1, (ws.bd & (1 << i)) ? TXT_GREY : i == sel ? TXT_GOLD : TXT_WHITE);
        txt_color(4, 5 + i * 4, 15, 1, mod ? TXT_GOLD : TXT_GREY);
        if(ws.bd & (1 << i)) txt_line(13, 3 + i * 4, "DONE", 4);
    }
    n = ws.bwin > 99 ? 99 : (UINT8)ws.bwin;
    d = u16s(b, n); d = scat(d, " MISSIONS WON"); *d = 0; txt_line(2, 15, b, 17); txt_color(2, 15, 17, 1, TXT_GREY);
    txt_line(2, 16, msg ? msg : "A: GO     B: BACK", 17); txt_color(2, 16, 17, 1, msg ? TXT_RED : TXT_GREY);
}
static void board(void){
    UINT8 sel = 0, d = 1, size, mod, star, got; UINT16 took; const char *msg = 0;
    for(;;){
        if(d){ board_draw(sel, msg); DISPLAY_ON; d = 0; }
        wait_vbl_done(); input_poll();
        if(HITR(J_UP)){ sel = sel ? sel - 1 : 2; msg = 0; d = 1; sfx(SFX_MOVE); }
        if(HITR(J_DOWN)){ sel = sel < 2 ? sel + 1 : 0; msg = 0; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_B) || HIT(J_START)){ sfx(SFX_BACK); return; }
        if(HIT(J_A)){
            if(ws.bd & (1 << sel)){ msg = "ALREADY DONE"; d = 1; sfx(SFX_HURT); continue; }
            mission(sel, &size, &mod, &star);
            got = raid(size, mod, &took);
            if(got){ ws.bd |= 1 << sel; if(ws.bd == 7){ ws.bd = 0; ws.rot++; } }
            results(size, mod, 1, got, took);
            DISPLAY_OFF; HIDE_SPRITES; SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); d = 1; msg = 0;
        }
    }
}

/* ---- a warship drops out of the warp on top of you: called after a jump (about one in five, from level 6) */
UINT8 warship_ambush(void) BANKED {
    UINT8 was = LCDC_REG & LCDCF_OBJ16, size, old = snd_song, hit = 0, got; UINT16 took;
    if(!WAR_OPEN) return 0;
    ws_load();
    rng2 ^= (UINT16)DIV_REG << 8 | 1; rn();
#ifndef TEST_WAR_AMB
    if(sv.el.jumps % 5 != 0 && (rn() & 7)) return 0;
#endif                     /* (every fifth jump, and one other in eight) */
    size = maxsize(); if(size > 1 && (rn() & 1)) size--;
    DISPLAY_OFF; HIDE_WIN; HIDE_SPRITES; txt_pal_reset(); txt_win(0); txt_cls(); txt_soft(); txt_box(0, 0, 20, 18);
    music(MUS_ALERT);
    txt_line(3, 3, "ALERT! ALERT!", 14); txt_color(3, 3, 14, 1, TXT_RED);
    txt_line(3, 6, "A WARSHIP DROPS", 16); txt_line(3, 7, "OUT OF THE WARP", 16); txt_line(3, 8, "RIGHT ON TOP OF", 16); txt_line(3, 9, "YOU. ITS GUNS", 16); txt_line(3, 10, "ARE LOCKING ON!", 16);
    txt_line(3, 13, sname[size], 16); txt_color(3, 13, 16, 1, TXT_GOLD);
    txt_line(3, 15, "A: FIGHT", 16); txt_color(3, 15, 16, 1, TXT_GREY);
    DISPLAY_ON;
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_START)) break; }
    got = raid(size, 0, &took);
    results(size, 0, 2, got, took);
    (void)hit;
    music(old);
    DISPLAY_OFF; HIDE_SPRITES; if(was) SPRITES_8x16; else SPRITES_8x8;
    txt_win(0); VBK_REG = 0; txt_cls(); SCX_REG = 0; SCY_REG = 0;
    return 1;
}

void warship_run(void) BANKED {
    UINT8 was = LCDC_REG & LCDCF_OBJ16, sel = 0, d = 1, got;
    UINT16 took;
    DISPLAY_OFF; HIDE_WIN; HIDE_SPRITES; ws_load(); txt_pal_reset();
#ifdef TEST_WAR
    ws.clr |= 3;
#endif
    for(;;){
        if(d){ draw_menu(sel, d == 2); DISPLAY_ON; d = 0; }
        wait_vbl_done(); input_poll();
        if(HITR(J_UP)){ sel = sel ? sel - 1 : 3; d = 2; sfx(SFX_MOVE); }
        if(HITR(J_DOWN)){ sel = sel < 3 ? sel + 1 : 0; d = 2; sfx(SFX_MOVE); }
        if(HIT(J_B) || HIT(J_START)){ sfx(SFX_BACK); break; }
        if(HIT(J_A)){
            if(sel < 3 && sel && !(ws.clr & (1 << (sel - 1)))){ sfx(SFX_HURT); continue; }
            if(sel == 3){
                if(!(ws.clr & 1)){ sfx(SFX_HURT); continue; }
                board(); DISPLAY_OFF; HIDE_SPRITES; SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); d = 1; continue;
            }
            got = raid(sel, 0, &took);
            results(sel, 0, 0, got, took);
            DISPLAY_OFF; HIDE_SPRITES; SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); d = 1;
        }
    }
    DISPLAY_OFF; HIDE_SPRITES; if(was) SPRITES_8x16; else SPRITES_8x8;
    txt_win(0); VBK_REG = 0; txt_cls(); SCX_REG = 0; SCY_REG = 0;
}
