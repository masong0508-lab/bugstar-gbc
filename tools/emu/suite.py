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
def run(name, sav, script, saveout=None):
    if ONLY and ONLY not in name: return
    env = dict(os.environ); 
    if saveout: env['SAVEOUT'] = saveout                         # the save the emulator ends with (a power cycle: the next run starts from it)
    r = subprocess.run([sys.executable, HERE + '/fl.py', ROM, sav, '%s_%s' % (TAG, name), script], capture_output=True, text=True, env=env)
    sys.stdout.write(r.stdout + r.stderr)
T = 'w300 '                      # the title
cont = T + 'hA:8 w250 ' + 'hA:3 w50 ' * 14 + 'w60 '          # CONTINUE: on the planet (intro dialog pages dismissed)
rich = dict(level=30, gold=9000, story=7, potions=3)
s = mk('rich', **rich)
# the title's options page
run('options', s, T + 'D D D hA:8 w60 shot [ w90 ] D w3 [ w20 ] hD:40 shot hr:3 w10 shot [ w20 ] hB:8 w60 shot')
run('load', s, T + 'D D hA:8 w120 shot [ w60 ] D w5 shot [ w20 ] hB:8 w60')
# the SYSTEM menu on the planet and what is behind it
run('system', s, cont + 'hS:6 w60 shot [ w60 ] D w5 [ w30 ] hD:30 shot hS:6 w60')
run('status', s, cont + 'hS:6 w60 D w6 hA:6 w60 shot [ w60 ] hB:6 w30 shot')
run('options_sys', s, cont + 'hS:6 w60 D D D hA:6 w60 shot [ w40 ] D w4 [ w20 ] hr:3 w6 shot hB:6 w60 shot')
run('skills', s, cont + 'hS:6 w60 D D D D D hA:6 w60 shot [ w40 ] D w5 [ w20 ] hA:3 w6 shot hB:6 w60 shot')
run('feats', s, cont + 'hS:6 w60 D D D D D D D hA:6 w60 shot [ w40 ] hB:6 w60 shot')
run('guide', s, cont + 'hS:6 w60 D D D D D D D D hA:6 w120 shot [ w40 ] hr:3 w10 shot [ w20 ] hr:3 w10 shot hB:6 w80 shot')
run('extras', s, cont + 'hS:6 w60 D D D D D D hA:6 w120 shot [ w40 ] D w5 shot [ w20 ] hD:30 w5 shot hB:6 w60')
# BUG HIVE. Keys: START is 'S' (hold it a few frames), SELECT is 's'; the cursor starts on the queen's tunnel, so A would open the build menu: leave it with B first.
HIVE = cont + 'hS:6 w60 D D D D D D hA:6 w120 ' + 'D w4 ' * 12 + 'hA:6 w200 '          # extras card 12, A: the intro page
HNEW = HIVE + 'shot hA:6 w60 hA:6 w60 hA:6 w120 shot ' + 'hr:3 w10 ' * 5 + 'shot hB:6 w90 '  # intro, world, difficulty, then the help pages the first time
# 1. a new colony: the map idle (the flicker numbers), mark three cells to dig and idle while the workers dig them, the menu, its pages, leave
run('hive', s, HNEW + 'shot [ w300 ] D w4 D w4 D w4 D w4 hA:6 w30 shot [ w900 ] shot hS:6 w60 shot [ w120 ] ' + 'D w4 ' * 8 + 'hA:6 w60 shot hB:6 w30 D w4 D w4 hA:6 w200 shot', P + '/su_hive1.sav')
# 2. the second visit: the same save, a power cycle, CONTINUE, the hive loads (no intro, no help), the map is the same colony
run('hive_again', P + '/su_hive1.sav', cont + 'hS:6 w60 D D D D D D hA:6 w120 ' + 'D w4 ' * 12 + 'hA:6 w200 shot [ w200 ] hS:6 w60 shot hB:6 w30', P + '/su_hive2.sav')
# 3. time away: 1500 seconds of play have gone by (more than the 10 days the colony can catch up)
if not ONLY or ONLY in 'hive_away' or 'hive_away'.startswith(ONLY): subprocess.run([sys.executable, HERE + '/mksave.py', P + '/su_hive2.sav', P + '/su_hive_away.sav', 'secs+=1500'], check=True)
run('hive_away', P + '/su_hive_away.sav', cont + 'hS:6 w60 D D D D D D hA:6 w120 ' + 'D w4 ' * 12 + 'hA:6 w300 shot w150 shot hA:6 w60 shot [ w200 ] hS:6 w60 D w4 D w4 D w4 D w4 D w4 D w4 D w4 hA:6 w60 shot hB:6 w30', P + '/su_hive3.sav')
