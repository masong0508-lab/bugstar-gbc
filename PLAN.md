# BUGSTAR: GBA -> GBC de-port plan

The GBA game is ARM C with 240x160 mode-4 bitmaps, a 64-bit economy, textured 3D dungeons,
Mode-7 karts and 2^155-cluster galaxies. None of that maps to an 8 MHz Sharp LR35902 with
160x144, tile/palette graphics and 16-bit ints, so each step rebuilds a feature, not translates it.
Each step ends with a ROM that boots in an emulator.

1. Scaffold: GBDK-2020 build, CI, input, title. DONE.
2. Title art: planet re-rendered at 160x144 (tools/make_title.py), converted to 243 GBC tiles + 8 palettes (tools/img2gbc.py), sprite cursor, twinkling stars. DONE (untested on hardware/emulator).
3. Options + SRAM save: 3 slots (one SRAM bank each, FNV checksum), 15 options, 32-bit world seed, title menu CONTINUE/NEW/LOAD/OPTIONS. DONE (host-tested; `make test`).
4. Text/menu engine, in parts:
   4a. Own font (tools/make_font.py -> src/font.c/h, tile number == ASCII), text engine core (src/text.c: put/line/box/bar/colour), menus moved onto it. DONE (host-tested).
   4b. Window-layer HUD (HP/MP bars, gold, level) that scrolls independently of the map.
   4c. Typewriter dialogue box with word wrap, speaker name, A to speed up, "more" arrow.
   4d. SYSTEM menu (options, save, quit) as an overlay usable from the game.
5. Bug creator, tile-built sprite from parts (OAM 8x16, 10 sprites/line budget).
6. Planet top-down: tilemap scroll replaces the 3D hills; seeded terrain from WorldDNA.
7. Dungeon: top-down or fake-3D by row-strip (NOT textured raycast); monsters, bite, items.
8. Space/ship/hangar: flat-sprite flight, simple galaxy of 256 stars, 32-bit seed.
9. Sound: GBC 4-channel music driver (hUGEDriver) replacing sound.inc.
10. Cut/shrink: karts, hell-world variants, 8 cores story as a flag set. (Film and trailer are dropped entirely.)
