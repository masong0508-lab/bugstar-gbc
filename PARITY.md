# PARITY: BUGSTAR GBA vs BUGSTAR GBC

Reference: the GBA game (github masong0508-lab/bugstar), commit 440c557 (1 Nov 2026). This file says what the GBC port
matches, what it only approximates, what is not ported yet, and what the Game Boy Color cannot do. It was made by reading the
GBA README/ROADMAP and grepping this source; "not verified in play" marks lines I did not run in an emulator.

## Matches (same feature, rebuilt for the GBC)
- Story: Bolt, the 8 data cores (one per freed world), the Technobeast, the ending (DAWN). Memories per core.
- **TALK TO BOLT** (new in this pass): GO tab of the ship menu. He worries (hurt, low fuel, no potions), says what to do next, a
  memory for the cores you have, a remark for the world you stand on (6 zone remarks, 16 class remarks, 40 trait remarks, mapped
  onto the GBC's 6 world kinds and 16 weird types) and one of the GBA's 64 idle remarks. Texts come from the GBA ui.inc through
  `tools/make_bolt.py` (`src/bolt_data.h`).
- **Rumble** (new in this pass): rumble cartridge type (0x1E), the GBA's pattern engine (19 patterns, strength, event filter, engine
  hum), OPTIONS rows RUMBLE / RUMBLE EVENTS / RUMBLE HUM. Motor is held off while the cartridge RAM is open (saves can never leave it on).
- Bug creator: 16 parts (length, shape, finish, pattern, eyes, mouth, antennae, legs, feet, horn, wings, tail, hat, cheeks, collar, back),
  8 colours, 18 presets, lock row, matching colours, surprise me, undo, RGB mixer, name entry.
- Planets: 6 kinds of world, towns/cities, shops, the ship menu (GO / SHOP / CARE / YOU), berries, fishing, weird worlds with marks,
  moods, living creatures, relics.
- Dungeons: rooms and corridors, chests, potions, spells, floor events, labyrinths (1 floor in 4), the city pit, hell worlds, moon
  dungeons, queens and cages, fainting rules, six difficulties (BABY ... NIGHTMARE).
- Space: flight, pirates, 256-star galaxy map and arrow, warp rail shooter, 3D flight, stations, Elite trade (goods, jobs, fame), lounge,
  fleet battles and wingmen, outposts, the Technobeast fight.
- Kart racing and KART WORLD (quick race and more modes, items, jump pads), mining, feats, extras cards, how-to guide, debug menu.
- Saving: 3 slots, checksums, 6 difficulties, old-save conversion (versions 1 to 4).

## Approximated
- Dungeons are top-down 32x32 tile maps; the GBA has textured first-person 3D dungeons.
- The galaxy is 256 worlds from a 32-bit seed; the GBA's is 2^155 clusters.
- Monsters: 4 base kinds plus per-world variants; the GBA has 66 planet monsters with BOOM/SPLIT tricks.
- Weird worlds: 16 types x 9 strengths; the GBA has 40 kinds and 1000 traits. Bolt's trait remarks are matched to the nearest.
- Pause screen: the GBC has the SYSTEM overlay (RESUME, STATUS, SAVE, OPTIONS, MIRROR, SKILLS, EXTRAS, FEATS, GUIDE), not the GBA's nine-page pause screen.
- Bolt is the ship in the GBC game's lines; the GBA now has him as a small droid that rolls after you. Same remarks, ship wording left alone.
- Options: 15 plus 3 rumble rows; the GBA has more (message speed, buddy, graphics...). HUNGER/STAIRS HINT/etc. exist; camera option is not used.
- Mining: ore for gold at the ship. The GBA has a 12-ore catalogue, smelter, forge and ore grades.

## Not ported yet (would fit, left for a later pass)
- Tabbed hangar-style creators for bug and humanoid; ornaments; the deeper anchored wings/horns/hats/tails. (The GBC creator already has wings, horns, hats and tails as parts; the GBA's new anchoring and ornaments are not in.)
- Humanoid form, humanoid CALLINGS (outfit perks) and creature shapes. The GBC has no humanoid form at all, so callings have nothing to attach to.
- The new MARKET screen layout (the GBC market works, the GBA's new screen is not copied).
- Bounty office, cantina gossip, race post, ruins that ask riddles, the PORTAL spell, quest board's 8 kinds, Tech Hive campaign.
- Landable asteroids, hollow-rock mines, mine hazards, megadeposits, ore grade stacks, ship parts at a smelter, SCAN of the richest vein.
- Keycard/locked-door outposts and power cores; the kart garage's 16 bodies and track maker; kart camera settings.
- IDENTIFY and WORKBENCH (SELL and buy-back are in the loot screen).

## Impossible (or not sensible) on the GBC
- 2^155 x 2^155 universe, universe jumps, 4.3 quadrillion kart tracks, 1000 weird traits as a table: 32-bit seeds, 16-bit ints.
- Textured real-time 3D dungeons and Mode-7 ground at the GBA's 240x160 in 15-bit colour: 160x144 tiles and sprites, 40 sprites, 10 per line.
- Planet DNA at the GBA's full detail (cities built into the 3D land, creatures with bodies and tempers per planet).
- 64-bit economy numbers. Gold is 32-bit, goods are bytes.
- "How big is it?" page: skipped on purpose (asked for).

## Not verified in play
- Rumble motor on a real rumble cartridge (checked only that the motor bit toggles in an emulator, and that saves stay valid with it on).
- Real hardware, in general: every run was done in the Walnut-CGB emulator.
