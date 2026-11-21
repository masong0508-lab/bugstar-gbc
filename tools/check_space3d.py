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
assert "near_pl == 255 && near_st == 255) shoot();" in fly
assert "if(near_st != 255 && !dock_lock)" in fly

# The same 40-sprite OAM allocation is retained; nearer worlds beat
# far-away clutter and palettes no longer recolor active objects.
assert "static Cand cand[16]" in s3
assert "if(i >= 15)" in s3 and "if(z >= far) return;" in s3
assert "pal_used |= 1 << i" in s3
assert "y0 = cy - rows * 8;" in s3
assert "if(cols > 4 && rows > 1) y0 &=" in s3
assert "scan();                         /* correct docking/boost state on the very first frame */" in fly
assert "return 4;                   /* fifth distinct biome" in s3
assert "tgt_rear" in s3 and "BEHIND!" in fly
assert "s3_render(frame);" in fly and "s3_flush(frame);" in fly
print("GBC 3D space: seamless yaw, 2D/3D station parity, 8-way heading, brake, dock and sprite-budget checks passed")
