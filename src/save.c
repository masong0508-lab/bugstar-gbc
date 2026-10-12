/* BUGSTAR GBC - step 3: 3-slot battery save + options (port of the GBA save.inc, minus the 64-bit words) */
#include <gb/gb.h>
#include <string.h>
#include "save.h"

#define SAVE_MAGIC 0x43475542UL      /* "BUGC" */
#define SET_MAGIC  0x53544553UL      /* "SETS" */
#define SAVE_VER   1
#define SET_BANK   3
/* MBC5 cart RAM: 0x0A to 0x0000-0x1FFF enables it, the RAM bank number goes to 0x4000-0x5FFF.
   (tests/ overrides these to run the save logic on a PC) */
#ifndef SRAM
#define SRAM ((volatile UINT8 *)0xA000)
#define RAM_ON()    (*(volatile UINT8 *)0x0000 = 0x0A)
#define RAM_OFF()   (*(volatile UINT8 *)0x0000 = 0x00)
#define RAM_BANK(b) (*(volatile UINT8 *)0x4000 = (b))
#endif

Save  sv;
Opts  opt;
UINT8 cur_slot = 0, last_slot = 0;

typedef struct { UINT32 magic; UINT8 last; Opts o; UINT32 sum; } Settings;

static volatile UINT32 vbl32;
static UINT32 play_base, play_t0;

static void vbl_isr(void){ vbl32++; }
UINT32 frames32(void){ UINT32 v; CRITICAL { v = vbl32; } return v; }
UINT32 play_secs(void){ return play_base + (frames32() - play_t0) / 60; }
void   play_reset(UINT32 base){ play_base = base; play_t0 = frames32(); }

static UINT32 fnv(const void *d, UINT16 n){
    const UINT8 *p = (const UINT8 *)d; UINT32 h = 2166136261UL;
    while(n--){ h ^= *p++; h *= 16777619UL; }
    return h;
}
static void sram_read(UINT8 bank, void *d, UINT16 n){
    UINT8 *p = (UINT8 *)d; UINT16 i;
    RAM_ON(); RAM_BANK(bank);
    for(i = 0; i < n; i++) p[i] = SRAM[i];
    RAM_OFF();
}
static void sram_write(UINT8 bank, const void *s, UINT16 n){
    const UINT8 *p = (const UINT8 *)s; UINT16 i;
    RAM_ON(); RAM_BANK(bank);
    for(i = 0; i < n; i++) SRAM[i] = p[i];
    RAM_OFF();
}

static void opts_default(void){
    opt.music = 2; opt.sfx = 1; opt.autosave = 1; opt.shake = 1; opt.diff = 1;
    opt.baby = 0; opt.hunger = 1; opt.faint = 1; opt.xpm = 0; opt.goldm = 0;
    opt.cam = 0; opt.minimap = 1; opt.nums = 1; opt.hints = 1; opt.arrow = 1;
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
    if(opt.diff > 4) opt.diff = 4;
    if(opt.xpm > 2) opt.xpm = 2;
    if(opt.goldm > 2) opt.goldm = 2;
    opt.sfx &= 1; opt.autosave &= 1; opt.shake &= 1; opt.baby &= 1; opt.hunger &= 1; opt.faint &= 1;
    opt.cam &= 1; opt.minimap &= 1; opt.nums &= 1; opt.hints &= 1; opt.arrow &= 1;
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
    return out->magic == SAVE_MAGIC && out->ver == SAVE_VER && out->sum == fnv(out, sizeof(Save) - 4);
}
UINT8 slot_read(UINT8 slot){
    Save t;
    if(!slot_peek(slot, &t)) return 0;
    sv = t;
    return 1;
}
void slot_write(UINT8 slot){
    Save old;
    UINT16 n = slot_peek(slot, &old) ? old.saves : 0;
    sv.magic = SAVE_MAGIC; sv.ver = SAVE_VER;
    sv.secs = play_secs(); sv.saves = n + 1;
    sv.sum = fnv(&sv, sizeof(Save) - 4);
    sram_write(slot, &sv, sizeof(Save));
    if(last_slot != slot){ last_slot = slot; settings_save(); }
}
void slot_erase(UINT8 slot){
    Save z;
    memset(&z, 0, sizeof z);
    sram_write(slot, &z, sizeof z);
    if(last_slot == slot){ last_slot = first_good_slot() == 255 ? 0 : first_good_slot(); settings_save(); }
}
void new_game(UINT8 slot, UINT32 seed){
    memset(&sv, 0, sizeof sv);
    sv.seed = seed; sv.level = 1; sv.hp = 10; sv.mp = 5;
    strcpy(sv.name, "BUG");                    /* the bug creator (step 5) will set this */
    play_reset(0);
    slot_write(slot);
}
UINT8 first_good_slot(void){
    Save t; UINT8 i;
    if(slot_peek(last_slot, &t)) return last_slot;
    for(i = 0; i < NSLOT; i++) if(slot_peek(i, &t)) return i;
    return 255;
}
