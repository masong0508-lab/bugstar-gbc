#pragma bank 49
/* BUGSTAR GBC - BUG HIVE, the defend phase (hive_int.h). A boss or a raid comes down the shaft toward the queen. You place guards on the tunnels (they stand where you put
   them), then watch: raiders walk the shortest road to the queen (the flood in VIS), guards and towers hit them, and they have to break walls and gates in the way. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hive_int.h"
#include "hive_art.h"

#define MAXG 10
#define MAXE 12
#define QX 16
#define QY 7
#define px hvd.px
#define py hvd.py
#define gx hvd.gx
#define gy hvd.gy
#define ghp hvd.ghp
#define gflash hvd.gflash
#define gn hvd.gn
#define gmax hvd.gmax
#define ox hvd.ox
#define oy hvd.oy
#define ohp hvd.ohp
#define oflash hvd.oflash
#define on_ hvd.on_
#define oboss hvd.oboss
#define brk_hp hvd.brk_hp
#define brk_at_x hvd.brk_at_x
#define brk_at_y hvd.brk_at_y
#define tx hvd.tx
#define ty hvd.ty
#define tl hvd.tl
#define tn hvd.tn

static UINT8 enemy_pass(UINT8 c){ c = CT(c); return c == C_TUNNEL || c == C_ENTRY || c == C_QUEEN || c == C_SITE || c >= C_FIRSTB; }
static void flood(void){
    UINT8 head = 0, tail = 0, d, x, y, v; UINT16 cur, nb;
    memset(VIS, 0, 1024); HV->stat[S_VGEN] = 0;
    cur = CI(QX, QY); VIS[cur] = 1; QUE[tail++ & 127] = cur;
    while(head != tail){
        cur = QUE[head++ & 127]; x = (UINT8)(cur & 31); y = (UINT8)(cur >> 5); v = VIS[cur];
        if(v >= 254) continue;
        for(d = 0; d < 4; d++){
            if((d == 0 && y == 0) || (d == 1 && x == MW - 1) || (d == 2 && y == MH - 1) || (d == 3 && x == 0)) continue;
            nb = cur + (d == 0 ? -32 : d == 2 ? 32 : d == 1 ? 1 : -1);
            if(VIS[nb] || !enemy_pass(MAP[nb])) continue;
            VIS[nb] = v + 1;
            if((UINT8)(tail - head) < 126) QUE[tail++ & 127] = nb;
        }
    }
}
static UINT8 guard_at(UINT8 x, UINT8 y){ UINT8 i; for(i = 0; i < gn; i++) if(gx[i] == x && gy[i] == y && ghp[i]) return i + 1; return 0; }
static UINT8 enemy_at(UINT8 x, UINT8 y){ UINT8 i; for(i = 0; i < on_; i++) if(ox[i] == x && oy[i] == y && ohp[i]) return i + 1; return 0; }
static void hud_text(UINT8 mode, UINT8 a, UINT8 b2, UINT8 step){
    char b[24], *d;
    txt_soft();
    if(mode == 0){
        d = scat(b, "PLACE GUARDS "); d = u16s(d, gn); d = scat(d, "/"); d = u16s(d, gmax); txt_line(0, 0, b, 20);
        txt_line(0, 1, "A:PLACE/TAKE B:UNDO", 20); txt_line(0, 2, "START:GO SELECT:AUTO", 20);
    } else {
        d = scat(b, "FOES "); d = u16s(d, a); d = scat(d, " GUARDS "); d = u16s(d, b2); txt_line(0, 0, b, 20);
        d = scat(b, "STEP "); d = u16s(d, step); txt_line(0, 1, b, 20); txt_line(0, 2, "", 20);
    }
    txt_color(0, 0, 20, 1, TXT_GOLD); txt_color(0, 1, 20, 2, TXT_WHITE);
    txt_soft_end();
}
static UINT8 ok_cell(UINT8 x, UINT8 y){ UINT8 c = CT(MAP[CI(x, y)]); return VIS[CI(x, y)] && c != C_QUEEN && c != C_WALL && c != C_SITE && c != C_ENTRY && c != C_GATE; }
static void put_guard(UINT8 x, UINT8 y){
    UINT8 slot = gn, i;
    for(i = 0; i < gn; i++) if(!ghp[i]){ slot = i; break; }
    if(slot == gn) gn++;
    gx[slot] = px[slot] = x; gy[slot] = py[slot] = y; ghp[slot] = 5 + HV->bsum[C_BARR] + HV->gear / 10; gflash[slot] = 0;
}
/* SELECT: the free guards go along the raiders' road (the downhill walk of the flood from the entrance to the queen), spread out so towers and posts cover each other */
static void autoplace(void){
    UINT8 rx[40], ry[40], len = 0, x = ENTX, y = ENTY, v, d, bv, nx, ny, want, j, idx, tries;
    while(len < 40){
        rx[len] = x; ry[len] = y; len++; v = VIS[CI(x, y)];
        if(v <= 1) break;
        bv = v; nx = x; ny = y;
        for(d = 0; d < 4; d++){
            UINT8 cx2 = x + (d == 1 ? 1 : d == 3 ? 255 : 0), cy2 = y + (d == 2 ? 1 : d == 0 ? 255 : 0);
            if(cx2 >= MW || cy2 >= MH) continue;
            if(VIS[CI(cx2, cy2)] && VIS[CI(cx2, cy2)] < bv){ bv = VIS[CI(cx2, cy2)]; nx = cx2; ny = cy2; }
        }
        if(nx == x && ny == y) break;
        x = nx; y = ny;
    }
    while(gn && !ghp[gn - 1]) gn--;
    { UINT8 have = 0; for(j = 0; j < gn; j++) if(ghp[j]) have++; want = gmax > have ? gmax - have : 0; }
    if(len < 4 || !want) return;
    for(j = 0; j < want; j++){
        idx = 2 + (UINT8)((UINT16)j * (len - 3) / want);
        for(tries = 0; tries < 4; tries++, idx++){
            if(idx >= len - 1) idx = 2;
            if(ok_cell(rx[idx], ry[idx]) && !guard_at(rx[idx], ry[idx])){ put_guard(rx[idx], ry[idx]); break; }
        }
    }
}
/* a guard without a target walks up to 3 cells from its post toward the nearest raider within 4, and drifts home when none is near */
static void patrol(UINT8 gi, UINT8 spawned){
    UINT8 best = 255, bd = 255, e, d, tx2, ty2, nx, ny, cur, dd;
    for(e = 0; e < spawned; e++){
        UINT8 dx, dy;
        if(!ohp[e]) continue;
        dx = ox[e] > gx[gi] ? ox[e] - gx[gi] : gx[gi] - ox[e]; dy = oy[e] > gy[gi] ? oy[e] - gy[gi] : gy[gi] - oy[e];
        if(dx + dy < bd){ bd = dx + dy; best = e; }
    }
    if(best != 255 && bd <= 4){ tx2 = ox[best]; ty2 = oy[best]; } else { tx2 = px[gi]; ty2 = py[gi]; }
    cur = (gx[gi] > tx2 ? gx[gi] - tx2 : tx2 - gx[gi]) + (gy[gi] > ty2 ? gy[gi] - ty2 : ty2 - gy[gi]);
    if(cur <= 1 && best != 255 && tx2 == ox[best]) return;
    if(!cur) return;
    for(d = 0; d < 4; d++){
        nx = gx[gi] + (d == 1 ? 1 : d == 3 ? 255 : 0); ny = gy[gi] + (d == 2 ? 1 : d == 0 ? 255 : 0);
        if(nx >= MW || ny >= MH) continue;
        dd = (nx > tx2 ? nx - tx2 : tx2 - nx) + (ny > ty2 ? ny - ty2 : ty2 - ny);
        if(dd >= cur || !ok_cell(nx, ny) || guard_at(nx, ny) || enemy_at(nx, ny)) continue;
        if((nx > px[gi] ? nx - px[gi] : px[gi] - nx) + (ny > py[gi] ? ny - py[gi] : py[gi] - ny) > 3) continue;
        gx[gi] = nx; gy[gi] = ny; return;
    }
}
static void draw(UINT8 f, UINT8 cur_x, UINT8 cur_y, UINT8 show_cur){
    UINT8 i;
    for(i = 0; i < MAXG; i++){
        if(i < gn && ghp[i]) hm_put_sprite(1 + i, (INT16)gx[i] * 8, (INT16)gy[i] * 8, TS_SHLD, gflash[i] ? 7 : 4, 0); else move_sprite(1 + i, 0, 0);
        if(gflash[i]) gflash[i]--;
    }
    for(i = 0; i < MAXE; i++){
        if(i < on_ && ohp[i]) hm_put_sprite(11 + i, (INT16)ox[i] * 8, (INT16)oy[i] * 8, (f & 8) ? TS_BEETH1 : TS_BEETH0, oflash[i] ? 7 : (oboss && i == 0 ? 2 : 6), 0); else move_sprite(11 + i, 0, 0);
        if(oflash[i]) oflash[i]--;
    }
    for(i = 0; i < 8; i++) move_sprite(24 + i, 0, 0);
    if(show_cur && !(f & 16)) hm_put_sprite(0, (INT16)cur_x * 8, (INT16)cur_y * 8, TS_CURS, 7, 0); else move_sprite(0, 0, 0);
}
UINT8 hd_run(void) BANKED {
    UINT8 k = HV->rpend - 1, na = 0, cx, cy, f = 0, step = 0, alive, i, won = 0, done = 0, spawned = 0, fcnt = 0, ng = 0;
    UINT16 s = hs_strength(k);
    for(i = 0; i < HV->pop; i++) if(HV->wk[i].job == J_GUARD) ng++;
    DISPLAY_OFF; HIDE_SPRITES; txt_pal_reset(); hm_pals(); hm_fullmap();
    txt_win(1); WX_REG = 7; WY_REG = 120; SHOW_WIN; SHOW_BKG; SHOW_SPRITES;
    flood();
    gmax = 2 + ng + ((HV->tech >> 7) & 1); if(gmax > MAXG) gmax = MAXG; gn = 0;
    memset(ghp, 0, sizeof ghp); memset(gflash, 0, sizeof gflash); memset(oflash, 0, sizeof oflash);
    /* the towers */
    tn = 0; { UINT16 q; for(q = 0; q < 1024 && tn < 8; q++) if(CT(MAP[q]) == C_TOWER){ tx[tn] = (UINT8)(q & 31); ty[tn] = (UINT8)(q >> 5); tl[tn] = CLV(MAP[q]) + 1; tn++; } }
    /* the raiders */
    on_ = 2 + s / 7; if(on_ > 10) on_ = 10; oboss = k == RK_BOSS;
    if(oboss){ on_ = 5 + s / 30; if(on_ > MAXE) on_ = MAXE; }
    na = on_;
    if(HV->rpend2){ UINT8 n2 = 2 + hs_strength(HV->rpend2 - 1) / 10; if(n2 > 5) n2 = 5; on_ += n2; if(on_ > MAXE) on_ = MAXE; }       /* a second band comes down the shaft too */
    for(i = 0; i < on_; i++){ ox[i] = ENTX; oy[i] = ENTY; ohp[i] = 3 + HV->rn / 2 + ((i < na ? k : HV->rpend2 - 1) == RK_RIVAL ? 1 : 0) + HV->day / 20; }
    if(oboss) ohp[0] = 24 + 2 * HV->rn;
    cx = ENTX; cy = 9;
    hm_cursor_to(cx, cy); DISPLAY_ON;
    hud_text(0, 0, 0, 0);
    music(MUS_BOSS);
    for(;;){                                                            /* placing guards */
        UINT8 moved = 0;
        wait_vbl_done(); input_poll(); f++;
        if(HITR(J_UP) && cy > 3){ cy--; moved = 1; } if(HITR(J_DOWN) && cy < MH - 2){ cy++; moved = 1; }
        if(HITR(J_LEFT) && cx > 1){ cx--; moved = 1; } if(HITR(J_RIGHT) && cx < MW - 2){ cx++; moved = 1; }
        if(moved){ hm_cursor_to(cx, cy); sfx(SFX_MOVE); }
        if(HIT(J_A)){
            UINT8 g = guard_at(cx, cy);
            if(g){ ghp[g - 1] = 0; if(g == gn) gn--; hud_text(0, 0, 0, 0); sfx(SFX_BACK); }
            else {
                UINT8 c = CT(MAP[CI(cx, cy)]);
                if(gn < gmax && VIS[CI(cx, cy)] && c != C_QUEEN && c != C_WALL && c != C_SITE && c != C_ENTRY && c != C_GATE){
                    put_guard(cx, cy); hud_text(0, 0, 0, 0); sfx(SFX_PICK);
                } else sfx(SFX_HURT);
            }
        }
        if(HIT(J_B)){ while(gn && !ghp[gn - 1]) gn--; if(gn){ ghp[gn - 1] = 0; gn--; hud_text(0, 0, 0, 0); sfx(SFX_BACK); } }
        if(HIT(J_SELECT)){ autoplace(); hud_text(0, 0, 0, 0); sfx(SFX_OK); }
        if(HIT(J_START)) break;
        draw(f, cx, cy, 1);
    }
    for(i = 0; i < gn; i++) ghp[i] = ghp[i] ? ghp[i] : 0;
    while(gn && !ghp[gn - 1]) gn--;
    brk_hp = 0; brk_at_x = brk_at_y = 0; step = 0; spawned = 0;
    sfx(SFX_HIT);
    while(!done){                                                        /* the fight */
        wait_vbl_done(); input_poll(); f++;
        draw(f, 0, 0, 0);
        if(++fcnt < 9) continue;
        fcnt = 0; step++;
        if(step > 3 * on_ + 150){ done = 1; won = 0; break; }
        if(spawned < on_ && !(step % 3)) spawned++;
        alive = 0;
        {   /* the guards hit */
            UINT8 gi, ei, busy[MAXG];
            memset(busy, 0, sizeof busy);
            for(gi = 0; gi < gn; gi++){
                if(!ghp[gi]) continue;
                for(ei = 0; ei < spawned; ei++){
                    UINT8 dx = ox[ei] > gx[gi] ? ox[ei] - gx[gi] : gx[gi] - ox[ei], dy = oy[ei] > gy[gi] ? oy[ei] - gy[gi] : gy[gi] - oy[ei];
                    if(!ohp[ei] || dx + dy > 1) continue;
                    busy[gi] = 1;
                    { UINT8 dm = 2 + HV->gear / 20 + ((HV->tech >> 4) & 1) * 2 + (hs_rnd() & 1); ohp[ei] = ohp[ei] > dm ? ohp[ei] - dm : 0; oflash[ei] = 4; if(!ohp[ei]) sfx(SFX_CRUNCH); else sfx(SFX_BITE); }
                    break;
                }
            }
            for(gi = 0; gi < gn; gi++) if(ghp[gi] && !busy[gi]) patrol(gi, spawned);       /* guards that have no one to hit walk */
            {                                                            /* the towers shoot */
                UINT8 t, best, bd, j, r, dx, dy, m, dm;
                for(t = 0; t < tn; t++){
                    r = 3 + tl[t] / 2 + ((HV->tech >> 6) & 1) * 2; best = 255; bd = 255;
                    for(j = 0; j < spawned; j++){
                        if(!ohp[j]) continue;
                        dx = ox[j] > tx[t] ? ox[j] - tx[t] : tx[t] - ox[j]; dy = oy[j] > ty[t] ? oy[j] - ty[t] : ty[t] - oy[j]; m = dx > dy ? dx : dy;
                        if(m <= r && VIS[CI(ox[j], oy[j])] < bd){ bd = VIS[CI(ox[j], oy[j])]; best = j; }
                    }
                    if(best != 255){ dm = 1 + tl[t] / 3 + ((HV->tech >> 6) & 1); ohp[best] = ohp[best] > dm ? ohp[best] - dm : 0; oflash[best] = 3; sfx(SFX_LASER); }
                }
            }
            for(ei = 0; ei < spawned; ei++){                             /* raiders hit or walk */
                UINT8 hit = 0, d, nx, ny, bestd, bd2, atk;
                if(!ohp[ei]) continue;
                alive++;
                atk = 1 + HV->rn / 4 + ((ei < na ? k : HV->rpend2 - 1) == RK_RIVAL ? 1 : 0) + (oboss && ei == 0 ? 3 : 0); if(atk > 6) atk = 6;
                for(gi = 0; gi < gn; gi++){
                    UINT8 dx = ox[ei] > gx[gi] ? ox[ei] - gx[gi] : gx[gi] - ox[ei], dy = oy[ei] > gy[gi] ? oy[ei] - gy[gi] : gy[gi] - oy[ei];
                    if(ghp[gi] && dx + dy <= 1){ ghp[gi] = ghp[gi] > atk ? ghp[gi] - atk : 0; gflash[gi] = 4; hit = 1; break; }
                }
                if(hit) continue;
                if(ox[ei] == QX && oy[ei] == QY){ won = 0; done = 1; break; }
                bestd = VIS[CI(ox[ei], oy[ei])]; nx = ox[ei]; ny = oy[ei]; bd2 = bestd;
                for(d = 0; d < 4; d++){
                    UINT8 cx2 = ox[ei] + (d == 1 ? 1 : d == 3 ? 255 : 0), cy2 = oy[ei] + (d == 2 ? 1 : d == 0 ? 255 : 0);
                    if(cx2 >= MW || cy2 >= MH) continue;
                    if(VIS[CI(cx2, cy2)] && VIS[CI(cx2, cy2)] < bd2 && !enemy_at(cx2, cy2)){ bd2 = VIS[CI(cx2, cy2)]; nx = cx2; ny = cy2; }
                }
                if(nx == ox[ei] && ny == oy[ei]) continue;
                { UINT8 t = CT(MAP[CI(nx, ny)]);
                  if(t == C_WALL || t == C_GATE){                          /* break through */
                      if(brk_at_x != nx || brk_at_y != ny){ brk_at_x = nx; brk_at_y = ny; brk_hp = 5 * (CLV(MAP[CI(nx, ny)]) + 1) + ((HV->tech >> 5) & 1) * 4; }
                      brk_hp = brk_hp > atk ? brk_hp - atk : 0; sfx(SFX_THUD);
                      if(!brk_hp){ MAP[CI(nx, ny)] = C_TUNNEL; hm_cell(nx, ny); brk_at_x = brk_at_y = 0; }
                      continue;
                  }
                }
                ox[ei] = nx; oy[ei] = ny;
            }
        }
        { UINT8 gl = 0; for(i = 0; i < gn; i++) if(ghp[i]) gl++;
          hud_text(1, alive, gl, step);
          for(i = 0; i < spawned; i++) if(ohp[i]){ hm_look(ox[i], oy[i]); break; } }
        if(!done && spawned >= on_ && !alive){ done = 1; won = 1; }
        if(!done){ UINT8 left = 0; for(i = 0; i < on_; i++) if(ohp[i]) left++; if(!left){ done = 1; won = 1; } }
    }
    for(i = 0; i < 60; i++){ wait_vbl_done(); draw(f++, 0, 0, 0); }
    memset(VIS, 0, 1024); HV->stat[S_VGEN] = 0;
    hm_sprites_off(); hs_recount();
    return won;
}
