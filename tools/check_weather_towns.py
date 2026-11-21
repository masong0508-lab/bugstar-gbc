#!/usr/bin/env python3
"""Runtime and source regressions for GBC weather, town dialogue and errands.

Extracts the actual C functions, compiles with strict warnings + UBSan, and
checks every nonzero xorshift8 seed. ROM GBDK and boot tests run separately.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
weather = (root / "src/weather.c").read_text()
town2 = (root / "src/town2.c").read_text()
town4 = (root / "src/town4.c").read_text()


def function(source, sig):
    match = re.search(r"\b" + re.escape(sig) + r"\s*\{", source)
    assert match, f"missing authentic C routine {sig}"
    depth = 0
    for pos in range(match.end() - 1, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():pos + 1]
    raise AssertionError(f"unterminated C routine {sig}")


def check_tables(source, names):
    for name in names:
        # The GBC board renders 18 characters and needs the 19th byte for
        # termination. A 19-character initializer silently drops that NUL.
        m = re.search(r"static const char\s+" + name +
                      r"\s*\[[^\n=]+\]\s*=\s*\{(.*?)\};", source, re.S)
        assert m, f"missing dialogue table {name}"
        strings = re.findall(r'"([^"]*)"', m.group(1))
        assert strings, name
        long = [(s, len(s)) for s in strings if len(s) > 18]
        assert not long, f"{name}: unterminated or oversized strings {long}"


check_tables(town2, ["lore"])
check_tables(town4, ["chat", "mood"])
assert "static UINT8 rnd(void)" in weather
assert "wx_rs >> 9" not in weather and "wx_rs << 8" not in weather
assert "UINT16 delta = (UINT16)(EX_KILLS -" in town4

test_c = r"""
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef struct { UINT8 xr[20]; } XA;
static UINT8 wx_rs=1, saved_fish=0;
static UINT16 kill_total=0;
static struct {
    UINT8 potions, star;
    struct { UINT8 cargo[8]; } el;
} sv;
static struct { UINT8 fish; } ex;
#define G_ORE 0
#define EX_KILLS kill_total
#define E_TYPE xr[8]
#define E_N xr[9]
#define E_BASE 10
static UINT16 w16(const UINT8 *p){ return p[0] | ((UINT16)p[1]<<8); }
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"GBC regression at %d: %s\n",__LINE__,#x); return 1; } }while(0)
"""
test_c += "\n" + function(weather, "static UINT8 rnd(void)")
test_c += "\n" + function(town4, "static UINT8 e_have(const XA *x, UINT8 t)")
test_c += r"""
static void setbase(XA *x, UINT16 base){ x->xr[10]=(UINT8)base; x->xr[11]=(UINT8)(base>>8); }
int main(void){
    unsigned seed, iter, unique=0; unsigned char visit[256];
    for(seed=1;seed<256;seed++){
        UINT8 val=(UINT8)seed;
        for(iter=0;iter<256;iter++) visit[iter]=0;
        wx_rs=val;
        for(iter=0;iter<255;iter++){
            CHECK(wx_rs!=0);
            CHECK(!visit[wx_rs]);
            visit[wx_rs]=1;
            rnd();
        }
        CHECK(wx_rs==val);
        unique++;
    }
    CHECK(unique==255);
    XA x={{0}}; x.xr[8]=4; x.xr[9]=5;
    setbase(&x,65533U); kill_total=65533U; CHECK(e_have(&x,4)==0);
    kill_total=65535U; CHECK(e_have(&x,4)==2);
    kill_total=0; CHECK(e_have(&x,4)==3);
    kill_total=2; CHECK(e_have(&x,4)==5);
    kill_total=300; CHECK(e_have(&x,4)==255);
    sv.potions=3; CHECK(e_have(&x,1)==3);
    sv.el.cargo[0]=4; CHECK(e_have(&x,2)==4);
    ex.fish=7; CHECK(e_have(&x,5)==7);
    sv.star=42; x.xr[9]=42; CHECK(e_have(&x,3)==1);
    x.xr[9]=13; CHECK(e_have(&x,3)==0);
    puts("GBC weather: 255 seeds x 255-step full cycles; townsfolk kill rollover and goods PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="gbc-weather-town-") as d:
    path = Path(d) / "test.c"
    binpath = Path(d) / "test"
    path.write_text(test_c)
    subprocess.run([
        "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=undefined", "-fno-sanitize-recover=all",
        str(path), "-o", str(binpath),
    ], check=True)
    subprocess.run([str(binpath)], check=True)
print("GBC library and townsfolk dialogue: every fixed-size 19-byte entry NUL-safe; PASS")
