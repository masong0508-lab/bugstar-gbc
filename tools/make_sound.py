#!/usr/bin/env python3
"""BUGSTAR GBC music data (step 9). Writes src/sound_data.c + src/sound_data.h (bank 7, read by src/sound_play.c).

The tunes are the GBA game's (sound.inc): a 64-step melody and bass per track, note numbers are MIDI (0 = rest, 1 = hold).
The GBC adds a drum line per track (16 steps, looped) and an echo of the melody on channel 1 (see sound_play.c).
Frequency tables: square channels f = 131072 / (2048 - x), the wave channel f = 65536 / (2048 - x).

Run:  python3 tools/make_sound.py
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_, H = 0, 1

TRACKS = {  # name: (melody, bass, tempo in frames per step, melody volume 0..15, drums)
    "TITLE": ([72,H,76,H,79,H,84,H, H,H,83,H,79,H,76,H, 77,H,81,H,84,H,89,H, H,H,88,H,84,H,81,H,
               79,H,H,H,77,H,76,H, 74,H,H,H,72,H,74,H, 76,H,H,H,79,H,H,H, 72,H,H,H,H,H,_,_],
              [48,H,H,H,H,H,H,H, 52,H,H,H,H,H,H,H, 53,H,H,H,H,H,H,H, 57,H,H,H,H,H,H,H,
               55,H,H,H,H,H,H,H, 53,H,H,H,H,H,H,H, 55,H,H,H,H,H,H,H, 48,H,H,H,H,H,H,H], 8, 11, "k...s...k.k.s..h"),
    "PLANET": ([76,H,79,H,81,H,79,76, 74,H,72,H,74,H,H,_, 76,H,79,H,84,H,81,79, 76,H,H,_,_,_,_,_,
                77,H,81,H,84,H,81,77, 76,H,74,H,72,H,H,_, 74,H,76,H,79,H,76,74, 72,H,H,H,_,_,_,_],
               [48,H,H,H,55,H,H,H, 45,H,H,H,52,H,H,H, 48,H,H,H,55,H,H,H, 43,H,H,H,50,H,H,H,
                41,H,H,H,48,H,H,H, 43,H,H,H,50,H,H,H, 45,H,H,H,52,H,H,H, 43,H,H,H,47,H,H,H], 9, 10, "k.h.s.h.k.hkh.s."),
    "DUNGEON": ([69,H,H,_,72,H,_,_, 76,H,H,H,74,H,72,_, 71,H,H,_,72,H,74,_, 72,H,H,H,H,_,_,_,
                 _,_,64,H,65,H,_,_, 69,H,H,_,68,H,H,_, 71,H,_,_,69,H,_,68, 69,H,H,H,H,H,_,_],
                [45,H,H,H,H,H,H,H, 41,H,H,H,H,H,H,H, 43,H,H,H,H,H,H,H, 40,H,H,H,H,H,H,H,
                 45,H,H,H,H,H,H,H, 41,H,H,H,H,H,H,H, 44,H,H,H,H,H,H,H, 45,H,H,H,40,H,H,H], 12, 8, "k.......s.....h."),
    "SPACE": ([69,H,72,H,76,H,72,H, 69,H,72,H,76,H,79,H, 77,H,74,H,69,H,74,H, 77,H,H,H,76,H,74,H,
               72,H,76,H,79,H,76,H, 72,H,76,H,79,H,84,H, 83,H,79,H,76,H,74,H, 76,H,H,H,H,H,_,_],
              [45,H,H,H,H,H,H,H, 45,H,H,H,52,H,H,H, 50,H,H,H,H,H,H,H, 50,H,H,H,57,H,H,H,
               48,H,H,H,H,H,H,H, 48,H,H,H,55,H,H,H, 43,H,H,H,H,H,H,H, 40,H,H,H,47,H,H,H], 11, 9, "k...h...s...h..h"),
    "BOSS": ([64,H,67,_,64,H,71,_, 70,H,67,_,64,H,62,H, 64,H,67,_,64,H,72,_, 71,H,67,_,66,H,H,_,
              76,H,74,H,72,H,71,H, 72,H,71,H,69,H,67,H, 69,H,71,H,72,H,74,H, 75,H,H,H,71,H,H,_],
             [40,_,40,_,52,_,40,_, 40,_,40,_,52,_,40,_, 36,_,36,_,48,_,36,_, 35,_,35,_,47,_,35,_,
              40,_,40,_,52,_,40,_, 38,_,38,_,50,_,38,_, 36,_,36,_,48,_,36,_, 35,_,35,_,47,_,47,_], 6, 10, "k.hks.hkk.hks.ss"),
    "ENDING": ([72,H,H,H,74,H,76,H, 79,H,H,H,76,H,H,H, 77,H,H,H,76,H,74,H, 72,H,H,H,H,H,_,_,
                69,H,H,H,72,H,74,H, 76,H,H,H,79,H,H,H, 77,H,76,H,74,H,71,H, 72,H,H,H,H,H,H,_],
               [48,H,H,H,H,H,H,H, 52,H,H,H,H,H,H,H, 53,H,H,H,H,H,H,H, 48,H,H,H,55,H,H,H,
                45,H,H,H,H,H,H,H, 52,H,H,H,H,H,H,H, 50,H,H,H,43,H,H,H, 48,H,H,H,H,H,H,H], 12, 10, "k.......s......."),
}


# ------------------------------------------------------------------ the composer: a 256-step tune (16 bars) per kind of world
import random
SCALES = {"major": [0, 2, 4, 5, 7, 9, 11], "minor": [0, 2, 3, 5, 7, 8, 10], "dorian": [0, 2, 3, 5, 7, 9, 10], "lydian": [0, 2, 4, 6, 7, 9, 11],
          "mixo": [0, 2, 4, 5, 7, 9, 10], "phryg": [0, 1, 4, 5, 7, 8, 10], "locrian": [0, 1, 3, 5, 6, 8, 10]}
RHYTHMS = {   # onsets inside a 16-step bar
    "calm":  [[0, 4, 8, 12], [0, 6, 8, 12], [0, 4, 8, 10, 12], [0, 8, 10, 12], [0, 3, 6, 8, 12]],
    "lively": [[0, 2, 4, 6, 8, 10, 12, 14], [0, 3, 4, 6, 8, 11, 12, 14], [0, 2, 4, 8, 10, 12], [0, 4, 6, 8, 12, 14], [0, 3, 6, 8, 10, 12, 14]],
    "sparse": [[0, 8], [0, 6, 12], [0, 10], [0, 4, 12], [0, 8, 14]],
}


def compose(seed, root, scale, progA, progB, style, bass_style, lowmel=False):
    R = random.Random(seed)
    sc = SCALES[scale]
    def deg_note(d, octv=0): return root + 12 * (d // 7 + octv) + sc[d % 7]
    def snap(m, lo, hi):
        while m < lo: m += 12
        while m > hi: m -= 12
        return m
    mel, bas = [], []
    base_oct = 0 if lowmel else 1
    sections = [("A", progA, 0), ("A2", progA, 0), ("B", progB, 1), ("A3", progA, 0)]
    motif = None
    for si, (name, prog, lift) in enumerate(sections):
        cur = 4 + R.randrange(3)                    # a scale degree (counted from the root octave + base)
        for bi, chord in enumerate(prog):
            rh = RHYTHMS[style][(R.randrange(len(RHYTHMS[style])) if (bi % 2 == 0 and not (name != "B" and bi == 2 and motif)) else (motif or [0])[0])] if False else None
            if bi % 2 == 0:
                rh_i = R.randrange(len(RHYTHMS[style])); motif_r = rh_i
            else:
                rh_i = motif_r if R.random() < 0.7 else R.randrange(len(RHYTHMS[style]))
            onsets = RHYTHMS[style][rh_i]
            tones = [chord, chord + 2, chord + 4]                         # degrees of the chord tones
            bar = [0] * 16
            for oi, o in enumerate(onsets):
                strong = o % 8 == 0
                cands = [d for d in range(chord - 2, chord + 8) if (d % 7) in [t % 7 for t in tones]] if strong else list(range(chord - 1, chord + 8))
                cands = [d for d in cands if d + 7 * base_oct >= 0]
                best = min(cands, key=lambda d: abs(d - cur) + R.random() * 1.5)
                step_lim = best if abs(best - cur) <= 3 else cur + (3 if best > cur else -3)
                cur = step_lim
                if bi == 3 and oi == len(onsets) - 1 and si in (1, 3): cur = chord + (0 if si == 3 else 2)      # cadence: land on the chord
                m = snap(deg_note(cur, base_oct + lift), 60 if not lowmel else 52, 88)
                bar[o] = m
            line = []
            for i in range(16):
                if bar[i]: line.append(bar[i])
                else:
                    line.append(H if (line and line[-1] != _) else _)
            # let the last note of a bar breathe: a rest before the next bar on sparse styles
            if style == "sparse": line[15] = _
            mel += line
            r = deg_note(chord, -2) if not lowmel else deg_note(chord, -2)
            r = snap(r, 36, 52); f = snap(r + 7, 36, 57)
            if bass_style == "hold": b = [r] + [H] * 15
            elif bass_style == "fifth": b = [r] + [H] * 7 + [f] + [H] * 7
            elif bass_style == "pulse": b = [r, H, r, H, f, H, r, H, r, H, r, H, f, H, f, H]
            elif bass_style == "walk": b = [r, H, H, H, snap(r + 2, 36, 57), H, H, H, f, H, H, H, snap(r + 4, 36, 57), H, H, H]
            else: b = [r] + [H] * 15
            bas += b
    return mel, bas


NEW_TRACKS = {  # name: (seed, root, scale, progA, progB, rhythm style, bass, tempo, vol, drums, low melody)
    "P_DESERT": (11, 64, "phryg", [0, 0, 1, 0], [3, 4, 1, 0], "calm", "fifth", 10, 10, "k.h.s.h.k.h.s.hh", False),
    "P_ICE":    (12, 62, "lydian", [0, 1, 5, 0], [3, 4, 5, 1], "sparse", "hold", 12, 9, "k.......s.......", False),
    "P_JUNGLE": (13, 55, "mixo", [0, 6, 3, 0], [3, 4, 6, 0], "lively", "pulse", 7, 10, "k..ks.h.k.k.s.hh", False),
    "P_OCEAN":  (14, 57, "dorian", [0, 3, 0, 6], [5, 3, 6, 0], "calm", "hold", 12, 10, "k.......s.....h.", False),
    "P_DUSK":   (15, 60, "minor", [0, 5, 2, 6], [3, 5, 6, 0], "calm", "walk", 10, 10, "k...s...k.h.s...", False),
    "D_DESERT": (21, 52, "phryg", [0, 1, 0, 6], [3, 1, 0, 0], "sparse", "hold", 13, 8, "k.......s.....h.", True),
    "D_ICE":    (22, 55, "minor", [0, 3, 5, 4], [2, 3, 4, 0], "sparse", "hold", 14, 8, "k.......s.......", True),
    "D_JUNGLE": (23, 50, "dorian", [0, 0, 3, 6], [5, 6, 3, 0], "calm", "pulse", 10, 9, "k..kk..hs..kk.hh", True),
    "D_OCEAN":  (24, 54, "dorian", [0, 5, 3, 4], [2, 5, 3, 0], "sparse", "fifth", 14, 8, "k.......s.......", True),
    "D_DUSK":   (25, 53, "locrian", [0, 1, 4, 3], [5, 3, 1, 0], "sparse", "walk", 12, 8, "k.h.....s.h....h", True),
}


def midi_hz(m): return 440.0 * 2 ** ((m - 69) / 12.0)


def main():
    sq = [max(0, min(2047, round(2048 - 131072 / midi_hz(m)))) for m in range(128)]
    wv = [max(0, min(2047, round(2048 - 65536 / midi_hz(m)))) for m in range(128)]
    c = ["#pragma bank 7", "/* generated by tools/make_sound.py: do not edit */", "#include <gb/gb.h>", '#include "sound_data.h"']
    c.append("const UINT16 snd_sq[128] = {" + ",".join(map(str, sq)) + "};")
    c.append("const UINT16 snd_wv[128] = {" + ",".join(map(str, wv)) + "};")
    for n, (seed, root, scale, pa, pb, sty, bs, tempo, vol, dr, low) in NEW_TRACKS.items():
        mel, bas = compose(seed, root, scale, pa, pb, sty, bs, low)
        TRACKS[n] = (mel, bas, tempo, vol, dr)
    names = list(TRACKS)
    for n in names:
        mel, bas, tempo, vol, dr = TRACKS[n]
        assert len(mel) in (64, 256) and len(mel) == len(bas) and len(dr) == 16, n
        drums = [".ksh".index(ch) for ch in dr]
        c.append("static const UINT8 %s_m[%d] = {%s};" % (n.lower(), len(mel), ",".join(map(str, mel))))
        c.append("static const UINT8 %s_b[%d] = {%s};" % (n.lower(), len(bas), ",".join(map(str, bas))))
        c.append("static const UINT8 %s_d[16] = {%s};" % (n.lower(), ",".join(map(str, drums))))
    c.append("const SndTrack snd_tracks[MUS_N] = {" + ",".join("{%s_m,%s_b,%s_d,%d,%d,%d}" % (n.lower(), n.lower(), n.lower(), TRACKS[n][2], TRACKS[n][3], len(TRACKS[n][0]) - 1) for n in names) + "};")
    open(os.path.join(ROOT, "src", "sound_data.c"), "w").write("\n".join(c) + "\n")
    h = ["#ifndef SOUND_DATA_H", "#define SOUND_DATA_H", "#include <gb/gb.h>", "/* generated by tools/make_sound.py: the tunes (bank 7) */",
         "enum { " + ", ".join("MUS_" + n for n in names) + ", MUS_N };",
         "typedef struct { const UINT8 *mel, *bas, *drm; UINT8 tempo, vol, mask; } SndTrack;   /* mask: the last step (63 or 255) */   /* drm: 0 none, 1 kick, 2 snare, 3 hat */",
         "extern const UINT16 snd_sq[128];   /* MIDI note -> square-channel frequency register */",
         "extern const UINT16 snd_wv[128];   /* MIDI note -> wave-channel frequency register */",
         "extern const SndTrack snd_tracks[MUS_N];",
         "#define MUS_PLANET_K(k) ((k) ? MUS_P_DESERT + (k) - 1 : MUS_PLANET)    /* the tune of world kind k (0 garden .. 5 dusk) */",
         "#define MUS_DUNGEON_K(k) ((k) ? MUS_D_DESERT + (k) - 1 : MUS_DUNGEON)", "#endif"]
    open(os.path.join(ROOT, "src", "sound_data.h"), "w").write("\n".join(h) + "\n")
    print("sound: %d tracks" % len(names))


if __name__ == "__main__":
    main()
