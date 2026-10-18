#pragma bank 9
/* BUGSTAR GBC - the warp: a 3D rail shooter (see warp.h).
   The engine: things live in 3D, x and y in world units, z from 255 (far) to 16 (where your ship flies). A thing is drawn at
   screen 80 + x * 64 / z, 72 + y * 64 / z: the divide is a multiply by warp_rz[z] = 4096 / z and a shift. At z = 16 one world
   unit is 4 pixels, so a meteor thrown at world (x, y) reaches the screen at (80 + 4x, 72 + 4y): that is how they are aimed at you.
   Far things draw as small sprites, near ones as 16x16. Your shots start at your ship (z = 16) and fly into the screen; a shot hits
   what is at the same x, y and depth. The tunnel behind is one still picture whose 4 colours step to make the rings fly at you,
   and it slides a little as you steer.
   Sprites (8x16 mode, tiles in VRAM bank 0 from 128): 0,1 the ship; 2..4 shots; 5..16 meteors and shards; 17..22 pirates;
   23..26 pirate orbs; 27..38 the starfield. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "input.h"
#include "save.h"
#include "hud.h"
#include "text.h"
#include "menus.h"
#include "dialog.h"
#include "fx.h"
#include "sound.h"
#include "space_art.h"
#include "space.h"
#include "warp_art.h"
#include "warp.h"

#define T_STAR   128             /* 3 star sizes (8x16, dot in the top half) */
#define T_WARP   134             /* meteors and shards: warp_spr, 16 tiles */
#define T_SHIP   150             /* the ship: straight, banking right, banking left (4 tiles each) */
#define T_PIR    162             /* pirate, 2 frames */
#define T_BOOM   170             /* explosion, 2 frames */
#define T_ORB    178             /* an orb (2 tiles) */
#define P_STAR 0
#define P_SHIP 1
#define P_PIR  2
#define P_BOOM 3
#define P_ROCK 4
#define P_HOT  5
#define P_SHOT 6

#define NSTAR 12
#define NHAZ  6
#define NPIR  3
#define NEORB 4
#define NSHOT 3
#define O_SHIP 0
#define O_SHOT 2
#define O_HAZ  5
#define O_PIR  17
#define O_EORB 23
#define O_STAR 27

enum { H_NONE, H_ROCK, H_SHARD, H_BOOM };
typedef struct { INT8 x, y; UINT8 z, kind, hp, t; } Haz;
typedef struct { INT16 x, y; INT8 vx; UINT8 z, hp, t, st; } Pir;        /* x, y in 1/4 units; st: 0 none, 1 coming, 2 fighting, 3 leaving, 4.. blowing up */
typedef struct { INT16 x, y, dx, dy; UINT8 z; } EOrb;                  /* x, y in 1/16 units */
typedef struct { INT8 x, y; UINT8 z; } Shot;

static INT8  sx3[NSTAR], sy3[NSTAR]; static UINT8 sz3[NSTAR];
static Haz   haz[NHAZ];
static Pir   pir[NPIR];
static EOrb  eorb[NEORB];
static Shot  shot[NSHOT];
static INT16 px, py;                     /* the ship's middle on the screen */
static UINT8 inv, speed;
static WarpResult *R;
static UINT16 rng = 0x7A31;
static UINT8 rnd(void){ rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; return (UINT8)rng; }

static const UINT8 star_tiles[96] = {                                    /* far, mid, near (8x16: the dot is in the top half) */
    0,0, 0,0, 0,0, 0x18,0, 0,0, 0,0, 0,0, 0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0, 0,0, 0x18,0x18, 0x3C,0x24, 0x18,0x18, 0,0, 0,0, 0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0, 0x18,0x18, 0x3C,0x3C, 0x7E,0x66, 0x3C,0x3C, 0x18,0x18, 0,0, 0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static const palette_color_t my_pals[12] = {
    RGB(0,0,0), RGB(10,14,24), RGB(20,26,31), RGB(31,31,31),            /* stars */
    RGB(0,0,0), RGB(5,3,2), RGB(16,10,6), RGB(27,21,14),                /* meteors */
    RGB(0,0,0), RGB(24,10,2), RGB(31,24,6), RGB(31,31,24) };            /* star shards: hot */
static const palette_color_t white4[4] = { RGB(31,31,31), RGB(31,31,31), RGB(31,31,31), RGB(31,31,31) };

static INT16 proj(INT16 v, UINT8 z){                     /* v * 64 / z, without a divide (|v| up to 255) */
    UINT16 a = v < 0 ? -v : v, p = (a * warp_rz[z]) >> 6;
    return v < 0 ? -(INT16)p : (INT16)p;
}
static void put16(UINT8 o, UINT8 tile, INT16 x, INT16 y, UINT8 pal){     /* a 16x16 sprite with its middle at (x, y) */
    if(x < -8 || x > 168 || y < -8 || y > 152){ move_sprite(o, 0, 0); move_sprite(o + 1, 0, 0); return; }
    set_sprite_tile(o, tile); set_sprite_tile(o + 1, tile + 2); set_sprite_prop(o, pal); set_sprite_prop(o + 1, pal);
    move_sprite(o, (UINT8)x, (UINT8)(y + 8)); move_sprite(o + 1, (UINT8)(x + 8), (UINT8)(y + 8));
}
static void put8(UINT8 o, UINT8 tile, INT16 x, INT16 y, UINT8 pal){      /* an 8x8 picture (top of an 8x16) with its middle at (x, y) */
    if(x < -4 || x > 164 || y < -4 || y > 148){ move_sprite(o, 0, 0); return; }
    set_sprite_tile(o, tile); set_sprite_prop(o, pal); move_sprite(o, (UINT8)(x + 4), (UINT8)(y + 12));
}
static INT8 wx(void){ return (INT8)((px - 80) / 4); }   /* where the ship is, in world units at z = 16 */
static INT8 wy(void){ return (INT8)((py - 72) / 4); }

static void star_born(UINT8 i, UINT8 far){
    do { sx3[i] = (INT8)rnd(); sy3[i] = (INT8)rnd(); } while(sx3[i] > -12 && sx3[i] < 12 && sy3[i] > -12 && sy3[i] < 12);
    sz3[i] = far ? 255 - (rnd() & 31) : 24 + rnd() % 231;
}
static void hurt(UINT8 dmg){
    if(inv) return;
    sv.hp = sv.hp > dmg ? sv.hp - dmg : 0;
    inv = 50; R->hits++; sfx(SFX_HURT); shake(12);
}
static void throw_haz(void){                              /* a meteor or a shard, aimed near where you are now */
    UINT8 i;
    for(i = 0; i < NHAZ; i++) if(!haz[i].kind){
        Haz *h = &haz[i];
        h->kind = (rnd() < 80) ? H_SHARD : H_ROCK;
        h->x = wx() + (INT8)(rnd() % 17) - 8; h->y = wy() + (INT8)(rnd() % 13) - 6;
        h->z = 250; h->hp = h->kind == H_ROCK ? 1 + (rnd() & 1) : 255; h->t = rnd() & 1;
        return;
    }
}
static void pir_fire(Pir *p){                            /* an orb from a pirate at where you are */
    UINT8 i, steps;
    for(i = 0; i < NEORB; i++) if(!eorb[i].z){
        EOrb *o = &eorb[i];
        o->x = p->x * 4; o->y = p->y * 4; o->z = p->z;        /* (1/4 units -> 1/16 units) */
        steps = (p->z - 16) / 4; if(!steps) steps = 1;
        o->dx = (INT16)(((INT16)wx() * 16 - o->x) / steps); o->dy = (INT16)(((INT16)wy() * 16 - o->y) / steps);
        sfx(SFX_FIRE); return;
    }
}

void warp_run(UINT16 frames, WarpResult *res) BANKED {
    UINT16 t; UINT8 i, j, phase = 0, pacc = 0, cool = 0, attack_at, k, row[32], arow[32], ship_t = T_SHIP;
    R = res; R->gold = 0; R->rocks = R->pirates = R->hits = R->wrecked = 0;
    rng ^= DIV_REG | ((UINT16)DIV_REG << 8); if(!rng) rng = 1;
    R->attacked = (rnd() % 3) == 0;                       /* pirates: about one warp in three */
#ifdef TEST_ATTACK
    R->attacked = 1;
#endif
    attack_at = R->attacked ? (UINT8)((frames / 2) >> 4) : 255;
    /* the tunnel: 20x18 of tiles, widened to the whole 32x32 map so it can slide as you steer */
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, WARP_NTILES, warp_tiles);
    for(j = 0; j < 32; j++){
        UINT8 sy = j < 18 ? j : j < 25 ? 17 : 0;
        for(i = 0; i < 32; i++){ UINT8 sxx = i < 20 ? i : i < 26 ? 19 : 0; row[i] = warp_map[sy * 20 + sxx]; arow[i] = warp_attr[sy * 20 + sxx]; }
        VBK_REG = VBK_BANK_1; set_bkg_tiles(0, j, 32, 1, arow);
        VBK_REG = VBK_BANK_0; set_bkg_tiles(0, j, 32, 1, row);
    }
    set_bkg_palette(4, 4, warp_pals);
    set_sprite_data(T_STAR, 6, star_tiles);
    set_sprite_data(T_WARP, WARP_NSPR, warp_spr);
    space_spr_copy(T_SHIP, SP_T_SHIP(0), 8); space_spr_copy(T_SHIP + 8, SP_T_SHIP(7), 4);
    space_spr_copy(T_PIR, SP_T_PIRATE(0), 8); space_spr_copy(T_BOOM, SP_T_BOOM(0), 8); space_spr_copy(T_ORB, SP_T_ORB, 2);
    set_sprite_palette(P_STAR, 1, &my_pals[0]); set_sprite_palette(P_ROCK, 2, &my_pals[4]);
    space_pals_copy(P_SHIP, 0, 1); space_pals_copy(P_PIR, 1, 1); space_pals_copy(P_BOOM, 3, 1); space_pals_copy(P_SHOT, 2, 1);
    SCX_REG = 0; SCY_REG = 0; SPRITES_8x16;
    for(i = 0; i < 40; i++){ move_sprite(i, 0, 0); set_sprite_prop(i, 0); }
    for(i = 0; i < NSTAR; i++) star_born(i, 0);
    for(i = 0; i < NHAZ; i++) haz[i].kind = 0;
    for(i = 0; i < NPIR; i++) pir[i].st = 0;
    for(i = 0; i < NEORB; i++) eorb[i].z = 0;
    for(i = 0; i < NSHOT; i++) shot[i].z = 0;
    px = 80; py = 92; inv = 0;
    txt_pal_reset(); hud_init();
    SHOW_BKG; SHOW_SPRITES;
    sfx(SFX_BOOM);
    fade_in();
    for(t = 0; t < frames; t++){
        input_poll(); k = keys;
        speed = t < 60 ? 1 + t / 12 : t + 60 > frames ? 1 + (frames - t) / 12 : 6;
        /* the ship: steer anywhere on the screen, A shoots */
        ship_t = T_SHIP;
        if(k & J_LEFT){ if(px > 20) px -= 2; ship_t = T_SHIP + 8; }
        if(k & J_RIGHT){ if(px < 140) px += 2; ship_t = T_SHIP + 4; }
        if((k & J_UP) && py > 24) py -= 2;
        if((k & J_DOWN) && py < 112) py += 2;
        if(cool) cool--;
        if((k & J_A) && !cool){ for(i = 0; i < NSHOT; i++) if(!shot[i].z){ shot[i].x = wx(); shot[i].y = wy(); shot[i].z = 20; cool = 7; sfx(SFX_LASER); break; } }
        if(inv) inv--;
        /* the tunnel */
        pacc += speed; if(pacc >= 4){ pacc -= 4; phase = (phase + 1) & 3; }
        /* meteors and shards: more as the warp goes on */
        if(t > 90 && t + 120 < frames && !(rnd() % (t < 600 ? 34 : 22))) throw_haz();
        /* a hyperspace attack (sometimes) */
        if((UINT8)(t >> 4) == attack_at && !(t & 15)){
            banner(BN_ATTACK); hud_init();
            for(i = 0; i < NPIR; i++){ Pir *p = &pir[i]; p->st = 1; p->z = 255; p->x = ((INT16)(i * 40) - 40) * 4; p->y = -60 + (INT16)(rnd() & 31) * 2; p->hp = 3 + (sv.zone > 4 ? 4 : sv.zone); p->vx = i == 1 ? 1 : -1; p->t = rnd(); }
            attack_at = 255;
        }
        /* shots fly away from you */
        for(i = 0; i < NSHOT; i++){ Shot *s = &shot[i];
            if(!s->z) continue;
            if(s->z > 240){ s->z = 0; continue; }
            s->z += 8;
            for(j = 0; j < NHAZ; j++){ Haz *h = &haz[j]; INT8 dx, dy;
                if(h->kind != H_ROCK) continue;
                dx = s->x - h->x; dy = s->y - h->y;
                if(dx > -5 && dx < 5 && dy > -5 && dy < 5 && s->z + 14 > h->z && s->z < h->z + 14){
                    s->z = 0; sfx(SFX_HIT);
                    if(!--h->hp){ h->kind = H_BOOM; h->t = 14; R->rocks++; R->gold += 3; sfx(SFX_CRUNCH); }
                    break;
                }
            }
            if(!s->z) continue;
            for(j = 0; j < NPIR; j++){ Pir *p = &pir[j]; INT16 dx, dy;
                if(p->st != 2 && p->st != 1) continue;
                dx = (INT16)s->x * 4 - p->x; dy = (INT16)s->y * 4 - p->y;
                if(dx > -24 && dx < 24 && dy > -24 && dy < 24 && s->z + 16 > p->z && s->z < p->z + 16){
                    s->z = 0; sfx(SFX_HIT); shake(3);
                    if(!--p->hp){ p->st = 4; R->pirates++; R->gold += 15 + sv.zone * 3; sfx(SFX_BOOM); shake(10); }
                    break;
                }
            }
        }
        /* meteors and shards come at you */
        for(i = 0; i < NHAZ; i++){ Haz *h = &haz[i];
            if(!h->kind) continue;
            if(h->kind == H_BOOM){ if(!--h->t) h->kind = 0; continue; }
            if(h->z <= 16 + speed + 2){                          /* it reaches you: hit, or a near miss */
                INT16 dx = 80 + h->x * 4 - px, dy = 72 + h->y * 4 - py;
                if(dx > -12 && dx < 12 && dy > -12 && dy < 12){ hurt(h->kind == H_ROCK ? 3 + sv.zone : 2 + sv.zone / 2); h->kind = H_BOOM; h->t = 10; h->z = 18; }
                else h->kind = 0;
                continue;
            }
            h->z -= speed + 2;
        }
        /* pirates */
        for(i = 0; i < NPIR; i++){ Pir *p = &pir[i];
            if(!p->st) continue;
            if(p->st >= 4){ if(++p->st > 22) p->st = 0; continue; }
            if(p->st == 1){ p->z -= 3; if(p->z <= 90) p->st = 2; }
            else if(p->st == 2){
                p->x += p->vx * 2; if(p->x > 200 || p->x < -200) p->vx = -p->vx;     /* strafe side to side */
                if(!(++p->t & 63)) pir_fire(p);
                if(t + 150 > frames) p->st = 3;
            } else { p->z += 4; if(p->z > 240) p->st = 0; }                         /* gone */
        }
        for(i = 0; i < NEORB; i++){ EOrb *o = &eorb[i];
            if(!o->z) continue;
            o->x += o->dx; o->y += o->dy;
            if(o->z <= 20){
                INT16 dx = 80 + o->x / 4 - px, dy = 72 + o->y / 4 - py;
                if(dx > -10 && dx < 10 && dy > -10 && dy < 10) hurt(2 + sv.zone / 2);
                o->z = 0; continue;
            }
            o->z -= 4;
        }
        /* the starfield */
        for(i = 0; i < NSTAR; i++){
            if(sz3[i] <= 8 + speed * 2) star_born(i, 1); else sz3[i] -= speed * 2;
        }
        /* draw */
        wait_vbl_done();
        shake_tick();
        SCX_REG = (UINT8)((px - 80) / 6 + shake_x); SCY_REG = (UINT8)((py - 72) / 6 + shake_y);      /* the tunnel slides as you steer */
        set_bkg_palette(4, 4, &warp_pals[phase * 16]);
        if(inv & 4){ move_sprite(O_SHIP, 0, 0); move_sprite(O_SHIP + 1, 0, 0); } else put16(O_SHIP, ship_t, px, py, P_SHIP);
        for(i = 0; i < NSHOT; i++){
            if(shot[i].z) put8(O_SHOT + i, T_ORB, 80 + proj(shot[i].x, shot[i].z), 72 + proj(shot[i].y, shot[i].z), P_SHOT); else move_sprite(O_SHOT + i, 0, 0);
        }
        for(i = 0; i < NHAZ; i++){ Haz *h = &haz[i]; UINT8 o = O_HAZ + i * 2; INT16 x, y;
            if(!h->kind){ move_sprite(o, 0, 0); move_sprite(o + 1, 0, 0); continue; }
            x = 80 + proj(h->x, h->z); y = 72 + proj(h->y, h->z);
            if(h->kind == H_BOOM) put16(o, T_BOOM + (h->t > 6 ? 4 : 0), x, y, P_BOOM);
            else if(h->z > 110){ move_sprite(o + 1, 0, 0); put8(o, h->kind == H_ROCK ? T_WARP + WARP_T_ROCK_S : T_WARP + WARP_T_SHARD_S, x, y, h->kind == H_ROCK ? P_ROCK : P_HOT); }
            else put16(o, h->kind == H_ROCK ? T_WARP + WARP_T_ROCK(h->t) : T_WARP + WARP_T_SHARD, x, y, h->kind == H_ROCK ? P_ROCK : P_HOT);
        }
        for(i = 0; i < NPIR; i++){ Pir *p = &pir[i]; UINT8 o = O_PIR + i * 2; INT16 x, y;
            if(!p->st){ move_sprite(o, 0, 0); move_sprite(o + 1, 0, 0); continue; }
            x = 80 + proj(p->x / 4, p->z); y = 72 + proj(p->y / 4, p->z);
            if(p->st >= 4) put16(o, T_BOOM + (p->st > 12 ? 4 : 0), x, y, P_BOOM);
            else if(p->z > 160){ move_sprite(o + 1, 0, 0); put8(o, T_STAR + 2, x, y, P_PIR); }
            else put16(o, T_PIR + ((t >> 3) & 1) * 4, x, y, P_PIR);
        }
        for(i = 0; i < NEORB; i++){ EOrb *e = &eorb[i];
            if(e->z) put8(O_EORB + i, T_ORB, 80 + proj(e->x / 16, e->z), 72 + proj(e->y / 16, e->z), P_BOOM); else move_sprite(O_EORB + i, 0, 0);
        }
        for(i = 0; i < NSTAR; i++){ INT16 x = 80 + proj(sx3[i], sz3[i]), y = 72 + proj(sy3[i], sz3[i]);
            if(x < -4 || x > 164 || y < -4 || y > 148){ star_born(i, 1); move_sprite(O_STAR + i, 0, 0); continue; }
            put8(O_STAR + i, T_STAR + (sz3[i] < 70 ? 4 : sz3[i] < 150 ? 2 : 0), x, y, P_STAR);
        }
        hud_update();
        if(!sv.hp){ R->wrecked = 1; sv.hp = 1; break; }
    }
    if(!R->hits) R->gold += 50;                           /* a clean run */
    sv.gold += R->gold;
    wait_vbl_done();
    for(i = 4; i < 8; i++) set_bkg_palette(i, 1, white4);           /* out of the tunnel: a flash */
    for(i = 0; i < 40; i++) move_sprite(i, 0, 0);
    for(i = 0; i < 6; i++) wait_vbl_done();
    hud_hide(); SCX_REG = 0; SCY_REG = 0;
}

void warp_report(WarpResult *r) BANKED {
    char b[100], *d;
    d = scat(b, r->wrecked ? "WE MADE IT... BARELY. " : "WARP DONE! ");
    d = scat(d, "METEORS "); d = u16s(d, r->rocks); d = scat(d, ", PIRATES "); d = u16s(d, r->pirates);
    d = scat(d, ", HITS "); d = u16s(d, r->hits); d = scat(d, ". +"); d = u16s(d, r->gold); d = scat(d, " GOLD");
    if(!r->hits) d = scat(d, " (CLEAN RUN BONUS!)");
    if(r->gold > sv.warp_best){ sv.warp_best = r->gold; d = scat(d, " NEW BEST!"); }
    dlg_say("BOLT", b);
    if(r->rocks || r->pirates) gain_xp(r->rocks + r->pirates * 4);
}
void space_say(UINT8 id) BANKED {
    if(id == SAY_HOWTO) dlg_say("BOLT", "SPACE! STEER WITH THE D-PAD. B GOES FASTER AND A SHOOTS. THE ARROW POINTS TO THE NEAREST WORLD: FLY OVER ONE AND PRESS A TO LAND. SELECT WARPS US TO WHERE THE ARROW POINTS!");
    else if(id == SAY_BEAST) dlg_say("BOLT", "THERE IT IS: THE TECHNOBEAST! LASERS BOUNCE OFF ITS ARMOUR. SHOOT ITS CORE WHEN IT GLOWS PINK!");
    else { dlg_say("BOLT", "NOW! I SAY MY NAME... I AM DAWN!"); dlg_say(0, "THE BEAST'S HEART OPENS WIDE. EVERY SHIP IT EVER ATE FLIES OUT, BACK TO THE STARS."); }
}
