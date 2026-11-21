#!/usr/bin/env python3
"""Optional headless Game Boy Color boot/screen smoke test using PyBoy.

Checks that the packaged ROM gets beyond boot into an actual nonempty screen,
then exercises title-screen input. Saves a real emulator framebuffer, not an
AI-generated preview. Full space-flight and boss progression still need
longer emulator playthrough tests with a controlled save.
"""
from pathlib import Path
import sys

from pyboy import PyBoy

rom = Path(sys.argv[1] if len(sys.argv) > 1 else "bugstar.gbc")
out = Path(sys.argv[2] if len(sys.argv) > 2 else "smoke/boot.png")
out.parent.mkdir(parents=True, exist_ok=True)
boy = PyBoy(str(rom), window="null", sound_emulated=False, cgb=True)
boy.set_emulation_speed(0)
max_colors = 0
try:
    for n in range(1, 8):
        assert boy.tick(60, render=True, sound=False), "Emulation stopped during boot"
        image = boy.screen.image.convert("RGB")
        max_colors = max(max_colors, len(image.getcolors(maxcolors=257) or [0] * 257))
        if n == 3:
            boy.button_press("start")
        if n == 4:
            boy.button_release("start")
        if n == 5:
            boy.button_press("a")
        if n == 6:
            boy.button_release("a")
    boy.screen.image.save(out)
    assert max_colors >= 3, f"No varied graphics after 420 frames: {max_colors} colors"
    print(f"Headless GBC emulator boot passed: {max_colors} screen colors, 420 frames, snapshot {out}")
finally:
    boy.stop()
