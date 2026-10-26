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

## After the plan: the style pass (the person asked for "cooler, action packed and stylized")
- Pass 1 DONE: fades between every scene (src/fx.c, bank 7, reads colour RAM so it fits any scene), a WARNING cut-in with crawling
  hazard tape before the queen and the Technobeast, screen shake (OPTIONS: SHAKE), pirates that shoot, engine puffs when boosting.
- Pass 2 DONE: the dungeon: gold damage numbers that float up, a burst in the monster's colours when it dies, hit-stop on bites
  (3 frames, 6 on a kill). Test build: -DTEST_PLANET starts on the planet (add -DTEST_DOOR -DTEST_BOSS for the queen).
- Pass 3 DONE: title (waving logo by HBlank raster, glint, shooting stars, Bolt flies by); space darts and mines, debris, sky
  flashes; dungeon spider lunges, slime splits, queen spiderlings. Space tiles moved to bank 8; tools/check_banks.py fails the
  build when a bank is over 16 KB.
- The warp DONE (src/warp.c, bank 9, art tools/make_warp.py): SELECT in space jumps to where the arrow points, as a 3D rail
  shooter down a hyperspace tunnel. A small 3D engine: points (x, y, z), perspective by a 4096/z table; the tunnel is one picture
  of depth rings (quarter stored, mirrored by flip bits, fog palettes by distance) whose colours step to fly at you, and it slides
  as you steer. Meteors (shoot them) and star shards (dodge them) are aimed where you are; about one warp in three a hyperspace
  attack (3 pirates that strafe and shoot orbs). Gold, XP, a clean-run bonus and the save's best (warp_best). Test: -DTEST_ATTACK.

## Optimizing and room to grow (after the warp)
- ROM: 8 MB now (512 banks, the most MBC5 can do; header 0x148 = 08). GBDK's banked calls reach banks 1..255;
  banks 256..511 are for data, read after SWITCH_ROM_MBC5_8M. Bank map: 0 core, 2 title + menus + ending, 3 bug creator,
  4 planet, 5 dungeon code, 6 space (map, ship, landing, warp), 7 sound + font, 8 space tiles, 9 warp, 10 space foes,
  11 dungeon tiles; 12..511 empty. tools/check_banks.py prints every bank's free bytes and fails the build on an overflow.
- Bank 0 (always mapped, the precious one): the font moved to bank 7 (bsfont_load); 2.9 KB free (was 0.9).
- WRAM: the stack now tops out at D000 (-Wl-g.STACK=0xD000), so D000-DFFF is free: the big buffers (bug canvas, fade copies,
  dungeon map, stars) sit there at fixed addresses (src/wram.h). The variables in C000-CFFF went from 5.6 KB to 2.2 KB,
  leaving ~1.9 KB for the stack, and WRAM banks 2..7 (24 KB) are untouched, for later.
- Code: space.c split into space.c (bank 6) and space_foe.c (bank 10), sharing state through space_int.h.
- Build: one object per file in build/ (make -j2, only what changed is rebuilt; a full build went from ~8 to ~2.5 minutes).
  Test builds: make DEFS="-DTEST_SPACE" BUILD=build_t OUT=test.gbc

## Update: creator v2, MIRROR, blended ground
- Bug creator rewritten (bank 3 UI, bank 12 builder/art, bank 13 big preview, bank 14 data): 2x smoothed preview, tabs, name entry, colour mixer, undo, row locks, dice, stage themes.
- SYSTEM menu has MIRROR (re-opens the creator anywhere; the scene reloads its tiles after it).
- Planet ground is blended: palettes form a chain (deep|shallow|sand|grass) and 42 blend tiles round every shore and edge (tools/make_planet.py).
- Still to do: dungeon walls and space planets are still plain tiles; more creator parts (cheek, neck, back), Save v3; use the big ROM for pre-rendered art.

## Upgrades, the Elite layer, the 3D view, the Arena, Extras, polish pass
- Upgrades and skills (save v4), the Elite layer (stations, market of 8 goods, fuel, hold, jobs, fame ranks, galaxy chart, pods, shield,
  shipyard; save v5): src/econ.c (bank 15 with shop.c), src/elite.c (16), src/galaxy.c (17).
- 3D view (banks 18 + 19: space3d.c = maths and the sprite list, flight3d.c = the flight): unlocked at fame rank POOR (sv.el.rsv[0] bit 0),
  switched in the station's VIEW row or EXTRAS. A real 3D point model (yaw, pitch, climb), planets as shaded sprites in 4 sizes, a per-scanline
  sprite budget (10 per line: bigger planets fall back to smaller pictures), stations, pirates, pods, dust. space_run wraps both views (SP_FLIP).
- The Arena (flight3d.c): wave shooter, hover ship, a flagship every 4th wave, bombs, gold per kill, best wave in sv.el.rsv[1]; unlocked at rank
  AVERAGE (rsv[0] bit 2). unlocks_update() (econ.c) opens both.
- EXTRAS in the SYSTEM menu (sysmenu.c -> extras_run in flight3d.c): shows what is unlocked, switches the view, starts the Arena. GBA: an EXTRAS tab.
- Polish pass: bug art (head level with the body, jointed legs, seam, wings attached), framed save cards, station screens as cards with grey labels,
  bright values and gold selection; text palette 3 (grey) matches the space sky.

## Extras pass (cities, tricks, pets, day/night, fishing, pit, star map)
- Cities (town.c, bank 13): 3 per planet, tiers hamlet..metropolis; inn, quest board, stores, workshop, academy, fortune, casino, shrine, shipyard, bug pit. Open/closed by time of day.
- feat.c (bank 20): clock (480 s day, ex.off), pets (follow history ring), fishing, queen tricks (ex.tricks bits), memory words.
- Save: Extra block at SRAM offset 256 of each slot (own magic/sum), see save.h.
- dungeon.c: tricks, boss variants per world kind, pets chip at monsters, dungeon_pit(tier).
