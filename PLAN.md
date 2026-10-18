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
   4b. Window-layer HUD (src/hud.c: HP/MP bars, gold, level; low-HP red) that stays put while the map scrolls. DONE (host-tested).
   4c. Typewriter dialogue box (src/dialog.c): slides over the HUD, word wrap, speaker name, paging with a blinking arrow, A rushes. DONE (host-tested).
   4d. SYSTEM menu (src/sysmenu.c): right-side overlay in the window layer, RESUME / SAVE / OPTIONS / TO TITLE, START opens it in the game. DONE (host-tested). Step 4 complete.
5. Bug creator. DONE (builds; untested on screen). src/bug.c builds the bug at run time from parts (art: tools/make_bug.py -> src/bugparts.c),
   cut into 8x16 sprites, at most 6 per scanline; 13 parts + 8 colours (the GBA's antenna/leg colours are now the outline), 18 ready-made looks,
   MATCH colour schemes, lock, surprise, undo, RGB mixer, name entry (src/bugmake.c). Save slots are now v2 (96 bytes, holds the look; v1 slots convert on load).
   NEW GAME opens the creator; a walking bug shows on the placeholder game screen. Not done: a MIRROR entry in the SYSTEM menu (call bugmake_run(&sv.look, sv.name, 0)).
6. Planet top-down. DONE (tested in an emulator). src/planet.c (bank 4) + tools/make_planet.py -> src/planet_art.c: a 256x256-metatile world (4096 px,
   wrapping) worked out from the save's seed as it scrolls into view (table-driven value noise: no RAM map), six kinds of world (GARDEN, DESERT, ICE,
   JUNGLE, OCEAN, DUSK) that recolour the same 11 ground tiles (VRAM bank 1, BG palettes 4..7), forests, flowers, boulders on high ground, berry bushes
   (heal, gold; the last 32 eaten are remembered), the ship as a sprite on a landing pad (A: heal + save), the door down (sealed until step 7).
   B runs. NEW GAME / CONTINUE / LOAD now land on the planet; the step-5 placeholder screen is gone.
   ROM layout: bank 0 = always-mapped core (keep it under 16 KB!), bank 2 = title + menus, bank 3 = bug creator, bank 4 = planet, bank 5 = dungeon, bank 6 = space.
7. Dungeon, top-down. DONE (tested in an emulator). src/dungeon.c (bank 5) + tools/make_dungeon.py -> src/dungeon_art.c. The planet's door leads down.
   Floors of 32x32 metatiles (1 KB map in RAM) from the seed + floor number: up to 8 rooms and L corridors, wall faces, stairs up and down,
   chests (bite to open: gold, sometimes a potion), potions on the floor (SELECT drinks one), spiders, beetles, wasps and slimes that wake
   when you come near, chase, bite and get knocked back; A bites the way you face (the bite flash sprite), XP and level-ups, gold.
   3 floors + 1 per world freed (max 6); the last floor has the spider queen (32x32) and a caged friend: beat her, bite the cage, the world
   is free (sv.zone++, autosave). Fainting: back to the ship, a quarter of the gold lost. Save v2 gained friends / deepest / potions
   (taken from the reserved bytes). Test builds: -DTEST_DOOR starts by the door, -DTEST_BOSS goes straight to the queen.
8. Space and the ship. DONE (tested in an emulator). src/space.c (bank 6) + tools/make_space.py -> src/space_art.c. A at the ship heals, saves and
   asks FLY INTO SPACE?; main.c loops planet -> space -> planet. Space is 4096 px square (wrapping), 16x16 cells with one star each: 256 worlds.
   world_seed(n) (save.c) gives star n its world (star 0 = the save's seed, so older saves keep their home), and its kind and made-up name
   (util.c). The planet and the dungeon use world_seed(sv.star). Flight: d-pad 8 ways with drift, B boosts, A shoots, A over a planet lands
   (its name and kind show in the HUD line). Pirate drones spawn away from planets, chase and ram; shoot them for gold and XP. An arrow
   points at the nearest other world. Planets are tinted per kind with BG palettes 4..7 on a 2x2-cell checkerboard; the stars use palette 3.
   Save gained star (from the reserved bytes). Test build: -DTEST_SPACE starts in space with a fresh game in slot 1.
9. Sound. DONE (tested in an emulator with an APU: tools are minigb_apu + Walnut-CGB). A small player of our own instead of hUGEDriver:
   src/sound_play.c (bank 7, run from the VBL interrupt; src/sound.c in bank 0 maps the bank and keeps the requests) + tools/make_sound.py
   -> src/sound_data.c. The GBA tunes (title, planet, dungeon, space, boss, ending): tune on channel 2, wave bass on 3, an echo of the tune
   three steps late on 1 (right speaker), a 16-step drum line on 4. 22 effects (the GBA's 15 plus laser, boom, menu move/ok/back,
   level up, dialogue typing) borrow channel 1 or 4 for a moment. OPTIONS music OFF/LOW/FULL and SFX ON/OFF work.
10. Story and ending. DONE (tested in an emulator). Cut from the GBA game: karts, hell-world variants, towns, the film and the trailer.
   The 8 data cores, as a flag set in the save (cores, story flags, the 8 freed stars, from the reserved bytes): each new world whose queen
   you beat gives Bolt a core and a memory (the GBA's words). Bolt tells the goal on the first landing; SYSTEM shows CORES n/8.
   With all 8, the Technobeast waits in space where four cells meet, halfway from home (2048, 2048): a 48x48 sprite of 18 + a core
   sprite that opens for a moment (lasers clang off the shut armour), spits orbs, rams. Beat it: Bolt says her name (DAWN), the ending
   (src/ending.c, bank 2) pages through the story with the ending tune, then the game's numbers; the save is kept with ST_DONE.
   Test builds: -DTEST_SPACE -DTEST_BEAST starts under the beast (3 HP) at level 30.
