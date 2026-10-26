#pragma bank 2
#include <gb/gb.h>
#include <string.h>
#include "sound.h"
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
enum { OP_DIFF, OP_HUNGER, OP_FAINT, OP_XP, OP_GOLD, OP_CAM, OP_MUSIC, OP_SFX, OP_AUTO,
       OP_SHAKE, OP_MAP, OP_NUMS, OP_HINTS, OP_ARROW, NOPT };
static const char *const op_name[NOPT] = { "DIFFICULTY", "HUNGER", "FAINT LOSS", "XP SPEED", "GOLD",
    "CAMERA", "MUSIC", "SOUND FX", "AUTOSAVE", "SCREEN SHAKE", "MINIMAP", "DMG NUMBERS", "HINTS", "GUIDE ARROW" };
static const char *const op_help[NOPT] = {
    "BABY: 1 DAMAGE, NO LOSSES, 2X XP", "OFF: NOBODY EVER STARVES",
    "OFF: FAINTING COSTS NOTHING", "HOW FAST YOU LEVEL UP", "HOW MUCH GOLD THINGS PAY",
    "THE VIEW A NEW GAME USES", "THE CHIPTUNE TUNES", "BITES, HITS AND PICKUPS",
    "SAVES AT SHIP, DUNGEON, STATION", "THE JOLT WHEN YOU GET HURT", "MINIMAP IN THE DUNGEON",
    "NUMBERS THAT FLY OFF FOES", "LINES THAT SAY WHAT A DOES", "THE YELLOW ARROW ON PLANETS" };
static const char *const dif_n[6] = { "BABY", "EASY", "NORMAL", "HARD", "BRUTAL", "NIGHTMR" };
static const char *const mus_n[3] = { "OFF", "LOW", "FULL" };
static const char *const cam_n[2] = { "3RD", "1ST" };
static const char *const mul_n[3] = { "X1", "X2", "X5" };
static const char *const yn_n[2]  = { "OFF", "ON" };

static UINT8 *op_ptr(UINT8 i){
    switch(i){
        case OP_DIFF: return &opt.diff;   case OP_HUNGER: return &opt.hunger;
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
    return i == OP_DIFF ? 6 : (i == OP_MUSIC || i == OP_XP || i == OP_GOLD) ? 3 : 2;
}

#define OP_ROWS 10                       /* list rows 2..11 */
void options_screen(void) BANKED {
    UINT8 sel = 0, top = 0, dirty = 1, i, first = 1;
    for(;;){
        if(dirty){
            dirty = 0;
            if(first){ first = 0; txt_box(0, 1, 20, 12); txt_box(0, 13, 20, 4);
                put_line(7, 0, "OPTIONS", 7); txt_color(7, 0, 7, 1, TXT_GOLD);
                put_line(2, 17, "< > CHANGE B:BACK", 17); txt_color(2, 17, 18, 1, TXT_GREY); }
            for(i = 0; i < OP_ROWS; i++){
                UINT8 o = top + i; char b[20]; UINT8 nl, vl; const char *v;
                if(o >= NOPT){ put_line(1, 2 + i, "", 18); continue; }
                v = op_val(o); nl = (UINT8)strlen(op_name[o]); vl = (UINT8)strlen(v);
                memset(b, ' ', 18); b[18] = 0;
                b[0] = (o == sel) ? T_CURSOR : ' ';
                memcpy(b + 1, op_name[o], nl); memcpy(b + 17 - vl, v, vl);
                put_line(1, 2 + i, b, 18);
                txt_color(1, 2 + i, 18, 1, o == sel ? TXT_GOLD : TXT_WHITE);
                txt_color(18 - vl, 2 + i, vl, 1, TXT_GOLD);
            }
            put_line(18, 2, top > 0 ? S_ARROW_UP : " ", 1); put_line(18, 11, top + OP_ROWS < NOPT ? S_ARROW_DN : " ", 1);
            put_line(1, 14, "", 18); put_line(1, 15, "", 18);
            print_wrap(14, 2, op_help[sel]); txt_color(1, 14, 18, 2, TXT_GREY);
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : NOPT - 1; dirty = 1; sfx(SFX_MOVE); }
        if(HIT(J_DOWN)) { sel = (sel + 1) % NOPT; dirty = 1; sfx(SFX_MOVE); }
        if(sel < top) top = sel;
        if(sel >= top + OP_ROWS) top = sel - OP_ROWS + 1;
        if(HIT(J_LEFT) || HIT(J_RIGHT)){
            UINT8 *p = op_ptr(sel), n = op_max(sel);
            *p = HIT(J_RIGHT) ? (*p + 1) % n : (*p + n - 1) % n; sfx(SFX_MOVE);
            dirty = 1;
        }
        if(HIT(J_B)){ settings_save(); sfx(SFX_BACK); return; }
    }
}

/* ------------------------------------------------------------ slot picker */
static UINT8 confirm(const char *l1){
    put_line(1, 15, l1, 18); put_line(1, 16, "A: YES   B: NO", 18);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_A)) { put_line(1, 15, "", 18); put_line(1, 16, "", 18); return 1; }
        if(HIT(J_B)) { put_line(1, 15, "", 18); put_line(1, 16, "", 18); return 0; }
    }
}
int slot_picker(UINT8 mode) BANKED {
    UINT8 sel = (mode == 0 && first_good_slot() != 255) ? first_good_slot() : 0, dirty = 1, i;
    Save t; UINT8 good[NSLOT];
    for(;;){
        if(dirty){
            dirty = 0;
            put_line(5, 0, mode ? "NEW GAME" : "LOAD GAME", 10); txt_color(5, 0, 10, 1, TXT_GOLD);
            put_line(1, 1, "PICK A SAVE SLOT", 18);
            for(i = 0; i < NSLOT; i++){                          /* a framed card per slot; the chosen one is gold */
                char b[32], *d; UINT8 y = 3 + i * 4;
                good[i] = slot_peek(i, &t);
                txt_box(0, y, 20, 4);
                d = b; *d++ = (i == sel) ? T_CURSOR : ' '; *d++ = '1' + i; *d++ = ' ';
                if(good[i]){ d = scat(d, t.name); d = scat(d, "  LV "); u16s(d, t.level); }
                else scat(d, "EMPTY");
                put_line(1, y + 1, b, 18);
                d = b; *d++ = ' '; *d++ = ' ';
                if(good[i]){ d = fmt_time(d, t.secs); d = scat(d, " "); d = u16s(d, t.cores); scat(d, "/8 CORES"); } else *d = 0;
                put_line(1, y + 2, b, 18);
                txt_color(1, y + 1, 18, 2, i == sel ? TXT_GOLD : TXT_WHITE);
            }
            put_line(1, 15, "A:OK  B:BACK", 18);
            put_line(1, 16, good[sel] ? "START: ERASE" : "", 18);
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : NSLOT - 1; dirty = 1; sfx(SFX_MOVE); }
        if(HIT(J_DOWN)) { sel = (sel + 1) % NSLOT; dirty = 1; sfx(SFX_MOVE); }
        if(HIT(J_B)){ sfx(SFX_BACK); return -1; }
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
