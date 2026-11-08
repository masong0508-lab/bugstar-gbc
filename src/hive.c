#pragma bank 45
/* BUGSTAR GBC - BUG HIVE (see hive.h): entry, the colony's save block and the catch-up for the time spent away. The colony itself runs from WRAM bank 6 (hive_int.h);
   it is kept in the slot's SRAM bank at offset HV_OFF (state, then the 1 KB map; own magic and checksum). */
#include <gb/gb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "traits.h"
#include "hive_int.h"

extern volatile UINT8 snd_song;
#define HV_OFF   4608U                                                 /* 4608 + 640 + 1024 stays inside the 8 KB of the slot's bank (and clear of the mirror at 4096) */
#define HV_MAGIC 0x32564842UL                                          /* "BHV2" */

static UINT32 hv_sum(void){ return fnv(HV, sizeof(HS) - 4) ^ fnv(MAP, 1024); }
static void hv_save(void){
    HV->magic = HV_MAGIC; HV->seed = sv.seed; HV->sum = hv_sum();
    sram_write_at(cur_slot, HV_OFF, HV, sizeof(HS));
    sram_write_at(cur_slot, HV_OFF + sizeof(HS), MAP, 1024);
}
static UINT8 hv_load(void){
    sram_read_at(cur_slot, HV_OFF, HV, sizeof(HS));
    sram_read_at(cur_slot, HV_OFF + sizeof(HS), MAP, 1024);
    if(HV->magic != HV_MAGIC || HV->seed != sv.seed || HV->ver != HV_VER || HV->sum != hv_sum()) return 0;
    return 1;
}
void hive_save(void) BANKED { HV->last = play_secs(); hv_save(); }
UINT16 hive_info(char *out) BANKED {
    UINT8 b[32]; char *d;
    sram_read_at(cur_slot, HV_OFF, b, 32);
    if(*(UINT32 *)b != HV_MAGIC || b[8] != HV_VER){ out[0] = 0; scat(out, "NEW COLONY"); return 0; }
    d = u16s(out, b[28]); scat(d, " BUGS"); return b[28];
}
void hive_reset(void) BANKED { UINT8 z[8]; memset(z, 0, 8); sram_write_at(cur_slot, HV_OFF, z, 8); }
void hive_run(void) BANKED {
    UINT8 old = snd_song, sv6 = SVBK_REG, again;
    SVBK_REG = 6;
    DISPLAY_OFF; HIDE_WIN; HIDE_SPRITES; txt_pal_reset();
    do {
        again = 0;
        if(!hv_load()){
            UINT8 r;
#if defined(HT_RICH) || defined(HT_AWAY)
            r = HT_KIND | (HT_DIFF << 4);
#else
            DISPLAY_ON; r = hu_intro(); DISPLAY_OFF;
#endif
            if(r == 255){ SVBK_REG = sv6; DISPLAY_OFF; HIDE_SPRITES; txt_win(0); txt_cls(); SCX_REG = 0; SCY_REG = 0; WY_REG = 144; return; }
            hs_new(r & 15, r >> 4);
#ifdef HT_RICH
            hs_test_rich();
#endif
#ifdef HT_AWAY
            hs_away(HT_AWAY);
#endif
#ifdef HT_TYPES
            { UINT8 x; for(x = 0; x < C_N; x++){ MAP[CI(x + 2, 10)] = x; MAP[CI(x + 2, 11)] = (x >= C_FIRSTB ? x | 0x80 : x | 0x20); } }
#endif
            hv_save();
        } else {
            UINT32 now = play_secs();
            hs_recount();
            if(now > HV->last){ UINT32 n = (now - HV->last) / 6; hs_away(n > 200 ? 200 : (UINT16)n); }
            HV->last = now;
            HV->stat[S_VGEN] = 0; memset(VIS, 0, 1024); HV->stat[S_DQN] = 0; HV->stat[S_FULL] = 0;
        }
        music(MUS_CAMP);
        hm_run();
        if(HV->stat[S_DEF] == 2){ hive_reset(); memset(HV, 0, 8); again = 1; }
        else { HV->last = play_secs(); HV->stat[S_DEF] = 0; hv_save(); }
    } while(again);
    SVBK_REG = sv6; music(old);
    DISPLAY_OFF; HIDE_SPRITES; WY_REG = 144; txt_win(0); VBK_REG = 0; txt_cls(); SCX_REG = 0; SCY_REG = 0; txt_pal_reset();
}
