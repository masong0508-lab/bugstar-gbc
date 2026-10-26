#pragma bank 20
/* BUGSTAR GBC - the extras (see feat.h): the clock, pets, fishing, and the queen's words and tricks. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "dialog.h"
#include "menus.h"
#include "bugspr.h"
#include "fx.h"
#include "feat.h"

static UINT16 rs = 0xB5A3;
static UINT8 rnd(void){ rs ^= rs << 7; rs ^= rs >> 9; rs ^= rs << 8; return (UINT8)rs; }

/* ------------------------------------------------------------ the clock */
static UINT16 day_t(void){ return (UINT16)((play_secs() + 200 + (UINT32)ex.off * 20) % 480); }
UINT8 tod_hour(void) BANKED { return (UINT8)(day_t() / 20); }
UINT8 tod_dark(void) BANKED {                         /* dawn 100..159 (16 -> 0), day, dusk 380..439 (0 -> 16), night */
    UINT16 t = day_t();
    if(t >= 440 || t < 100) return 16;
    if(t < 160) return (UINT8)((159 - t) * 16 / 60);
    if(t < 380) return 0;
    return (UINT8)((t - 380) * 16 / 60);
}
void tod_name(char *out) BANKED {
    UINT16 t = day_t(); const char *s = t >= 440 || t < 100 ? "NIGHT" : t < 160 ? "DAWN" : t < 380 ? "DAY" : "DUSK";
    scat(out, s);
}
void tod_tint(palette_color_t *p, UINT8 n) BANKED {
    UINT8 d = tod_dark(), i;
    if(!d) return;
    for(i = 0; i < n; i++){
        UINT16 c = p[i], r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
        r = r * (32 - d) / 32; g = g * (64 - d * 7 / 4) / 64; b = b * (64 - d * 5 / 4) / 64 + d / 4;
        p[i] = (palette_color_t)(r | (g << 5) | (b << 10));
    }
}
void tod_sleep(void) BANKED {                         /* on to t = 160 (the morning): a whole number of 20 s steps */
    UINT16 t = day_t(), need = (UINT16)((160 + 480 - t) % 480);
    ex.off = (UINT8)((ex.off + (need + 19) / 20) % 24);
}

/* ------------------------------------------------------------ pets */
#define PH 13
static UINT16 phx[PH], phy[PH]; static UINT8 phi, pht, pf;
void pet_load(void) BANKED {
    VBK_REG = VBK_BANK_1; set_sprite_data(PET_TILE, 4, pet_tiles); VBK_REG = VBK_BANK_0;
}
void pet_reset(UINT16 x, UINT16 y) BANKED { UINT8 i; for(i = 0; i < PH; i++){ phx[i] = x; phy[i] = y; } phi = 0; pht = 0; }
void pet_push(UINT16 x, UINT16 y) BANKED {
    if(++pht < 3) return;
    pht = 0; phi = phi + 1 == PH ? 0 : phi + 1; phx[phi] = x; phy[phi] = y;
}
static UINT8 pet_n(void){ return sv.friends > 3 ? 3 : sv.friends; }
UINT8 pet_get(UINT8 i, UINT16 *x, UINT16 *y) BANKED {
    UINT8 j;
    if(i >= pet_n()) return 0;
    j = (UINT8)((phi + PH - (i + 1) * 4) % PH);
    *x = phx[j]; *y = phy[j];
    return 1;
}
void pet_draw(const UINT8 *oam, UINT16 cx, UINT16 cy, UINT8 wrap) BANKED {
    UINT8 i; pf++;
    for(i = 0; i < 3; i++){
        UINT16 x, y; INT16 sx, sy;
        if(!pet_get(i, &x, &y)){ move_sprite(oam[i], 0, 0); continue; }
        sx = (INT16)(x - cx); sy = (INT16)(y - cy);
        if(wrap){ sx &= 4095; if(sx > 2048) sx -= 4096; sy &= 4095; if(sy > 2048) sy -= 4096; }
        sx += (INT16)i * 6 - 6;
        if(sx < -8 || sx > 168 || sy < -8 || sy > 152){ move_sprite(oam[i], 0, 0); continue; }
        set_sprite_tile(oam[i], PET_TILE + ((pf >> 3) & 1) * 2); set_sprite_prop(oam[i], BUGSPR_PAL0 | 0x08);
        move_sprite(oam[i], (UINT8)(sx + 4), (UINT8)(sy + 1));
    }
}

/* ------------------------------------------------------------ fishing */
static const char *const fish_nm[6] = { "MINNOW", "SANDFISH", "FROSTFISH", "PIRANHA", "TUNA", "GLOWFISH" };
static void fline(UINT8 y, const char *s, UINT8 pal){ txt_win(1); txt_line(1, y, s, 18); txt_color(1, y, 18, 1, pal); txt_win(0); }
UINT8 fish_run(UINT8 world) BANKED {
    UINT16 wait; UINT8 phase = 0, t = 0, caught = 0, k; char b[24], *d;
    rs ^= (UINT16)frames32() ^ ((UINT16)DIV_REG << 8); if(!rs) rs = 1;
    wait = 40 + (UINT16)rnd() % 150;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 6); txt_win(0); move_win(7, 144 - 48); SHOW_WIN;
    fline(1, "FISHING", TXT_GOLD); fline(3, "THE LINE IS IN...", TXT_WHITE); fline(4, "A: PULL   B: STOP", TXT_GREY);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) break;
        if(phase == 0){
            if(HIT(J_A)){ fline(3, "TOO EARLY! IT LEFT.", TXT_RED); sfx(SFX_BACK); for(k = 0; k < 50; k++) wait_vbl_done(); break; }
            if(!(wait & 31)){ d = scat(b, "THE LINE IS IN"); { UINT8 n = (UINT8)((wait >> 5) & 3); while(n--) d = scat(d, "."); } fline(3, b, TXT_WHITE); }
            if(!--wait){ phase = 1; t = 45; sfx(SFX_SPLASH); fline(3, "!! A BITE !!  PULL!", TXT_GOLD); shake(4); }
        } else {
            if(HIT(J_A)){
                caught = 1;
                if((rnd() & 15) == 0){ if(ex.rare < 99) ex.rare++; d = scat(b, "A GOLDEN "); scat(d, fish_nm[world % 6]); fline(3, b, TXT_GOLD); sfx(SFX_FANFARE); }
                else { if(ex.fish < 99) ex.fish++; d = scat(b, "A "); scat(d, fish_nm[world % 6]); scat(d, "!"); fline(3, b, TXT_WHITE); sfx(SFX_COIN); }
                d = u16s(scat(b, "FISH "), ex.fish); d = scat(d, "  GOLDEN "); u16s(d, ex.rare); fline(4, b, TXT_GREY);
                for(k = 0; k < 70; k++){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B)) break; }
                break;
            }
            if(!--t){ fline(3, "IT GOT AWAY...", TXT_RED); sfx(SFX_BACK); for(k = 0; k < 50; k++) wait_vbl_done(); break; }
        }
    }
    txt_win(1); txt_cls(); txt_win(0);
    hud_redraw(); hud_pos(HUD_Y);
    return caught;
}

/* ------------------------------------------------------------ the queen's words and her trick */
static const char *const memory[8] = {      /* what each data core gives back to Bolt (the GBA game's words) */
    "I WAS A SHIP. I WAS CALLED THE DAWN. I REMEMBER THE DARK, AND THE STARS, AND SOMEONE SMALL WHO SANG.",
    "I CARRIED SEEDS. A GARDEN. A POND. A LAWN AT NIGHT. I PLANTED THEM ONE BY ONE. YOUR WORLDS. I MADE THEM.",
    "MY CREW WERE BUGS. SMALL. STUBBORN. BRAVE. THEY SLEPT IN MY HEART ON THE LONG WAY. ONE OF THEM LOOKED LIKE YOU.",
    "WE FELL. THE ANTHILL CAUGHT US. THE ANTS WERE KIND AT FIRST. THEN THEY WERE HUNGRY.",
    "THE ANT KING OPENED MY HEART. HE BUILT A MOUTH AROUND IT. HE FED IT SHIPS SO IT WOULD GROW. THAT MOUTH IS THE TECHNOBEAST.",
    "EVERY SHIP IT EATS, IT GROWS. IT IS EATING MY MEMORY TOO. THAT IS WHY I FORGET. THAT IS WHY I AM SMALL.",
    "THERE IS A WORD THAT OPENS MY HEART. THE KING NEVER FOUND IT. IT IS MY NAME. MY REAL NAME.",
    "I KNOW MY NAME NOW. TAKE ME TO THE BEAST. I WILL SAY IT. ITS HEART WILL OPEN. I AM NOT AFRAID. I HAVE YOU." };
static const char *const trick_nm[6] = { "VINE GRIP: POTIONS HEAL MORE", "SAND DASH: B RUNS FASTER", "FROST FANG: YOUR BITE HITS HARDER",
                                         "THORN SHELL: YOU TAKE LESS DAMAGE", "TIDE LUNGS: YOU HEAL SLOWLY ON YOUR OWN", "SHADOW STEP: SAFE LONGER AFTER A HIT" };
void dungeon_won(UINT8 kind) BANKED {                 /* the queen is beaten and the cage is open */
    char b[100], *d;
    if(kind < 6 && !TRICK(kind)){                       /* the first time this kind of queen falls she gives a trick, and it stays */
        ex.tricks |= 1 << kind;
        d = scat(b, "A NEW TRICK! "); scat(d, trick_nm[kind]); sfx(SFX_LEVEL);
        dlg_say("BOLT", b);
    }
    if(world_freed(sv.star)){ dlg_say("BOLT", "THE QUEEN CAME BACK, AND YOU BEAT HER AGAIN. THIS WORLD IS STILL SAFE."); return; }
    sv.zone++;
    if(sv.cores >= 8){ dlg_say("BOLT", "THE QUEEN IS BEATEN AND YOUR FRIEND IS FREE! THIS WORLD IS SAFE NOW."); return; }
    sv.freed[sv.cores++] = sv.star;
    sfx(SFX_LEVEL);
    d = scat(b, "THE QUEEN IS BEATEN AND YOUR FRIEND IS FREE! SHE HAD ONE OF MY DATA CORES. THAT IS "); d = u16s(d, sv.cores); scat(d, " OF 8.");
    dlg_say("BOLT", b);
    dlg_say("BOLT", memory[sv.cores - 1]);
    if(sv.cores == 8) dlg_say("BOLT", "THAT WAS THE LAST OF ME. FIND THE TECHNOBEAST: IN SPACE THE ARROW POINTS TO IT NOW. SHOOT ITS CORE WHEN IT GLOWS.");
    else dlg_say("BOLT", "EVERY WORLD WE FREE MAKES THE NEXT DUNGEON DEEPER. LET'S FLY TO A NEW WORLD!");
}
