#!/usr/bin/env python3
"""Headless GBC test runner (Walnut-CGB, tools/emu/gbflick.c) with a tiny key language and a flicker report.
usage: fl.py ROM SAVE NAME "script" [--keep]
script tokens (space separated):  wN wait N frames | A B S s U D l r press for 3 frames, release 4 | hKEYS:N hold keys N frames (e.g. hD:30)
  shot = screenshot now | [ and ] = an idle window: reported frame by frame for changes | #text ignored
Outputs: out/NAME_*.ppm -> NAME_sheet.png (all shots, 2x) and a flicker line per idle window:
  idle frames, frames whose picture changed, max changed pixels in one frame, tile-map cells written during the visible lines (act) and in vblank (vbl).
act should be 0 on a still menu; blink animations show as a few pixels every 16 frames."""
import sys, os, subprocess, glob
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
EMU = os.environ.get('GBFLICK', '/home/claude/tools/walnut/gbflick_new')
def main():
    rom, sav, name, script = sys.argv[1:5]
    out = os.environ.get('OUT', '/tmp/claude-0/-home-claude/abae1cef-5a19-5cae-b51b-a66fc6d2b543/scratchpad/out'); os.makedirs(out, exist_ok=True)
    for f in glob.glob('%s/%s_*' % (out, name)): os.remove(f)
    ev = []; shots = []; marks = []; fr = 0
    for t in script.split():
        if t[0] == '#': continue
        if t == 'shot': shots.append(fr); continue
        if t == '[': marks.append(['open', fr]); continue
        if t == ']': marks[-1][0] = 'closed'; marks[-1].append(fr); continue
        if t[0] == 'w': fr += int(t[1:]); continue
        if t[0] == 'h': k, n = t[1:].split(':'); ev.append((fr, fr + int(n), k)); fr += int(n) + 1; continue
        ev.append((fr, fr + 3, t)); fr += 7
    last = fr + 2
    sc = '%s/%s.txt' % (out, name)
    with open(sc, 'w') as f:
        for a, b, k in ev: f.write('%d %d %s\n' % (a, b, k))
        for s in shots: f.write('shot %d\n' % s)
        f.write('shot %d\n' % last)
    sv = '%s/%s.sav' % (out, name)
    if os.path.exists(sav): open(sv, 'wb').write(open(sav, 'rb').read())
    env = dict(os.environ, FLICK='1')
    r = subprocess.run([EMU, rom, sc, '%s/%s' % (out, name), sv], capture_output=True, text=True, env=env)
    if r.returncode: print('emulator exit', r.returncode, r.stderr[:300])
    rows = {}
    for l in r.stdout.splitlines():
        p = l.split()
        if p and p[0] == 'f': rows[int(p[1])] = (int(p[3]), int(p[5]), int(p[7]))
    for m in marks:
        if len(m) < 3: continue
        a, b = m[1], m[2]; fs = [rows.get(i, (0, 0, 0)) for i in range(a, b + 1)]
        if os.environ.get('ROWS'): print('   frames:', ' '.join('%d:%d/%d' % (a+i, x[2], x[0]) for i, x in enumerate(fs) if x[2] or x[0]))
        ch = sum(1 for x in fs if x[2]); mx = max((x[2] for x in fs), default=0)
        print('%-10s idle frames %d..%d: picture changed in %d frames, max %d px; map cells written: visible %d, vblank %d' % (name, a, b, ch, mx, sum(x[0] for x in fs), sum(x[1] for x in fs)))
    ims = []
    for f in sorted(glob.glob('%s/%s_*.ppm' % (out, name))): ims.append(Image.open(f).convert('RGB'))
    if ims:
        cols = min(4, len(ims)); rws = (len(ims) + cols - 1) // cols
        sh = Image.new('RGB', (cols * 324, rws * 292), (60, 0, 60))
        for i, im in enumerate(ims): sh.paste(im.resize((320, 288), Image.NEAREST), ((i % cols) * 324, (i // cols) * 292))
        sh.save('%s/%s_sheet.png' % (out, name)); print('sheet:', '%s/%s_sheet.png' % (out, name), len(ims), 'shots')
    if os.environ.get('SAVEOUT'): open(os.environ['SAVEOUT'], 'wb').write(open(sv, 'rb').read())
main()
