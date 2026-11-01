/* BUGSTAR GBC - step 3: 3-slot battery save + options (port of the GBA save.inc, minus the 64-bit words) */
#include <gb/gb.h>
#include <string.h>
#include "save.h"

#define SET_MAGIC  0x53544553UL      /* "SETS" */
#define SET_BANK   3
/* MBC5 cart RAM: 0x0A to 0x0000-0x1FFF enables it, the RAM bank number goes to 0x4000-0x5FFF.
   (tests/ overrides these to run the save logic on a PC) */
#ifndef SRAM
#define SRAM ((volatile UINT8 *)0xA000)
#define RAM_ON()    ENABLE_RAM            /* (the rumble-aware versions from rumble.h: the motor bit rides on the RAM bank register) */
#define RAM_OFF()   DISABLE_RAM
#define RAM_BANK(b) SWITCH_RAM(b)
#endif

Save  sv;
Extra ex;
#define EX_OFF   256
#define EX_MAGIC 0x58454742UL      /* "BGEX" */
Opts  opt;
UINT8 cur_slot = 0, last_slot = 0;

typedef struct { UINT32 magic; UINT8 last; Opts o; UINT32 sum; } Settings;
const UINT8 hold_tab[6] = { 10, 20, 30, 45, 65, 99 };
const UINT8 tank_tab[6] = { 14, 20, 26, 34, 44, 60 };

static volatile UINT32 vbl32;
static UINT32 play_base, play_t0;

static void vbl_isr(void){ vbl32++; rumble_tick(); }
UINT32 frames32(void){ UINT32 v; CRITICAL { v = vbl32; } return v; }
UINT32 play_secs(void){ return play_base + (frames32() - play_t0) / 60; }
void   play_reset(UINT32 base){ play_base = base; play_t0 = frames32(); }

UINT32 fnv(const void *d, UINT16 n){
    const UINT8 *p = (const UINT8 *)d; UINT32 h = 2166136261UL;
    while(n--){ h ^= *p++; h *= 16777619UL; }
    return h;
}
void sram_read_at(UINT8 bank, UINT16 off, void *d, UINT16 n){
    UINT8 *p = (UINT8 *)d; UINT16 i;
    RAM_ON(); RAM_BANK(bank);
    for(i = 0; i < n; i++) p[i] = SRAM[off + i];
    RAM_OFF();
}
static void sram_write_at(UINT8 bank, UINT16 off, const void *s, UINT16 n){
    const UINT8 *p = (const UINT8 *)s; UINT16 i;
    RAM_ON(); RAM_BANK(bank);
    for(i = 0; i < n; i++) SRAM[off + i] = p[i];
    RAM_OFF();
}
#define sram_write(b, s, n) sram_write_at(b, 0, s, n)

static void opts_default(void){
    opt.music = 2; opt.sfx = 1; opt.autosave = 1; opt.shake = 1; opt.diff = 2;
    opt.baby = 2;   /* 2 = difficulty list has BABY first (older saves: baby on = BABY, else diff + 1) */ opt.hunger = 1; opt.faint = 1; opt.xpm = 0; opt.goldm = 0;
    opt.cam = RUM_DEFAULT; opt.minimap = 1; opt.nums = 1; opt.hints = 1; opt.arrow = 1;
}
void settings_save(void){
    Settings s;
    memset(&s, 0, sizeof s);
    s.magic = SET_MAGIC; s.last = last_slot; s.o = opt;
    s.sum = fnv(&s, sizeof s - 4);
    sram_write(SET_BANK, &s, sizeof s);
}
static void settings_load(void){
    Settings s;
    opts_default(); last_slot = 0;
    sram_read(SET_BANK, &s, sizeof s);
    if(s.magic != SET_MAGIC || s.sum != fnv(&s, sizeof s - 4) || s.last >= NSLOT) return;   /* first run: defaults */
    last_slot = s.last; opt = s.o;
    if(opt.music > 2) opt.music = 2;                 /* clamp anything odd */
    if(opt.baby != 2){ opt.diff = opt.baby ? 0 : (opt.diff > 4 ? 5 : opt.diff + 1); opt.baby = 2; }
    if(opt.diff > 5) opt.diff = 5;
    if(opt.xpm > 2) opt.xpm = 2;
    if(opt.goldm > 2) opt.goldm = 2;
    opt.sfx &= 1; opt.autosave &= 1; opt.shake &= 1; opt.hunger &= 1; opt.faint &= 1;
    if(RUM_LVL() > 5) opt.cam = (opt.cam & ~7) | 2; if(RUM_EV() > 2) opt.cam = (opt.cam & ~0x18) | 8; opt.cam &= 0x3F;
    opt.minimap &= 1; opt.nums &= 1; opt.hints &= 1; opt.arrow &= 0x0F; if(BUD_CODE() > 6) opt.arrow &= 1;
}

void save_init(void){
    add_VBL(vbl_isr);
    set_interrupts(VBL_IFLAG);
    play_reset(0);
    settings_load();
}

UINT8 slot_peek(UINT8 slot, Save *out){
    if(slot >= NSLOT) return 0;
    sram_read(slot, out, sizeof(Save));
    if(out->magic == SAVE_MAGIC && out->ver == SAVE_VER && out->sum == fnv(out, sizeof(Save) - 4)) return 1;
    return sv_old(slot, out);                                            /* versions 1-4 are converted in save_guard.c (bank 38) */
}
UINT8 slot_read(UINT8 slot){
    Save t;
    if(!slot_peek(slot, &t)) return 0;
    sv_sanitize(&t);
    sv = t;
    sram_read_at(slot, EX_OFF, &ex, sizeof ex);
    if(ex.magic != EX_MAGIC || ex.sum != fnv(&ex, sizeof ex - 4)) memset(&ex, 0, sizeof ex);
    return 1;
}
void slot_write(UINT8 slot){
    Save old;
    UINT16 n = slot_peek(slot, &old) ? old.saves : 0;
    sv.magic = SAVE_MAGIC; sv.ver = SAVE_VER;
    sv.secs = play_secs(); sv.saves = n + 1;
    sv.sum = fnv(&sv, sizeof(Save) - 4);
    sram_write(slot, &sv, sizeof(Save));
    ex.magic = EX_MAGIC; ex.sum = fnv(&ex, sizeof ex - 4);
    sram_write_at(slot, EX_OFF, &ex, sizeof ex);
    if(last_slot != slot){ last_slot = slot; settings_save(); }
}
void slot_erase(UINT8 slot){
    Save z;
    memset(&z, 0, sizeof z);
    sram_write(slot, &z, sizeof z);
    if(last_slot == slot){ last_slot = first_good_slot() == 255 ? 0 : first_good_slot(); settings_save(); }
}
void new_game(UINT8 slot, UINT32 seed, const BugLook *look, const char *name){
    memset(&sv, 0, sizeof sv); memset(&ex, 0, sizeof ex);
    sv.seed = seed; sv.level = 1; sv.hp = 10; sv.mp = 5; sv.el.fuel = tank_tab[0];
    strcpy(sv.name, name ? name : "BUG");
    if(look) sv.look = *look; else bug_default(&sv.look);
    bug_clamp(&sv.look);
    play_reset(0);
    slot_write(slot);
}
UINT8 first_good_slot(void){
    Save t; UINT8 i;
    if(slot_peek(last_slot, &t)) return last_slot;
    for(i = 0; i < NSLOT; i++) if(slot_peek(i, &t)) return i;
    return 255;
}

UINT32 world_seed(UINT8 star){ return star ? sv.seed ^ ((UINT32)star * 0x9E3779B1UL) : sv.seed; }
UINT8 world_freed(UINT8 star){ UINT8 i; for(i = 0; i < sv.cores && i < 8; i++) if(sv.freed[i] == star) return 1; return 0; }
