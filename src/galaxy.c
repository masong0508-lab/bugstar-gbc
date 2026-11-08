#pragma bank 17
/* BUGSTAR GBC - the galaxy chart (SELECT in space): 16 x 16 worlds, you are the @, freed worlds are gold stars, the job's destination
   is a red X. Move the cursor, A jumps there (it costs fuel: one unit a cell, the most cells in either direction). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "econ.h"
#include "wram.h"
#include "elite.h"

static char *pad(char *d, char *start, UINT8 n){ while((UINT8)(d - start) < n) *d++ = ' '; *d = 0; return d; }
#define MX 2
#define MY 2
#define BEAST_CELL 0x77                                  /* the Technobeast sleeps at the corner of cells 7,7 .. 8,8 */

/* star classes (bits 5..7 of the world's seed): the chart colours a star you have not visited by its class and says the class on the jump line */
static const char *const cls_n[8] = { "RED DWF", "SUN", "BLU GNT", "WHT DWF", "ORANGE", "BINARY", "PULSAR", "NEBULA" };
static const UINT8 cls_p[8] = { TXT_RED, TXT_GOLD, TXT_WHITE, TXT_WHITE, TXT_GOLD, TXT_GOLD, TXT_WHITE, TXT_GREY };
static UINT8 sclass(UINT8 s){ return (UINT8)((world_seed(s) >> 5) & 7); }
static UINT8 glyph_of(UINT8 s, UINT8 from){
    if(s == from) return '@';
    if(sv.el.jk == J_SEND && s == sv.el.jd) return 'X';
    if(world_freed(s)) return '*';
    if(ex.city[s >> 3] & (1 << (s & 7))) return '=';           /* landed and walked into a city */
    if(ex.seen[s >> 3] & (1 << (s & 7))) return ':';           /* landed */
    return '.';
}
static void draw_cell(UINT8 s, UINT8 from, UINT8 cursor, UINT8 blink){
    char c[2]; UINT8 g = glyph_of(s, from), pal = TXT_WHITE;
    c[1] = 0;
    if(g == '@') pal = TXT_WHITE; else if(g == 'X') pal = TXT_RED; else if(g == '*') pal = TXT_GOLD; else if(g == '=' || g == ':') pal = TXT_GREY; else if(g == '.') pal = cls_p[sclass(s)];
    if(cursor && blink){ g = '+'; pal = TXT_GOLD; }
    c[0] = (char)g;
    txt_put(MX + (s & 15), MY + (s >> 4), c); txt_color(MX + (s & 15), MY + (s >> 4), 1, 1, pal);
}
static void info(UINT8 cur, UINT8 from, UINT8 beast_sel, const char *msg){
    char b[24], *d; UINT8 cost, ok;
    if(beast_sel){
        txt_line(0, 0, "THE TECHNOBEAST", 20); txt_color(0, 0, 20, 1, TXT_RED);
        cost = jump_fuel(from, BEAST_CELL);
    } else {
        d = world_name(b, world_seed(cur)); d = scat(d, " "); d = scat(d, kind_name[world_kind(world_seed(cur))]); if(world_freed(cur)) d = scat(d, " *"); if(ex.city[cur >> 3] & (1 << (cur & 7))) d = scat(d, " CITY"); else if(ex.seen[cur >> 3] & (1 << (cur & 7))) d = scat(d, " SEEN"); pad(d, b, 20);
        txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_GOLD);
        cost = cur == from ? 0 : jump_fuel(from, cur);
    }
    ok = sv.el.fuel >= cost;
    d = scat(b, "JUMP "); d = u16s(d, cost); d = scat(d, "/"); d = u16s(d, sv.el.fuel); if(!beast_sel){ d = scat(d, " "); d = scat(d, cls_n[sclass(cur)]); } pad(d, b, 20);
    txt_line(0, 1, msg ? msg : b, 20); txt_color(0, 1, 20, 1, msg || !ok ? TXT_RED : TXT_WHITE);
}

UINT16 galaxy_run(UINT8 from, UINT8 beast) BANKED {
    UINT8 cur = from, s, blink = 0, t = 0, bsel = 0, cost; UINT16 out = GAL_NONE; const char *msg = 0;
    txt_win(1); txt_soft();
    for(s = 0;; s++){ draw_cell(s, from, 0, 0); if(s == 255) break; }
    info(cur, from, bsel, msg);
    txt_soft_end(); txt_win(0); move_win(7, 0); SHOW_WIN;
    for(;;){
        UINT8 old = cur, moved = 0;
        wait_vbl_done(); input_poll();
        if(++t & 16) { if(!blink){ blink = 1; moved = 1; } } else if(blink){ blink = 0; moved = 1; }
        if(HITR(J_LEFT))  { cur = (cur & 0xF0) | ((cur - 1) & 15); moved = 1; }
        if(HITR(J_RIGHT)) { cur = (cur & 0xF0) | ((cur + 1) & 15); moved = 1; }
        if(HITR(J_UP))    { cur = (UINT8)(cur - 16); moved = 1; }
        if(HITR(J_DOWN))  { cur = (UINT8)(cur + 16); moved = 1; }
        if(cur != old){ bsel = 0; msg = 0; sfx(SFX_MOVE); }
        if(HIT(J_SELECT)){                                          /* jump the cursor: the job's destination, the beast, you */
            msg = 0;
            if(sv.el.jk == J_SEND && cur != sv.el.jd && !bsel){ cur = sv.el.jd; }
            else if(beast && !bsel){ bsel = 1; }
            else { bsel = 0; cur = from; }
            sfx(SFX_MOVE); moved = 1;
        }
        if(HIT(J_B) || HIT(J_START)) break;
        if(HIT(J_A)){
            if(bsel){ cost = jump_fuel(from, BEAST_CELL); if(sv.el.fuel >= cost){ sv.el.fuel -= cost; sv.el.jumps++; out = GAL_BEAST; break; } msg = "NOT ENOUGH FUEL"; sfx(SFX_BACK); }
            else if(cur == from){ msg = "YOU ARE HERE"; sfx(SFX_BACK); }
            else { cost = jump_fuel(from, cur); if(sv.el.fuel >= cost){ sv.el.fuel -= cost; sv.el.jumps++; out = cur; break; } msg = "NOT ENOUGH FUEL"; sfx(SFX_BACK); }
            moved = 1;
        }
        if(moved){
            txt_win(1);
            draw_cell(old, from, 0, 0); draw_cell(cur, from, !bsel, blink);
            info(cur, from, bsel, msg);
            txt_win(0);
        }
    }
    sfx(SFX_OK);
    txt_win(1); txt_soft(); txt_win(0);
    return out;
}
