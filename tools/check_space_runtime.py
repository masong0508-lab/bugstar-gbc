#!/usr/bin/env python3
"""Run the real GBC space navigation and pirate timer code under UBSan.

The fixture replaces display/audio/service calls, not planet selection,
input dispatch, flight, labels, chart handoff, or timer arithmetic. It uses
the production declarations and headers so byte-sized sentinel/timer
regressions cannot be hidden by wider host-only fixture variables.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
read = lambda name: (root / "src" / name).read_text()
flat, cockpit, flight, boss, foe, save = map(read, (
    "space.c", "space3d.c", "flight3d.c", "space3d_boss.c", "space_foe.c", "save.c",
))


def braced(source, start):
    opening = source.index("{", start)
    depth = 0
    for end in range(opening, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if not depth:
                return source[start:end + 1]
    raise AssertionError("unterminated production C block")


def function(source, name):
    match = re.search(r"^(?:static )?[^\n;{}]*\b" + name + r"\([^;{}]*\)(?: BANKED)?\s*\{", source, re.M)
    assert match, f"missing production function {name}"
    return braced(source, match.start())


def declarations(source, names):
    found = []
    for name in names:
        match = re.search(r"^(?:static )?(?:const )?(?:UINT8|UINT16|INT8|INT16)\s+[^;\n]*\b" + name + r"\b[^;\n]*;", source, re.M)
        if match and match.group() not in found:
            found.append(match.group())
    return "\n".join(found)


test = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"
#include "save.h"
#include "space.h"
#include "space3d_int.h"
#include "elite.h"
#include "econ.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "sound.h"
#include "dialog.h"
#include "warp.h"
#undef SHOW_SPRITES
#undef HIDE_SPRITES
#define SHOW_SPRITES (LCDC_REG |= LCDCF_OBJON)
#define HIDE_SPRITES (LCDC_REG &= (UINT8)~LCDCF_OBJON)
Save sv;
Opts opt;
UINT8 keys, keys_old, keys_rep, cur_slot, inv, toast_t, story_boss_state, arena_on;
UINT16 shx, shy, rng, b_x, b_y;
INT16 vx, vy;
UINT8 b_on, b_dead;
INT16 sz, pit16, vel, vfx, vfy, vfz, vrx, vry, vux, vuy, vuz;
UINT16 yaw;
P3 pir[NP3];
static Cand cand[16];
static UINT8 ncand;
static unsigned current_world, spawned, saved, fired, redrawn, warp_frames;
static UINT16 chart_choice;
UINT8 LCDC_REG, SCX_REG, SCY_REG;
INT8 shake_x, shake_y;
static UINT8 window_y;
static Laser las[NLAS];
static struct { UINT8 life; } puff[2];
static int docked = -1;
static char lines[3][32], dialogue[96];
static UINT8 sprite_x[40], sprite_y[40], sprite_tile[40];
static unsigned beacons;
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"Space runtime regression (world %u) at %d: %s\n",current_world,__LINE__,#x); exit(1); } }while(0)
void move_sprite(UINT8 n, UINT8 x, UINT8 y){ sprite_x[n] = x; sprite_y[n] = y; }
void set_sprite_tile(UINT8 n, UINT8 tile){ sprite_tile[n] = tile; }
void set_sprite_prop(UINT8 n, UINT8 p){ (void)n; (void)p; }
void txt_win(UINT8 on){ (void)on; }
void txt_line(UINT8 x, UINT8 y, const char *s, UINT8 w){
    UINT8 n;
    CHECK(y < 3 && x + w <= 32);
    for(n = 0; n < w; n++) lines[y][x + n] = *s ? *s++ : ' ';
    lines[y][20] = 0;
}
void txt_color(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 p){ (void)x; (void)y; (void)w; (void)h; (void)p; }
void hud_redraw(void){ redrawn++; strcpy(lines[1], "HUD"); }
void hud_update(void){}
void hud_pos(UINT8 y){ window_y = y; }
void shake_tick(void){}
void set_bkg_palette_entry(UINT8 p, UINT8 n, palette_color_t c){ (void)p; (void)n; (void)c; }
void sfx(UINT8 n){ (void)n; }
void dlg_say(const char *who, const char *s){ (void)who; CHECK(strlen(s) < sizeof dialogue); strcpy(dialogue, s); }
void slot_write(UINT8 slot){ (void)slot; saved++; }
UINT8 elite_dock(UINT8 s){ docked = s; return 0; }
void s3_boss_hud(void){ strcpy(lines[2], "BOSS"); }
static void spr(UINT8 tile, INT16 x, INT16 y, UINT8 pal){ (void)x; (void)y; CHECK(tile == TS(ARW_D) && pal == PAL_LAS); beacons++; }
static void pirate_spawn(UINT8 wave){ (void)wave; spawned++; pir[0].st = 1; }
static void basis(void){}
static void objects_clear(void){}
static void gfx(void){}
static void gfx3(void){}
static void fade_out(void){}
static void fire(void){ fired++; }
static void shoot(void){ fired++; }
void hud_hide(void){}
void foes_clear(void){}
static void warship_ambush(void){}
UINT16 galaxy_run(UINT8 from, UINT8 beast){ (void)from; (void)beast; return chart_choice; }
void warp_run(UINT16 frames, WarpResult *wr){ warp_frames = frames; memset(wr, 0, sizeof *wr); }
void warp_report(WarpResult *wr){ (void)wr; }
"""
test += declarations(flat, ("shown", "dir", "arrow_best", "arrow_t", "aim_ok", "aim_beast", "aim_x", "aim_y", "dir_x", "dir_y", "gun_cool"))
test += "\n" + declarations(cockpit, ("near_pl", "near_st", "tgt", "tgt_rear", "tgt_d", "dn", "prev_dn", "dl_t", "dl_x", "dl_y", "dl_p"))
test += "\n" + declarations(flight, ("cool", "turn_h", "spawn_t", "regen", "dock_lock"))
for source, names in (
    (save, ("world_seed", "world_freed")),
    (flat, ("near_station", "over_star", "label", "dock_label", "arrow", "space_tables")),
    (cockpit, ("proj", "cand_add", "s3_scan", "s3_from2d", "s3_to2d", "station_beacon", "s3_flush")),
    (flight, ("fly", "step_spawn")),
    (boss, ("s3_target_hud", "s3_land")),
    (foe, ("toast",)),
):
    for name in names:
        test += "\n" + function(source, name)

# Compile the actual action blocks, including their declared ID widths and
# branches. A one-iteration loop stands in for the outer flight loop only.
test += "\nstatic int flat_action(void){ UINT8 k = 0; do{\n"
test += braced(flat, flat.index("if(HIT(J_A)){", flat.index("static int space2d")))
test += "\n}while(0); out: return k; }\n"
test += "\nstatic int cockpit_action(void){ UINT8 out = 0; do{\n"
test += braced(flight, flight.index("if(HIT(J_A) && !story_boss_state)"))
test += "\n}while(0); return out; }\n"
test += "\nstatic void cockpit_fire(UINT8 k){\n"
test += flight[flight.index("if((k & J_A) && !cool &&"):flight.index("        step_bolts(); step_pirates(); step_orbs(); step_pods(); step_spawn();")]
test += "\n}\n"
label_start = re.search(r"\{ UINT(?:8|16) o = over_star\(\),", flat)
assert label_start
test += "\nstatic void flat_label_tick(void)" + braced(flat, label_start.start())
test += "\nstatic void flip_view(void){ UINT8 was3 = 0;\n"
test += braced(flat, flat.index("if(!was3 && V3_ON)"))
test += "\n}\n"
test += "\nstatic void flat_chart(void){ UINT8 i; do{\n"
test += braced(flat, flat.index("if(HIT(J_SELECT) && !b_dead)"))
test += "\n}while(0); }\n"

test += r"""
/* 256 is an expected out-of-range value, independent of the implementation.
   This keeps the test runnable against the old byte declarations as proof. */
#define NO_WORLD 256
static void reset_observed(void){
    saved = fired = redrawn = spawned = beacons = warp_frames = 0; docked = -1;
    memset(lines, 0, sizeof lines); memset(dialogue, 0, sizeof dialogue);
    memset(pir, 0, sizeof pir); keys = J_A; keys_old = 0;
    cool = dock_lock = inv = story_boss_state = arena_on = toast_t = 0;
    opt.autosave = 1; opt.arrow = 1;
    vfx = vfz = vry = vux = vuy = 0; vfy = -64; vrx = vuz = 64;
}
static void place(UINT8 s, INT16 dx, INT16 dy, INT16 dz){
    shx = (UINT16)((star_px(s) + dx) & 4095) << 4;
    shy = (UINT16)((star_py(s) + dy) & 4095) << 4;
    sz = (star_z(s) + dz) * 16;
}
static unsigned short_names, long_names;
static void check_action_name(UINT8 s, const char *prompt){
    char name[24], expected[21];
    size_t prefix = strlen(prompt), n;
    world_name(name, world_seed(s)); n = strlen(name);
    CHECK(prefix < 20);
    if(n > 20 - prefix){ n = 20 - prefix; long_names++; } else short_names++;
    memset(expected, ' ', 20); expected[20] = 0;
    memcpy(expected, prompt, prefix); memcpy(expected + prefix, name, n);
    CHECK(strcmp(lines[2], expected) == 0);
}
static void check_target_name(UINT8 s){
    char name[24], expected[21];
    size_t n;
    world_name(name, world_seed(s));
    snprintf(expected, sizeof expected, "%.12s %u", name, (unsigned)tgt_d);
    n = strlen(expected); memset(expected + n, ' ', 20 - n); expected[20] = 0;
    CHECK(strcmp(lines[2], expected) == 0);
}
static void navigation(UINT8 views){
    static const UINT32 seeds[] = {0, 0x2468ACE1UL, 0xFFFFFFFFUL, 0x12345678UL};
    unsigned seed, s, heading;
    for(seed = 0; seed < sizeof seeds / sizeof seeds[0]; seed++){
        sv.seed = seeds[seed]; space_tables();
        for(s = 0; s < 256; s++){
            current_world = s;
            if(views & 1){
            reset_observed(); place(s, 0, 0, 0);
            CHECK(over_star() == s);
            sv.star = (UINT8)(s + 1); flat_action();
            CHECK(sv.star == s && saved == 1 && docked == -1 && fired == 0);
            CHECK(strstr(dialogue, "LANDING ON ") == dialogue);
            reset_observed(); place(s, ST_DX, ST_DY, 8);
            CHECK(near_station() == s);
            flat_action(); CHECK(docked == (int)s && saved == 0 && fired == 0);
            }

            if(views & 2){
            reset_observed(); place(s, 0, 0, 0); s3_scan();
            CHECK(near_pl == s);
            sv.star = (UINT8)(s + 1); cockpit_fire(J_A); cockpit_action();
            CHECK(sv.star == s && saved == 1 && docked == -1 && fired == 0);
            s3_target_hud(); CHECK(strstr(lines[2], "A:LAND B:BRK ") == lines[2]);
            check_action_name(s, "A:LAND B:BRK ");
            spawn_t = 0; step_spawn(); CHECK(spawned == 0);
            vel = 24; fly(J_B, 0); CHECK(vel == 21);

            reset_observed(); place(s, ST_DX, ST_DY, 8); s3_scan();
            CHECK(near_st == s);
            cockpit_fire(J_A); cockpit_action();
            CHECK(docked == (int)s && saved == 0 && fired == 0 && dock_lock == 50);
            s3_target_hud(); CHECK(strstr(lines[2], "A:DOCK B:BRK ") == lines[2]);
            check_action_name(s, "A:DOCK B:BRK ");
            spawn_t = 0; step_spawn(); CHECK(spawned == 0);
            vel = 24; fly(J_B, 0); CHECK(vel == 21);

            /* A nearby port's beacon wins over a remote delivery target. */
            place(s, ST_DX, ST_DY + 20, 8); s3_scan(); tgt = (UINT8)(s + 128);
            station_beacon(); CHECK(beacons == 1);
            place(s, 110, 110, 0); sz = 3000; s3_scan(); tgt = s; tgt_rear = 0; tgt_d = 32767;
            s3_target_hud(); check_target_name(s);
            }

            /* Real view-switch reference/heading code, at every port. */
            place(s, ST_DX, ST_DY, 8); sv.el.rsv[0] = 3;
            for(heading = 0; heading < 8; heading++){
                dir = heading; flip_view();
                CHECK(sz == star_z(s) * 16 && s3_to2d() == heading);
            }

            /* Chart destinations now live only during the chart handoff;
               every byte ID must survive the real arrival assignments. */
            reset_observed(); keys = J_SELECT; chart_choice = s; sv.el.fuel = 20;
            flat_chart(); CHECK(warp_frames == 1500);
            CHECK(shx == (UINT16)(star_px(s) << 4));
            CHECK(shy == (UINT16)(((star_py(s) + 50) & 4095) << 4));
            CHECK(vx == 0 && vy == -16 && dir == 0);
        }
    }
    if(views & 2) CHECK(short_names && long_names);
    /* World 255's port also remains reachable from across the X wrap. */
    memset(star_j, 0xCC, 256); current_world = 255;
    place(255, ST_DX + 18, ST_DY, 8); CHECK(near_station() == 255);
    dir = 6; flip_view(); CHECK(sz == star_z(255) * 16 && s3_to2d() == 6);
    reset_observed(); keys = J_SELECT; chart_choice = GAL_BEAST; b_x = BEAST_X; b_y = BEAST_Y;
    flat_chart(); CHECK(warp_frames == 2100 && shx == (UINT16)(BEAST_X << 4));
    CHECK(shy == (UINT16)((BEAST_Y + 150) << 4));
    chart_choice = GAL_NONE; warp_frames = 0; flat_chart();
    CHECK(warp_frames == 0 && shx == (UINT16)(BEAST_X << 4) && shy == (UINT16)((BEAST_Y + 150) << 4));

    /* Compare every point around the overlapping interaction volumes to
       their geometric contract, including the station-wins-ties rule. */
    memset(star_j, 0x44, 256);
    for(s = 0; s < 256; s++){
        int x, y, z;
        current_world = s;
        for(x = -50; x <= 85; x += 5) for(y = -50; y <= 45; y += 5) for(z = -45; z <= 45; z += 15){
            int land = abs(x) < LAND_R && abs(y) < LAND_R && abs(z) < LAND_R;
            int dock = abs(x - ST_DX) < DOCK_R && abs(y - ST_DY) < DOCK_R && abs(z - 8) < DOCK_R;
            int lp = abs(x) + abs(y) + abs(z), ds = abs(x - ST_DX) + abs(y - ST_DY) + abs(z - 8);
            if(land && dock){ if(ds <= lp) land = 0; else dock = 0; }
            place(s, x, y, z); s3_scan();
            CHECK(near_pl == (land ? s : NO_WORLD)); CHECK(near_st == (dock ? s : NO_WORLD));
        }
    }
    puts("Space navigation: 256 worlds x 4 galaxy seeds, real A actions, brakes, safe zones, beacons, chart arrivals and 8-way view changes; 1,003,520 overlap points PASS");
}
static void labels_and_arrow(void){
    unsigned s, frame;
    sv.seed = 0x2468ACE1UL; space_tables(); reset_observed();
    for(s = 0; s < 256; s++){
        current_world = s;
        place(s, 0, 0, 0);
        dock_label((UINT8)(s + 1)); label(s);
        CHECK(strstr(lines[1], "A: LAND") != NULL);
        toast("TEST MESSAGE", "WAIT FOR IT"); toast_t = 1;
        flat_label_tick(); flat_label_tick();
        CHECK(strstr(lines[1], "A: LAND") != NULL);
        place(s, 110, 110, 0); label(over_star()); CHECK(strcmp(lines[1], "HUD") == 0);
        dock_label(s); CHECK(strstr(lines[1], "A: DOCK") != NULL);
    }
    memset(star_j, 0x44, 256);
    sv.el.jk = J_SEND; b_on = 0;
    for(s = 0; s < 256; s++){
        current_world = s; sv.el.jd = s; place(s, -128, 0, 0); arrow_t = 0;
        for(frame = 0; frame < 8; frame++){
            arrow(); CHECK(arrow_best == s);
            CHECK(sprite_x[OAM_ARR] && sprite_y[OAM_ARR]);
            CHECK(sprite_tile[OAM_ARR] == SPH_ARROW(2));
        }
    }
    sv.el.jk = J_NONE;
    puts("Space labels/arrow: all 256 worlds after station/toast/HUD transitions, delivery arrows and seven cached frames PASS");
}
static void timers(void){
    unsigned r, seed, ticks;
    reset_observed();
    /* Obtain 'none' through the real scan, so the old 255 sentinel can be
       exercised without truncating an expected 256 in this fixture. */
    memset(star_j, 0x44, 256); place(0, 110, 110, 0); s3_scan();
    sv.zone = 0; sv.level = 1;
    for(r = 0; r < 128; r++){
        for(seed = 1; seed < 65536; seed++){ rng = seed; if((rnd() & 127) == r) break; }
        CHECK(seed < 65536);
        rng = seed; spawn_t = 0; spawned = 0; memset(pir, 0, sizeof pir);
        step_spawn(); CHECK(spawned == 1);
        if(spawn_t != 260 + r) fprintf(stderr,"Pirate timer stored %u; intended %u ticks\n",(unsigned)spawn_t,260 + r);
        CHECK(spawn_t == 260 + r);
        memset(pir, 0, sizeof pir); /* the spawned pirate has been defeated */
        for(ticks = 260 + r; ticks; ticks--){ CHECK(spawn_t == ticks); step_spawn(); CHECK(spawned == 1); }
        CHECK(spawn_t == 0); step_spawn(); CHECK(spawned == 2);
    }
    spawn_t = 200; story_boss_state = 1; step_spawn(); CHECK(spawn_t == 200);
    story_boss_state = 0;
    puts("Space pirate timer: all 128 delays (260..387), every countdown tick, respawn and boss pause PASS");
}
static void cockpit_window(void){
    /* Dialogue restores the ordinary two-row HUD. The next actual cockpit
       flush must bring its third controls/target row back onto the LCD. */
    window_y = HUD_Y; s3_flush(1); CHECK(window_y == HUD3_Y);
    window_y = 0; s3_flush(32); CHECK(window_y == HUD3_Y);
    puts("Space cockpit HUD: actual frame flush restores all three visible rows after dialogue/menus PASS");
}
int main(int argc, char **argv){
    if(argc == 1 || !strcmp(argv[1], "navigation")) navigation(3);
    if(argc > 1 && !strcmp(argv[1], "cockpit")) navigation(2);
    if(argc == 1 || !strcmp(argv[1], "labels")) labels_and_arrow();
    if(argc == 1 || !strcmp(argv[1], "timer")) timers();
    if(argc == 1 || !strcmp(argv[1], "hud")) cockpit_window();
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-gbc-space-") as tmp:
    cpath, executable = Path(tmp) / "test.c", Path(tmp) / "test"
    cpath.write_text(test)
    subprocess.run([
        "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
        "-Wno-misleading-indentation", "-Wno-unused-function", "-Wno-unused-variable",
        "-fsanitize=undefined", "-fno-sanitize-recover=all",
        "-I" + str(root / "tests/stub"), "-I" + str(root / "src"),
        str(cpath), str(root / "src/util.c"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable), *sys.argv[1:]], check=True)
