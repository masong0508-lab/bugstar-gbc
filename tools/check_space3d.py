#!/usr/bin/env python3
"""GBC cockpit/flat flight parity, perspective and docking regressions.

Pure-Python checks that run before downloading GBDK; the ROM build remains the
authority for SDCC type checking, ROM-bank budgets and hardware constraints.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
read = lambda path: (root / path).read_text()
st = read("src/space3d_int.h")
s3 = read("src/space3d.c")
fly = read("src/flight3d.c")
boss = read("src/space3d_boss.c")
warp = read("src/warp.c")
flat = read("src/space.c")
api = read("src/space3d.h")

# The 2D metatile station is centered 3 cells to the right of the planet
# top-left, with Y at the top of that planet's 2x2 metatile picture.
station_dx = (3 * 16 + 8) - 16
station_dy = 8 - 16
assert (station_dx, station_dy) == (40, -8)
assert re.search(r"#define ST_DX\s+40\b", st)
assert re.search(r"#define ST_DY\s+\(-8\)", st)
assert "ex = dx + ST_DX; ey = dy + ST_DY" in s3
assert "aabs(ey) < DOCK_R" in s3
assert "proj(ex, ey, ez" in s3

# Planet landing and station docking volumes overlap in 3D. Selecting the
# first valid one used to make the port menu impossible to reach at the
# station's exact centre. Select the physically closer object instead.
assert "if(ad < best_pl)" not in s3 or "near_pl = s;" in s3
assert "ad < best_pl){ best_pl = ad; near_pl = s;" in s3
assert "if(ds < best_st){ best_st = ds; near_st = s; }" in s3
assert "if(best_st <= best_pl) near_pl = STAR_NONE;" in s3
assert "else near_st = STAR_NONE;" in s3

def chosen_action(x, y, z=0):
    # Ship offset from planet, station = (+40, -8, +8) relative to planet.
    land = max(abs(x), abs(y), abs(z)) < 46
    dock = max(abs(x - 40), abs(y + 8), abs(z - 8)) < 40
    planet_d = abs(x) + abs(y) + abs(z)
    dock_d = abs(x-40) + abs(y+8) + abs(z-8)
    if land and dock:
        return "dock" if dock_d <= planet_d else "land"
    return "dock" if dock else "land" if land else "none"
assert chosen_action(0,0,0) == "land"
assert chosen_action(40,-8,8) == "dock"
assert chosen_action(40,-8,0) == "dock"
assert chosen_action(31,-8,8) == "dock"
assert chosen_action(10,0,0) == "land"
assert chosen_action(90,0,0) == "none"
for x in range(-15,70):
    for y in range(-15,16):
        a=chosen_action(x,y,0)
        assert a in ("dock", "land", "none")

assert "((q & 15) + 3) << 4" in flat
assert "((q >> 4) << 4) + 8" in flat

# A full 256-step yaw turn must move the starfield by exactly 512 pixels.
# Because its BG tiles repeat at 256px, north wraps without an 8px jump.
scroll = lambda yaw: (-(yaw >> 7)) & 255
assert scroll(0) == 0
assert (scroll(0) - scroll(65535)) & 255 == 255
assert scroll(32768) == scroll(0)
assert "SCX_REG = (UINT8)(-(INT16)(yaw >> 7)" in s3
assert "SCY_REG = (UINT8)(-(pit16 >> 7)" in s3

# North (D-pad 0) = yaw 128, east (2) = yaw 64, south (4) =
# yaw 0, west (6) = yaw 192. Round trip must work for all eight.
for heading in range(8):
    yaw = (128 - (heading << 5)) & 255
    back = ((128 - yaw + 16) & 255) >> 5
    assert back == heading, (heading, yaw, back)
assert "s3_from2d(dir, ref);" in flat
assert "dir = s3_to2d();" in flat
assert "if(!was3 && V3_ON)" in flat
assert "sz = star_z(reference_star) * 16;" in s3
assert "void s3_from2d" in api and "UINT8 s3_to2d" in api

# B is never locked to turbo while inside the landing/docking zone.
assert "want = (k & J_B) ? 0 : 12" in fly
assert "UINT8 brake = (k & (J_B | J_DOWN)) == (J_B | J_DOWN)" in fly
assert "want = brake ? 0" in fly
assert "((k & J_DOWN) && !brake)" in fly
assert "if(vel > want) vel = want;" in fly
assert "if(vel < want) vel = want;" in fly
for velocity in (0, 3, 13, 24, 56, 92):
    v = velocity
    for frame in range(40):
        v -= 3 if v > 0 else 0
        if v < 0: v = 0
    assert v == 0, velocity

# Docking no longer teleports backwards, and holding A to land/dock
# doesn't also fire projectiles. Keep the small input cooldown.
assert "dock_lock = 50; vel = 0;" in fly
assert "shx -= (vfx * 70)" not in fly
assert "story_boss_state == 1 || (!story_boss_state && ((k & J_B) || (near_pl == STAR_NONE && near_st == STAR_NONE))))) shoot();" in fly
assert "if(HIT(J_A) && !story_boss_state && !(keys & J_B))" in fly, "No accidental arrival during combat override or finale"
assert 'vel > 12){ say3("SLOW DOWN: HOLD B"); continue; }' in fly
assert "if(near_st != STAR_NONE && !dock_lock)" in fly
assert "fade_out(); DISPLAY_OFF; gfx3();" in fly, "3D must restore tiles after station UI"
assert "fade_out(); DISPLAY_OFF; gfx();" in flat, "2D must restore tiles after station UI"

# The same 40-sprite OAM allocation is retained; nearer worlds beat
# far-away clutter and palettes no longer recolor active objects.
assert "static Cand cand[16]" in s3
assert "if(i >= 15)" in s3 and "if(z >= far) return;" in s3
assert "pal_used |= 1 << i" in s3
# Threats and the destination must win over cosmetic dust in the 40-sprite
# scene and ten-sprites-per-scanline GBC limits.
render = s3[s3.index("void s3_render("):s3.index("void s3_load(", s3.index("void s3_render("))]
assert render.index("list_things(frame);") < render.index("list_worlds();") < render.index("list_dust();")
assert "static void station_beacon(void)" in s3
assert "else { target_mark(); station_beacon(); }" in render
assert "spr(TS(ARW_D), sx - 4" in s3
assert "PAL_LAS" in s3[s3.index("static void station_beacon"):s3.index("/* ---------------------------------------------------------------- scene")]

assert "y0 = cy - rows * 8;" in s3
assert "if(cols > 4 && rows > 1) y0 &=" in s3
assert "scan();                         /* correct docking/boost state on the very first frame */" in fly
assert "return 4;                   /* fifth distinct biome" in s3
assert "tgt_rear" in s3 and "BEHIND!" in boss
# Aim-assist shots are kept in a signed 16-bit fixed-point format.
# Directly doing 400 * 90 overflows the GBC's 16-bit int and reversed shots.
assert "while(m > 255){ dx /= 2; dy /= 2; dz /= 2; m = (m + 1) / 2; }" in fly
def normalized(dx, dy, dz, speed):
    m = max(abs(dx), abs(dy), abs(dz))
    if not m:
        return 0, 0, 0
    while m > 255:
        dx = int(dx / 2); dy = int(dy / 2); dz = int(dz / 2)
        m = (m + 1) // 2
    assert max(abs(dx), abs(dy), abs(dz)) * speed <= 32767
    return tuple(int(c * speed / m) for c in (dx,dy,dz))
for target in ((400,0,0),(-480,0,0),(0,450,0),(250,-420,120),(-500,-350,200)):
    v = normalized(*target,90)
    assert any(v)
    for k in range(3):
        if target[k]: assert (v[k] >= 0) == (target[k] >= 0) or v[k] == 0
        assert abs(v[k]) <= 90

# The 3D mode must support the actual endgame destination, not simply
# omit the Beast option and force an undocumented trip back to 2D.
assert "galaxy_run(cell, sv.cores >= 8 && !(sv.story & ST_DONE))" in fly
assert "r == GAL_BEAST ? 2100 : 1500" in fly
assert "p->hp = BEAST_HP" in boss
assert "for(i = 0; i < NP3; i++) pir[i].st = 0;" in boss
assert "for(i = 0; i < NO3; i++) orb[i].life = 0;" in boss
assert "p = &pir[0];" in boss
assert "p->x = (UINT16)BEAST_X << 4" in boss
assert "p->kind == 2 && (p->t & 31) >= 18" in fly
assert "story_boss_state = 2; story_boss_fade = 80;" in fly
assert "sv.story |= ST_DONE;" in boss
assert all(t in boss for t in ("sv.gold += 5000;", "fame_add(300);", "gain_xp(200);"))
assert "space_say(SAY_DAWN);" in fly
assert "space_say(SAY_DAWN);" not in boss
assert "if(story_boss_finish()){ out = SP_END; break; }" in fly
assert "THE TECHNOBEAST LIVES IN THE FLAT VIEW" not in warp
assert "THE TECHNOBEAST IS HERE IN 3D TOO" in warp
assert "B AND DOWN BRAKES" in warp
assert "THE AMBER MARK SHOWS A STATION" in warp
assert "if(dist > 700 && !arena_on && p->kind != 2)" in fly
assert "if(story_boss_state == 1) s3_boss_arrow();" in fly
assert "p->kind == 2 ? ((p->t & 31) < 18 ? PAL_LAS : PAL_ORB)" in s3
assert "else if(story_boss_state && edge_on) edge_mark(" in s3
assert "#pragma bank 53" in boss
assert "void s3_boss_spawn(void) BANKED" in boss
assert "void s3_boss_arrow(void) BANKED" in boss
assert "void s3_boss_hud(void) BANKED" in boss
assert "void s3_boss_reward(void) BANKED" in boss
assert "s3_boss_hud(); return;" in boss
assert "s3_boss_reward();" in fly
assert "story_boss_state = 0; story_boss_fade = 0; edge_on = 0;" in fly
assert "static void story_boss_spawn(void)" not in fly
# The CPU's most frequently drawn cosmetic particles stay in bank 18:
# moving them here also fixes bank 19's real 16-KB overflow without
# allocating new RAM or adding another bank switch for each dust rebirth.
assert "#pragma bank 18" in s3
assert "void s3_dust_step(void) BANKED" in s3
assert "void s3_dust_reset(void) BANKED" in s3
assert "s3_dust_reset();" in fly
assert fly.count("s3_dust_step();") == 2  # space and arena
assert "static void step_dust(void)" not in fly
assert "void s3_target_hud(void) BANKED" in boss
assert "void s3_land(UINT8 s) BANKED" in boss
assert "void s3_wrecked(void) BANKED" in boss
assert "s3_target_hud();" in fly
assert "s3_land(near_pl);" in fly and "s3_wrecked();" in fly
assert "static void line3(void)" not in fly
assert "static void wrecked(void)" not in fly
assert "static void land(UINT8 s)" not in fly
assert "yaw_o = yaw; pit_o = pit16;" in s3

# At a 3D Beast-map jump the ship is 150 units south and initially faces
# north; 150 is inside the 195-unit aim-assist limit, unlike old far locks.
assert 24 < 150 < 195

assert "s3_render(frame);" in fly and "s3_flush(frame);" in fly
print("GBC 3D space: seamless yaw, 2D/3D station parity, 8-way heading, brake, dock and sprite-budget checks passed")
