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
- **Town places** (boot-tested in the emulator): MARKET (buy and sell the 8 goods at the town's prices), BOUNTY OFFICE (a job paid the moment it is done),
  CANTINA (soup, 24 rumours plus true tips, a round for fame, arm wrestling), RACE POST (a quick kart race for an entry fee). The quest board has the GBA's 8 kinds.
  The **PORTAL spell** (level 10, 7 MP) keeps the floor you cast it on; the next door down on that world opens on it (checked: cast on floor 3, the door reopened on floor 3).
- Bug creator: 16 parts (length, shape, finish, pattern, eyes, mouth, antennae, legs, feet, horn, wings, tail, hat, cheeks, collar, back),
  8 colours, 18 presets, lock row, matching colours, surprise me, undo, RGB mixer, name entry.
- Planets: 6 kinds of world, towns/cities, shops, the ship menu (GO / SHOP / CARE / YOU), berries, fishing, weird worlds with marks,
  moods, living creatures, relics.
- Dungeons: rooms and corridors, chests, potions, spells, floor events, labyrinths (1 floor in 4), the city pit, hell worlds, moon
  dungeons, queens and cages, fainting rules, six difficulties (BABY ... NIGHTMARE).
- Space: flight, pirates, 256-star galaxy map and arrow, warp rail shooter, 3D flight, stations, Elite trade (goods, jobs, fame), lounge,
  fleet battles and wingmen, outposts, the Technobeast fight.
- Kart racing and KART WORLD (grand prix, time trial, quick race, items, jump pads), mining, feats, extras cards, how-to guide, debug menu.
- **KART WORLD polish** (this pass): 1, 3 or 5 laps, a FINAL LAP banner, FORWARD or REVERSE (the same road driven the other way round), a results box after every race, best lap / best race / best time trial per track and per way kept in their own SRAM block (offset 3072, older saves just start empty), a records panel in the track menu, KART and WAR tunes, count-down beeps. The road runs at ~57 fps (raw STAT interrupt, VBlank-only HUD and palette writes, assembly row fill).
- **WARSHIP RAID** (this pass, EXTRAS, level 6): the GBA's colossal warships as a one-screen mission: a hull of 2, 4 or 6 guns over a world whose planet-buster is counting down; shoot the guns, then the core. Your ship parts count (LASER, GUNS, ENGINE, HULL, SHIELD). Gold, XP and fame; each size opens the next; best times kept (SRAM offset 3584).
- **STATS book** (this pass, EXTRAS): three pages of counters (adventure, galaxy, games).
- **BUG HIVE, a real colony game** (this pass, EXTRAS card BUG HIVE; banks 45..51; it never overrides or gates the main game: ignore it, or raise a hive beside it). Emulator-tested with frames looked at:
  - A scrollable top-down colony map (32x32 cells, the BG map itself, scrolled with SCX/SCY): sky and grass on top, soil below, tunnels you mark to dig (A on soil/ore/crystal, or hold B and walk to drag-dig), rooms you place on tunnels (A on a tunnel), levels 1 to 8 (palette tier changes at level 4).
  - Workers are real sprites on a 40-sprite budget (up to 24 bugs) that walk the tunnels (breadth-first search to the nearest place for their job) and do the work: FORAGERS leave by the entrance at day and come back with food, leaf or honey (what the world gives), MINERS and BUILDERS dig marked cells (ore, crystal, root leaf) and raise sites, NURSES tend the nursery, GUARDS patrol entrance and barracks, the IDLE tend farms, smelters, shrines. Quotas per job on the JOBS page.
  - Chains: leaf -> food (fungus garden, honey from level 4), ore + leaf -> bars (smelter), bars -> gear (workshop: faster building, stronger guards), crystal and honey for research and the shrine. 11 buildings: nursery, fungus farm, storehouse, barracks, workshop, smelter, wall, gate, watchtower, shrine, market (+ the queen's room).
  - Needs and happiness (food, honey, shrine, safety, plague, storms, grief after a loss), births from eggs, starvation, people leaving. Seasons (6 days each), weather (clear, rain, storm, snow, dry), day and night (palette darkens, foragers sleep), events (drought, plague, traders, bloom), raids by bandits, rival ants and, every 5th, a boss beetle, with a DEFEND phase (place guards on the tunnels, watch them and the towers hold the shaft; walls and gates must be broken) or auto-resolve by numbers.
  - 12-tech research tree (economy, war, lore), 3 quest slots of 8 kinds (two are tied to the worlds you have visited), 16 achievements, a score with CALM / NORMAL / HARSH, trade with towns (prices follow the town's kind of world) at the market, outposts on the 12 kinds of world (unlocked by having visited a world of that kind), and while you are away the colony runs by formula for up to 10 days (200 ticks).
  - The colony is founded on one of the 12 kinds of world you have stood on: GARDEN, DESERT, ICE, JUNGLE, OCEAN, DUSK, MESA, FUNGAL, CRYSTAL, LAVA, SWAMP, CANDY give different ground colours, ore and crystal veins and forage mixes.
  - LINK TO THE GAME: give gold, hold ore, crystal and food, claim a small daily TRIBUTE (a few gold, a potion every 4th claim), a ROYAL VISIT if you freed a friend. Rewards flow back only in small amounts (400 gold at most per claim). Level cap untouched (999).
  - Own save block in the slot's SRAM bank (offset 4608: state, then the 1 KB map; version byte, checksum), saved once a day and on leaving; old hives are dropped. All pages are full-window text pages through the shadow-buffer engine (no flicker); the HUD is a 3-row window.
  - Not done in BUG HIVE: one worker sprite each (no larvae or egg sprites on the map), guards stand where placed (no moving guards in the defend phase), one raid at a time, outposts are passive (no raids on them), no caravans (trade is a price list), the map is fixed at 32x32 and has no flooding or cave-ins, no sound effects of its own (reuses the game's), no real hardware test.

- **HUD** (this pass): bars glide to the real value, the heart bar blinks at low HP, numbers are four characters wide (K past 9999), STATUS pips shrink for long numbers. The level cap stays 999 (audited: every level multiplication fits 16 bits there; 9999 would not).
- **Save safety** (this pass): every slot is written twice (a mirror at SRAM offset 4096 first, then the real copy); a torn write leaves one good copy and load falls back to the mirror. The host test covers it.
- Saving: 3 slots, checksums, 6 difficulties, old-save conversion (versions 1 to 4).

## Approximated
- Dungeons are top-down 32x32 tile maps; the GBA has textured first-person 3D dungeons.
- The galaxy is 256 worlds from a 32-bit seed; the GBA's is 2^155 clusters.
- Monsters: 4 base kinds plus per-world variants; the GBA has 66 planet monsters with BOOM/SPLIT tricks.
- Weird worlds: 16 types x 9 strengths; the GBA has 40 kinds and 1000 traits. Bolt's trait remarks are matched to the nearest.
- Pause screen: the GBC has the SYSTEM overlay (RESUME, STATUS, SAVE, OPTIONS, MIRROR, SKILLS, EXTRAS, FEATS, GUIDE), not the GBA's nine-page pause screen.
- Bolt is the ship in the GBC game's lines; the GBA now has him as a small droid that rolls after you. Same remarks, ship wording left alone.
- Ladybug buddies: one extra for every 100 player levels (up to nine), as on the GBA; OPTIONS > LADYBUGS is the most you bring (ALL/NONE/1/2/3/5/7). They are small gold sprites that keep a place round you and nip monsters near them (never the last bit of health). Differences: the GBA ones are red and go after the nearest monster; the GBC has no spare red sprite palette, and it has no pet Buddy (the old "friends" trail you as before). To make the unlock reachable the GBC level cap went from 99 to 999; past level 99 the HP/MP curve is gentler, and the HUD shows "L100".
- Options: 15 plus 3 rumble rows; the GBA has more (message speed, buddy, graphics...). HUNGER/STAIRS HINT/etc. exist; camera option is not used.
- Warships: the GBA has threats that bombard worlds, troop waves, boarding and a burning-planet escape; the GBC has the one-screen RAID only (no consequences for failing).
- Mining: ore for gold at the ship. The GBA has a 12-ore catalogue, smelter, forge and ore grades.

## Not ported yet (would fit, left for a later pass)
- Tabbed hangar-style creators for bug and humanoid; ornaments; the deeper anchored wings/horns/hats/tails. (The GBC creator already has wings, horns, hats and tails as parts; the GBA's new anchoring and ornaments are not in.)
- Humanoid form, humanoid CALLINGS (outfit perks) and creature shapes. The GBC has no humanoid form at all, so callings have nothing to attach to.
- The new MARKET screen layout (the GBC market works, the GBA's new screen is not copied).
- Ruins that ask riddles, the Tech Hive campaign.
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

## Menus pass (soft redraw)
- Every text screen redraws through a shadow buffer (src/textsoft.c): only the cells that changed reach video memory, so menus no longer blank and refill. Options, load/new picker, SYSTEM, status, skills, extras, feats, guide, shop, loot, elite, fleet, galaxy, towns, stats, kart and warship menus use it.
- Up/Down wrap, held directions repeat, B backs out, a help line sits at the bottom. SYSTEM hides the sprites while its box is open.
- Font: G, O, 0 and 6 are now clearly different. DUSK worlds (planet, dungeon, space) have brighter colours.
- Kart: results box fits ("YOU WON!"), lap banner keeps clear of your car, countdown "GO!" text shortened, text palettes restored after the fade, kart menu no longer draws into the road map. Grand prix (3 races, points, cup, prize), REVERSE, 1/3/5 laps and time trial were run to the end in the emulator.
- Not done: a universe selector (there is no universe on the GBC), a level cap above 999 (asked to leave alone), new traits/planet kinds (kinds and traits come from bit fields of the world seed, so adding any would change existing worlds and saves).
- tools/emu: headless runner (Walnut-CGB) and menu flicker suite; kart test hooks KX_STEER / KX_GAS / KX_WAIT for end-to-end races.

## Content pass (version 6 saves; old saves are erased on first start with a message)
- Save version 6: the save slot gained a gift and a quirk (traits), the extras block gained 64 spare bytes (feats 17..48), settings gained TEXT SPD, RUN WITH B, TIMED SAVE. Saves from older versions are wiped and the title shows "OLD SAVES ERASED". Level cap is still 999.
- Worlds: 12 kinds now (was 6): MESA, FUNGAL, CRYSTAL, LAVA, SWAMP, CANDY added with their own planet art and palettes, dungeon tiles and palettes, space palettes, flora/creature art (life), and queens that call their own brood. The kind comes from `(seed>>8)%12`; the weird-world strength/trait byte was re-planned.
- Weather on planets (src/weather.c, bank 44): rain, storm with lightning, snow, ash, spores, dust, shards, fog, confetti. It depends on the world kind, the world and the day. Particle sprites plus palette bends. Not in dungeons or space.
- Traits (src/traits.c, bank 43): every bug is born with a gift and a quirk (15 traits: HARDY, BRAINY, STRONG, THRIFTY, CLEVER, HEALTHY, ARCANE, GRITTY, SHARP, SCHOLAR, GREEDY, BRAVE, FRAGILE, PATIENT, WILD). They change health, mana, bite, gold, XP, potion and spell numbers by a percent. The STATS book has a TRAITS page. The queens' tricks (6 new, one per new kind) work on kills.
- Spells: NOVA, DRAIN, QUAKE, LIGHTNING, STARFALL added (10 in all).
- Feats: 48 (was 16), three pages, stored in ex.xs[0..3].
- Warship raid mission board: the WAR BOARD entry of the raid menu (and SELECT on a station's job board) posts three missions a round, each with a size, a modifier (RUSH, IRON HULL, FLURRY, ARMOURED) and a bonus pay; a finished round posts a new one. After a jump, warships sometimes drop out of the warp on top of you (an ambush, with the alert tune, bonus pay).
- Kart: 6 tracks (was 3: desert sunset, candy and night added), 2 cups, MEGA and SHIELD items, ghost car in time trial.
- Music: 19 new tunes (bank 42): per-planet and per-dungeon themes for the six new kinds, TOWN, STATION, ALERT, KART2..4, SHOP, CAMP.
- Options: TEXT SPD (4 speeds), RUN WITH B (hold or toggle), TIMED SAVE (every 2 or 5 minutes on a planet).
- Build: per-object header dependencies (tools/deps.py), so a header edit rebuilds only what includes it. Host tests cover the wipe and the traits.
- Dungeon floor events: 5 new (PEDDLER potion, SPIRIT mana, CAMPFIRE heal, LUCKY COINS, OLD SCROLL xp), picked per floor with the old ones.
- BUG HIVE (EXTRAS, bank 45): a small colony game: farm, mill, quarry, house, wall, tower; resources grow with play time, raiders come every 8th tick (20 s) and grow stronger, SELECT sells the stores for gold. Own SRAM block at offset 3300.
- Not done in this pass: more shop items (the save has one free upgrade byte), dungeon floor themes/monsters/bosses beyond the six kinds' art, galaxy size, town quests, a colony side game.
