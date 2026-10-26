#pragma bank 33
/* BUGSTAR GBC - FLEET BATTLE (see fleet.h): a one-screen fight in the stars. You and your wingmen against waves of drones.
   All its state sits in WRAM bank 2 (the always-there RAM is full), which stays selected while it runs. */
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
#include "space_int.h"
#include "space.h"
#include "fleet.h"

typedef struct { INT16 x, y; INT8 dx, dy; UINT8 life, own; } Shot;      /* own: 1 yours, 0 theirs */
typedef struct { INT16 x, y; INT8 vx; UINT8 hp, st, t, ty; } Foe;       /* st: 0 free, 1 flying, 2.. blowing up */
typedef struct { INT16 x, y; UINT8 cool; } Wing;
typedef struct {
    Shot s[12]; Foe f[4]; Wing w[3];
    INT16 px, py; UINT8 dir, cool, hp, inv, wave, left, spawn, kills, hud, boss;
    UINT16 rng;
} Arena;
#define A WRAM_AT(0xD000, Arena)
#define NSHOT 12
#define NFOE  4
#define O_PL 0                 /* sprites: you 0-1, wingmen 2-7, foes 8-15, shots 16-39 (12 x 2) */
#define O_WG 2
#define O_FO 8
#define O_SH 16

static UINT8 rn(void){ A->rng ^= A->rng << 7; A->rng ^= A->rng >> 9; A->rng ^= A->rng << 8; return (UINT8)A->rng; }
static INT16 ab(INT16 v){ return v < 0 ? -v : v; }
static void shoot(INT16 x, INT16 y, INT8 dx, INT8 dy, UINT8 own){
    UINT8 i; Shot *s = A->s;
    for(i = 0; i < NSHOT; i++) if(!s[i].life){ s[i].x = x; s[i].y = y; s[i].dx = dx; s[i].dy = dy; s[i].life = 60; s[i].own = own; return; }
}
static void hud_line(void){
    char b[21], *d; UINT8 i;
    d = scat(b, "SHIELD "); d = u16s(d, A->hp); d = scat(d, "  WAVE "); d = u16s(d, A->wave); d = scat(d, "/3  KO "); d = u16s(d, A->kills); *d = 0;
    for(i = 0; b[i]; i++); while(i < 20) b[i++] = ' '; b[20] = 0;
    wram_bank(1);
    txt_win(1); txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_WHITE); txt_win(0);
    wram_bank(2);
}
static void say(const char *m){                         /* a word on the bottom line */
    char b[21]; UINT8 i = 0;
    while(m[i] && i < 20){ b[i] = m[i]; i++; } while(i < 20) b[i++] = ' '; b[20] = 0;
    wram_bank(1);
    txt_win(1); txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_GOLD); txt_win(0);
    wram_bank(2);
}
static void wave_start(void){
    UINT8 n = 3 + A->wave * 2 + (A->boss && A->wave == 3 ? 0 : 0);
    A->left = n; A->spawn = 20;
    if(A->boss && A->wave == 3) A->left = 1;
}
static void spawn_foe(UINT8 tier){
    UINT8 i; Foe *f = A->f;
    for(i = 0; i < NFOE; i++) if(!f[i].st){
        f[i].x = 16 + rn() % 128; f[i].y = -10; f[i].vx = (rn() & 1) ? 1 : -1; f[i].t = rn() & 63; f[i].ty = 18 + rn() % 40;
        f[i].hp = 1 + tier / 3;
        if(A->boss && A->wave == 3){ f[i].hp = 12 + tier * 2; f[i].x = 80; }
        f[i].st = 1; A->left--; return;
    }
}
static UINT8 foes_alive(void){ UINT8 i, n = 0; for(i = 0; i < NFOE; i++) if(A->f[i].st) n++; return n; }

static void draw(void){
    UINT8 i; Shot *s = A->s; Foe *f = A->f;
    wram_bank(2);
    if(A->inv & 4) hide2(O_PL); else spr16(O_PL, SP_T_SHIP(A->dir), A->px, A->py);
    for(i = 0; i < 3; i++){
        if(i < fh.wm) spr16(O_WG + i * 2, SP_T_SHIP(0), A->w[i].x, A->w[i].y); else hide2(O_WG + i * 2);
    }
    for(i = 0; i < NFOE; i++){
        if(f[i].st == 1) spr16(O_FO + i * 2, SP_T_PIRATE((f[i].t >> 3) & 1), f[i].x, f[i].y);
        else if(f[i].st >= 2) spr16(O_FO + i * 2, SP_T_BOOM(f[i].st > 9), f[i].x, f[i].y);
        else hide2(O_FO + i * 2);
    }
    for(i = 0; i < NSHOT; i++){
        if(!s[i].life){ hide2(O_SH + i * 2); continue; }
        if(s[i].own){ spr16(O_SH + i * 2, SP_T_LASER, s[i].x, s[i].y); }
        else { spr16(O_SH + i * 2, SP_T_ORB, s[i].x, s[i].y); }
    }
}
static void props(void){
    UINT8 i;
    for(i = 0; i < 8; i++) set_sprite_prop(i, 0x08);                       /* you and the wingmen: the ship colours */
    for(i = 8; i < 16; i++) set_sprite_prop(i, 1 | 0x08);
    for(i = 16; i < 40; i++) set_sprite_prop(i, 2 | 0x08);
}

UINT8 fleet_battle(UINT8 tier, UINT8 boss, UINT8 *kills) BANKED {
    UINT8 i, res = 0, frame = 0, end = 0; Foe *f; Shot *s;
    wram_bank(2);
    memset(A, 0, sizeof(Arena));
    A->rng = (UINT16)play_secs() * 31 + 7 + tier; A->boss = boss;
    A->px = 80; A->py = 112; A->dir = 0;
    A->hp = 5 + UPG(UP_HULL) + FL_ARMOR() + (UINT8)(fh.wm); if(A->hp > 15) A->hp = 15;
    for(i = 0; i < 3; i++){ A->w[i].x = 80 + (i == 0 ? -26 : i == 1 ? 26 : 0); A->w[i].y = 140 + 10 * i; A->w[i].cool = 20 + i * 11; }
    A->wave = 1; wave_start();
    f = A->f; s = A->s;
    HIDE_SPRITES; for(i = 0; i < 40; i++) move_sprite(i, 0, 0);
    SPRITES_8x16; props();
    SHOW_BKG; SHOW_SPRITES;
    wram_bank(1); txt_win(1); txt_cls(); txt_win(0); wram_bank(2);
    move_win(7, 136); SHOW_WIN; SCX_REG = 0; SCY_REG = 0;
    hud_line();
    DISPLAY_ON;
    say("FLEET BATTLE!");
    for(;;){
        input_poll(); frame++;
        if(end){ if(--end == 0) break; }
        else {
            /* you */
            { UINT8 k = keys; INT8 mx = 0, my = 0;
              if(k & J_LEFT) mx = -2; if(k & J_RIGHT) mx = 2; if(k & J_UP) my = -2; if(k & J_DOWN) my = 2;
              A->px += mx; A->py += my;
              if(A->px < 8) A->px = 8; if(A->px > 152) A->px = 152; if(A->py < 24) A->py = 24; if(A->py > 124) A->py = 124;
              if(mx || my) A->dir = mx > 0 ? (my > 0 ? 3 : my < 0 ? 1 : 2) : mx < 0 ? (my > 0 ? 5 : my < 0 ? 7 : 6) : (my > 0 ? 4 : 0);
              if(A->cool) A->cool--;
              if((k & (J_A | J_B)) && !A->cool){ static const INT8 ddx[8] = { 0, 4, 5, 4, 0, -4, -5, -4 }, ddy[8] = { -5, -4, 0, 4, 5, 4, 0, -4 };
                  shoot(A->px, A->py, ddx[A->dir], ddy[A->dir], 1); A->cool = 9 - (UPG(UP_GUNS) > 3 ? 3 : UPG(UP_GUNS)); sfx(SFX_LASER); }
            }
            if(HIT(J_START)){ res = 0; end = 1; say("YOU FELL BACK"); }
            /* wingmen */
            for(i = 0; i < fh.wm; i++){ Wing *w = &A->w[i]; INT16 tx = A->px + (i == 0 ? -26 : i == 1 ? 26 : 0), ty = A->py + 18 + (i == 2 ? 14 : 0);
                if(w->x < tx) w->x++; else if(w->x > tx) w->x--; if(w->y < ty) w->y++; else if(w->y > ty) w->y--;
                if(w->cool) w->cool--;
                else { UINT8 k, b = 255; INT16 bd = 9999;
                    for(k = 0; k < NFOE; k++) if(f[k].st == 1){ INT16 d = ab(f[k].x - w->x) + ab(f[k].y - w->y); if(d < bd){ bd = d; b = k; } }
                    if(b != 255){ INT16 dx = f[b].x - w->x, dy = f[b].y - w->y, m = ab(dx) > ab(dy) ? ab(dx) : ab(dy);
                        if(m){ shoot(w->x, w->y, (INT8)(dx * 5 / m), (INT8)(dy * 5 / m), 1); w->cool = 34 + i * 7; sfx(SFX_LASER); } }
                }
            }
            /* shots */
            for(i = 0; i < NSHOT; i++){ Shot *t = &s[i]; UINT8 k;
                if(!t->life) continue;
                t->life--; t->x += t->dx; t->y += t->dy;
                if(t->x < -8 || t->x > 168 || t->y < -8 || t->y > 152){ t->life = 0; continue; }
                if(t->own){
                    for(k = 0; k < NFOE; k++) if(f[k].st == 1 && ab(t->x - f[k].x) < 10 && ab(t->y - f[k].y) < 10){
                        UINT8 ld = 1 + (UPG(UP_LASER) + 1) / 2 + FL_GUN();
                        t->life = 0; sfx(SFX_HIT);
                        if(f[k].hp > ld) f[k].hp -= ld;
                        else { f[k].hp = 0; f[k].st = 2; A->kills++; sfx(SFX_BOOM); hud_line(); }
                        break;
                    }
                } else if(!A->inv && ab(t->x - A->px) < 7 && ab(t->y - A->py) < 7){
                    t->life = 0; sfx(SFX_HURT); A->inv = 50;
                    if(A->hp > 0) A->hp--;
                    hud_line();
                    if(!A->hp){ res = 0; end = 90; say("SHIP LOST..."); }
                }
            }
            if(A->inv) A->inv--;
            /* the drones */
            for(i = 0; i < NFOE; i++){ Foe *e = &f[i];
                if(!e->st) continue;
                if(e->st >= 2){ if(++e->st > 16) e->st = 0; continue; }
                e->t++;
                if(e->y < e->ty) e->y++;
                if(!(frame & 1)){ e->x += e->vx; if(e->x < 14) e->vx = 1; if(e->x > 146) e->vx = -1; }
                if(!(rn() & 63)) e->vx = -e->vx;
                if(A->boss && A->wave == 3 && !(e->t & 31)){      /* the outpost's core: a spread of three */
                    INT8 k; for(k = -1; k <= 1; k++) shoot(e->x, e->y + 8, k * 2, 3, 0); }
                else if(!(e->t & 63) && e->y >= e->ty){
                    INT16 dx = A->px - e->x, dy = A->py - e->y, m = ab(dx) > ab(dy) ? ab(dx) : ab(dy);
                    if(m) shoot(e->x, e->y + 6, (INT8)(dx * 2 / m), (INT8)(dy * 2 / m), 0);
                }
                if(!A->inv && ab(e->x - A->px) < 10 && ab(e->y - A->py) < 10){ A->inv = 50; sfx(SFX_HURT); if(A->hp) A->hp--; hud_line(); if(!A->hp){ res = 0; end = 90; say("SHIP LOST..."); } }
            }
            /* waves */
            if(A->left && !--A->spawn){ spawn_foe(tier); A->spawn = 40 - (A->wave * 6); }
            if(!A->left && !foes_alive() && !end){
                if(A->wave >= 3){ res = 1; end = 100; say("VICTORY!"); sfx(SFX_FANFARE); }
                else { A->wave++; wave_start(); hud_line(); say("NEXT WAVE!"); }
            }
        }
        wait_vbl_done();
        SCY_REG = (UINT8)(SCY_REG - 1);
        draw();
    }
    *kills = A->kills;
    DISPLAY_OFF;
    HIDE_WIN;
    wram_bank(1);
    return res;
}
