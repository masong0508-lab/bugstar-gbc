#!/usr/bin/env python3
"""Exercise the real GBC kart drive function for speed caps and symmetry.

Audio, items and jumps are isolated here; check_kart_items.py tests actual
item behavior separately. The production curve and position functions,
controls, rival tables and drive code are compiled unchanged under UBSan.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/kart.c").read_text()


def function(signature):
    match = re.search(r"\b" + re.escape(signature) + r"\s*\{", source)
    assert match, f"missing real kart routine: {signature}"
    depth = 0
    for pos in range(match.end() - 1, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if not depth:
                return source[match.start():pos + 1]
    raise AssertionError(f"unterminated kart routine: {signature}")


test = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"
#include "kart.h"
#include "sound.h"
#define LAPM 2048U
#define VMAX 256
#define KX_GAS 0
UINT8 keys, keys_old, keys_rep, kart_body, rivals_on, air_t;
static UINT8 trk, boost_t, drift_t, hold, stun[1 + NRIV], bump_cd, stripe, gh_on;
static UINT16 tm[1 + NRIV], stripe_acc, ghv;
static UINT8 tf[1 + NRIV];
static INT16 vel[1 + NRIV], lx[1 + NRIV], head;
static INT8 bends[16];
static const INT8 *sg = bends;
void sfx(UINT8 n){ (void)n; }
UINT8 kart_air(UINT16 pm, INT16 x, INT16 v, UINT8 b){
    (void)pm; (void)x; (void)v; (void)b; return 0;
}
static void use_item(void){}
static void items_tick(UINT8 go){ (void)go; }
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"Kart control regression at %d: %s\n",__LINE__,#x); exit(1); } }while(0)
"""
for name in ("tdif", "wob", "lane", "vrv"):
    match = re.search(r"static const (?:UINT8|INT8) " + name + r"\[[^\]]+\] = \{[^}]+\};", source)
    assert match, f"missing production rival table {name}"
    test += "\n" + match.group()
for signature in (
    "static void add_pos(UINT8 k, INT16 v)",
    "static INT16 curve_at(UINT16 m, UINT8 fr)",
    "static void drive(UINT8 go)",
):
    test += "\n" + function(signature)

test += r"""
static void reset(int curve, int lane0, int speed, int body, int boost, int throttle, int brake, int steer){
    unsigned i;
    memset(tm, 0, sizeof tm); memset(tf, 0, sizeof tf);
    memset(vel, 0, sizeof vel); memset(lx, 0, sizeof lx); memset(stun, 0, sizeof stun);
    for(i = 0; i < 16; i++) bends[i] = (INT8)curve;
    lx[0] = lane0; vel[0] = speed; kart_body = body; boost_t = boost;
    keys = (throttle ? J_A : 0) | (brake ? J_DOWN : 0) |
           (steer < 0 ? J_LEFT : steer > 0 ? J_RIGHT : 0);
    keys_old = keys; rivals_on = air_t = trk = gh_on = hold = drift_t = bump_cd = stripe = 0;
    head = 0; stripe_acc = 0;
}
int main(void){
    int curve, lane0, speed, body, boost, throttle, brake, steer, n;
    unsigned long cases = 0;

    /* Repeated turbo acceleration must settle at 400, not add four per tick. */
    reset(0, 0, 400, 0, 1, 1, 0, 0);
    for(n = 0; n < 240; n++){ drive(1); CHECK(vel[0] == 400); }
    reset(-28, 180, 0, 0, 0, 1, 0, -1); drive(1);
    CHECK(lx[0] == 179 && head == 0);  /* low-speed recovery on a left bend */
    reset(28, -180, 0, 0, 0, 1, 0, 1); drive(1);
    CHECK(lx[0] == -179 && head == 0);

    /* Acceleration below a cap retains the body/turbo's existing increments. */
    for(body = 0; body < 4; body++) for(boost = 0; boost < 2; boost++){
        int accel = boost ? 12 : body == 2 ? 7 : 4;
        int cap = boost ? 400 : body == 1 ? 296 : 256;
        reset(0, 0, 0, body, boost, 1, 0, 0);
        for(n = 1; n <= 8; n++){ drive(1); CHECK(vel[0] == n * accel); }
        for(n = 0; n < 240; n++) drive(1);
        CHECK(vel[0] == cap);
        keys = 0; drive(1); CHECK(vel[0] == cap - 2);
    }
    /* Entering grass or losing boost slows smoothly even while A stays down. */
    for(boost = 0; boost < 2; boost++){
        int cap = boost ? 140 : 70, previous = 400;
        reset(0, 180, 400, 0, boost, 1, 0, 0);
        for(n = 0; n < 60; n++){
            drive(1);
            CHECK(vel[0] == (previous - cap > 8 ? previous - 8 : cap));
            previous = vel[0];
        }
    }
    reset(0, 0, 400, 0, 0, 1, 0, 0);
    for(n = 0; n < 18; n++){ drive(1); CHECK(vel[0] == 400 - (n + 1) * 8); }
    drive(1); CHECK(vel[0] == 256);

    /* Exhaustive valid speed/curve range, all bodies, throttle/brake states,
       steering inputs and road/grass sides. Mirroring must preserve motion. */
    for(body = 0; body < 4; body++) for(boost = 0; boost < 2; boost++)
    for(curve = 0; curve <= 30; curve++) for(speed = 0; speed <= 400; speed++)
    for(lane0 = -180; lane0 <= 180; lane0 += 180)
    for(throttle = 0; throttle < 2; throttle++) for(brake = 0; brake < 2; brake++)
    for(steer = -1; steer <= 1; steer++){
        int x, heading, velocity, metres, fraction;
        int cap = lane0 ? (boost ? 140 : 70) : boost ? 400 : body == 1 ? 296 : 256;
        reset(curve, lane0, speed, body, boost, throttle, brake, steer);
        drive(1);
        x = lx[0]; heading = head; velocity = vel[0]; metres = tm[0]; fraction = tf[0];
        CHECK(velocity >= 0 && velocity <= (speed > cap + 8 ? speed - 8 : cap));
        CHECK(x >= -200 && x <= 200);
        reset(-curve, -lane0, speed, body, boost, throttle, brake, -steer);
        drive(1);
        CHECK(lx[0] == -x && head == -heading && vel[0] == velocity);
        CHECK(tm[0] == metres && tf[0] == fraction);
        cases++;
    }
    /* The existing 16-bit scenery heading continues wrapping in both ways. */
    reset(30, 0, 400, 0, 1, 1, 0, 0); head = 32760; drive(1); CHECK(head == -32765);
    reset(-30, 0, 400, 0, 1, 1, 0, 0); head = -32760; drive(1); CHECK(head == 32765);
    reset(-30, 180, 400, 0, 1, 1, 0, -1); drive(0);
    CHECK(!vel[0] && lx[0] == 180 && !head && !tm[0] && !tf[0]);
    printf("GBC kart control: %lu mirrored cases, turbo caps, surface slowdown, recovery and heading wrap PASS\n", cases);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-gbc-kart-control-") as tmp:
    cpath, executable = Path(tmp) / "test.c", Path(tmp) / "test"
    cpath.write_text(test)
    subprocess.run([
        "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
        "-Wno-misleading-indentation",
        "-fsanitize=undefined", "-fno-sanitize-recover=all",
        "-I" + str(root / "tests/stub"), "-I" + str(root / "src"),
        str(cpath), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
