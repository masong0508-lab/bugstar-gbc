#!/usr/bin/env python3
"""Menu flicker suite: runs every menu on a ROM and prints, for each idle stretch, how many frames changed on screen and how many tile-map cells
were written. usage: suite.py ROM [TAG] [only-substring]   (needs tools/emu/gbflick.c built, see fl.py; saves are made with mksave.py from $BASE_SAV)"""
import sys, os, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
P = os.environ.get('SCRATCH', '/tmp/claude-0/-home-claude/abae1cef-5a19-5cae-b51b-a66fc6d2b543/scratchpad')
BASE = os.environ.get('BASE_SAV', P + '/s1.sav')
ROM = sys.argv[1]; TAG = sys.argv[2] if len(sys.argv) > 2 else 'new'; ONLY = sys.argv[3] if len(sys.argv) > 3 else ''
def mk(name, **kw):
    out = '%s/su_%s.sav' % (P, name)
    subprocess.run([sys.executable, HERE + '/mksave.py', BASE, out] + ['%s=%s' % kv for kv in kw.items()], check=True)
    return out
def run(name, sav, script):
    if ONLY and ONLY not in name: return
    r = subprocess.run([sys.executable, HERE + '/fl.py', ROM, sav, '%s_%s' % (TAG, name), script], capture_output=True, text=True)
    sys.stdout.write(r.stdout + r.stderr)
T = 'w300 '                      # the title
cont = T + 'hA:8 w250 '          # CONTINUE: on the planet
rich = dict(level=30, gold=9000, story=7, potions=3)
s = mk('rich', **rich)
# the title's options page
run('options', s, T + 'D D D hA:8 w60 shot [ w90 ] D w3 [ w20 ] hD:40 shot hR:3 w10 shot [ w20 ] hB:8 w60 shot')
run('load', s, T + 'D D hA:8 w120 shot [ w60 ] D w5 shot [ w20 ] hB:8 w60')
# the SYSTEM menu on the planet and what is behind it
run('system', s, cont + 'hS:6 w60 shot [ w60 ] D w5 [ w30 ] hD:30 shot hS:6 w60')
run('status', s, cont + 'hS:6 w60 D w6 hA:6 w60 shot [ w60 ] hB:6 w30 shot')
run('options_sys', s, cont + 'hS:6 w60 D D D hA:6 w60 shot [ w40 ] D w4 [ w20 ] hR:3 w6 shot hB:6 w60 shot')
run('skills', s, cont + 'hS:6 w60 D D D D D hA:6 w60 shot [ w40 ] D w5 [ w20 ] hA:3 w6 shot hB:6 w60 shot')
run('feats', s, cont + 'hS:6 w60 D D D D D D D hA:6 w60 shot [ w40 ] hB:6 w60 shot')
run('guide', s, cont + 'hS:6 w60 D D D D D D D D hA:6 w120 shot [ w40 ] hR:3 w10 shot [ w20 ] hR:3 w10 shot hB:6 w80 shot')
run('extras', s, cont + 'hS:6 w60 D D D D D D hA:6 w120 shot [ w40 ] D w5 shot [ w20 ] hD:30 w5 shot hB:6 w60')
