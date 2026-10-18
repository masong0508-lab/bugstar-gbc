#pragma bank 2
#include <gb/gb.h>
#include <string.h>
#include "input.h"
#include "text.h"
#include "save.h"
#include "menus.h"

/* word-wrap s into `rows` lines of 18 columns starting at row y */
static void print_wrap(UINT8 y, UINT8 rows, const char *s){
    UINT8 r, i, n, last;
    for(r = 0; r < rows; r++){
        while(*s == ' ') s++;
        last = 0;
        for(i = 0; s[i] && i < 18; i++) if(s[i] == ' ') last = i;
        if(!s[i] || s[i] == ' ') n = i; else n = last ? last : i;
        {
            char b[19]; UINT8 k;
            for(k = 0; k < n; k++) b[k] = s[k];
            b[n] = 0; put_line(1, y + r, b, 18);
        }
        s += n;
    }
}

/* ------------------------------------------------------------ options */
enum { OP_BABY, OP_DIFF, OP_HUNGER, OP_FAINT, OP_XP, OP_GOLD, OP_CAM, OP_MUSIC, OP_SFX, OP_AUTO,
       OP_SHAKE, OP_MAP, OP_NUMS, OP_HINTS, OP_ARROW, NOPT };
static const char *const op_name[NOPT] = { "BABY MODE", "DIFFICULTY", "HUNGER", "FAINT LOSS", "XP SPEED", "GOLD",
    "CAMERA", "MUSIC", "SOUND FX", "AUTOSAVE", "SCREEN SHAKE", "MINIMAP", "DMG NUMBERS", "HINTS", "GUIDE ARROW" };
static const char *const op_help[NOPT] = {
    "1 DAMAGE MAX, NO HUNGER, NOTHING LOST, 2X GOLD AND XP",
    "EASY: HALF DAMAGE, KEEP YOUR BAG. HARD: MORE DAMAGE, +25% GOLD",
    "OFF: NOBODY EVER STARVES",
    "OFF: FAINTING COSTS NO GOLD AND NO BAG",
    "HOW FAST YOU LEVEL UP",
    "HOW MUCH GOLD EVERYTHING PAYS",
    "WHICH VIEW A NEW GAME STARTS WITH",
    "THE CHIPTUNE TUNES",
    "BITES, HITS AND PICKUPS",
    "SAVES AT THE SHIP, AFTER DUNGEONS AND AT STATIONS",
    "THE JOLT WHEN YOU GET HURT",
    "THE LITTLE MAP IN THE DUNGEON CORNER",
    "NUMBERS THAT FLY OFF THINGS YOU BITE",
    "THE LINES THAT SAY WHAT A DOES",
    "THE YELLOW ARROW ON PLANETS" };
static const char *const dif_n[5] = { "EASY", "NORMAL", "HARD", "BRUTAL", "NIGHTMR" };
static const char *const mus_n[3] = { "OFF", "LOW", "FULL" };
static const char *const cam_n[2] = { "3RD", "1ST" };
static const char *const mul_n[3] = { "X1", "X2", "X5" };
static const char *const yn_n[2]  = { "OFF", "ON" };

static UINT8 *op_ptr(UINT8 i){
    switch(i){
        case OP_BABY: return &opt.baby;   case OP_DIFF: return &opt.diff;   case OP_HUNGER: return &opt.hunger;
        case OP_FAINT: return &opt.faint; case OP_XP: return &opt.xpm;      case OP_GOLD: return &opt.goldm;
        case OP_CAM: return &opt.cam;     case OP_MUSIC: return &opt.music; case OP_SFX: return &opt.sfx;
        case OP_AUTO: return &opt.autosave; case OP_SHAKE: return &opt.shake; case OP_MAP: return &opt.minimap;
        case OP_NUMS: return &opt.nums;   case OP_HINTS: return &opt.hints; default: return &opt.arrow;
    }
}
static const char *op_val(UINT8 i){
    UINT8 v = *op_ptr(i);
    switch(i){
        case OP_DIFF: return dif_n[v];
        case OP_MUSIC: return mus_n[v];
        case OP_CAM: return cam_n[v];
        case OP_XP: case OP_GOLD: return mul_n[v];
        default: return yn_n[v & 1];
    }
}
static UINT8 op_max(UINT8 i){            /* number of values */
    return i == OP_DIFF ? 5 : (i == OP_MUSIC || i == OP_XP || i == OP_GOLD) ? 3 : 2;
}

#define OP_ROWS 11                       /* list rows 2..12 */
void options_screen(void) BANKED {
    UINT8 sel = 0, top = 0, dirty = 1, i;
    for(;;){
        if(dirty){
            dirty = 0;
            put_line(1, 0, "OPTIONS", 18);
            for(i = 0; i < OP_ROWS; i++){
                UINT8 o = top + i; char b[20]; UINT8 nl, vl; const char *v;
                if(o >= NOPT){ put_line(0, 2 + i, "", 19); continue; }
                v = op_val(o); nl = (UINT8)strlen(op_name[o]); vl = (UINT8)strlen(v);
                memset(b, ' ', 19); b[19] = 0;
                b[0] = (o == sel) ? T_CURSOR : ' ';
                memcpy(b + 1, op_name[o], nl); memcpy(b + 19 - vl, v, vl);
                put_line(0, 2 + i, b, 19);
            }
            put_line(0, 1,  top > 0 ? "                  " S_ARROW_UP : "", 19);
            put_line(0, 13, top + OP_ROWS < NOPT ? "                  " S_ARROW_DN : "", 19);
            print_wrap(14, 3, op_help[sel]);
            put_line(1, 17, "LEFT/RIGHT  B:BACK", 18);
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : NOPT - 1; dirty = 1; }
        if(HIT(J_DOWN)) { sel = (sel + 1) % NOPT; dirty = 1; }
        if(sel < top) top = sel;
        if(sel >= top + OP_ROWS) top = sel - OP_ROWS + 1;
        if(HIT(J_LEFT) || HIT(J_RIGHT)){
            UINT8 *p = op_ptr(sel), n = op_max(sel);
            *p = HIT(J_RIGHT) ? (*p + 1) % n : (*p + n - 1) % n;
            dirty = 1;
        }
        if(HIT(J_B)){ settings_save(); return; }
    }
}

/* ------------------------------------------------------------ slot picker */
static UINT8 confirm(const char *l1){
    put_line(1, 13, l1, 18); put_line(1, 14, "A: YES   B: NO", 18);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_A)) { put_line(1, 13, "", 18); put_line(1, 14, "", 18); return 1; }
        if(HIT(J_B)) { put_line(1, 13, "", 18); put_line(1, 14, "", 18); return 0; }
    }
}
int slot_picker(UINT8 mode) BANKED {
    UINT8 sel = (mode == 0 && first_good_slot() != 255) ? first_good_slot() : 0, dirty = 1, i;
    Save t; UINT8 good[NSLOT];
    for(;;){
        if(dirty){
            dirty = 0;
            put_line(1, 0, mode ? "NEW GAME" : "LOAD GAME", 18);
            put_line(1, 1, "PICK A SAVE SLOT", 18);
            for(i = 0; i < NSLOT; i++){
                char b[20], *d; UINT8 y = 3 + i * 3;
                good[i] = slot_peek(i, &t);
                d = b; *d++ = (i == sel) ? T_CURSOR : ' '; *d++ = '1' + i; *d++ = ' ';
                if(good[i]){ d = scat(d, t.name); d = scat(d, " LV "); u16s(d, t.level); }
                else scat(d, "EMPTY");
                put_line(0, y, b, 19);
                d = b; d = scat(d, "   ");
                if(good[i]){ d = scat(d, "TIME "); fmt_time(d, t.secs); } else *d = 0;
                put_line(0, y + 1, b, 19);
            }
            put_line(1, 15, "A:OK  B:BACK", 18);
            put_line(1, 16, good[sel] ? "START: ERASE" : "", 18);
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : NSLOT - 1; dirty = 1; }
        if(HIT(J_DOWN)) { sel = (sel + 1) % NSLOT; dirty = 1; }
        if(HIT(J_B)) return -1;
        if(HIT(J_START) && good[sel]){
            char b[20], *d = b; d = scat(d, "ERASE SLOT "); *d++ = '1' + sel; *d++ = '?'; *d = 0;
            if(confirm(b)) slot_erase(sel);
            dirty = 1;
            continue;                        /* the A that confirmed must not also pick the slot */
        }
        if(HIT(J_A)){
            if(mode == 0){ if(good[sel]) return sel; }
            else if(!good[sel]) return sel;
            else {
                char b[20], *d = b; d = scat(d, "OVERWRITE SLOT "); *d++ = '1' + sel; *d++ = '?'; *d = 0;
                if(confirm(b)) return sel;
            }
        }
    }
}
