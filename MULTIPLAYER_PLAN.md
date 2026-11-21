# BUGSTAR GBC: multiplayer link-cable implementation plan

Status: **L0 link protocol host tests pass; real serial cable and full co-op gameplay remain experimental**. A dedicated standalone link-test ROM now compiles only `link_diag.c`, `link_proto.c`, `link_cable.c` and the banked font. It deliberately avoids linking the whole campaign into the 16-KiB home bank and avoids changing retail single-player saves/UI. Companion to
[BUGSTAR multiplayer plan](https://github.com/masong0508-lab/bugstar/blob/main/MULTIPLAYER_PLAN.md).
This document applies to the real Game Boy Color ROM in this repository,
not an imaginary Internet-enabled GBC. Keep `PARITY.md` accurate.

## First deliverable: link-cable space encounter (2 players)

- Press **MULTIPLAYER** only once the transport, waiting/ready state and
  playable encounter are actually implemented. No dead menu item.
- Both Game Boy Colors show a ship and its remote partner in the existing
  bank-18 3D flight view; either player can turn, climb, fire, take damage,
  leave and reconnect. First mission is a bounded space-duel/wingman encounter;
  large shared procedural worlds and full campaign sync are not phase 1.
- The GBC serial cable uses two players and synchronous byte transfers; do
  not assume GBA's 4-player SIO wire format or PC's socket libraries work.
  Establish a master/slave exchange schedule and an explicit ready handshake.
- Use a tiny fixed packet with version, logical tick, sequenced input bits,
  signed/delta ship pose (quantized), event flags and CRC. Match the shared
  *meaning* of the messages in the main repository, not its raw native
  struct layout. No sending rendered pixels, bitmap frames or save files.
- Pause/hold a known input when a byte is late. Restart with a checksum
  resync or a clean error message, never a silent desync or emulator hang.
- Keep normal game RNG separate from cosmetic 3D dust and star effects.
  Player 1 creates the seed and mission rules; both see the same enemy
  attack telegraphs and result. Preserve single-player save slots.
- Quantized remote ship sprites come ahead of decorative dust in the existing
  **40 total sprites / 10 per scanline** constraints; retain nearby worlds,
  docking prompt, enemy projectiles and boss visibility. Do not add buffers
  exceeding VRAM/WRAM budgets or change fixed SRAM offsets.

## Specific source hooks to audit before coding

- `src/space3d.c`, `src/space3d.h`: 3D camera, 3D ship/dust priorities,
  frame integration and current docking and firing interactions.
- `src/space.c`, `src/space_foe.c`, `src/fleet.c`: physical state and
  attack resolution; isolate event hooks from single-player side effects.
- `src/input.c`: sample local buttons once per logical tick.
- `src/save.c` and `src/save_guard.c`: never write remote state into an
  old single-player slot; rewards happen only after a confirmed result.
- `tools/check_space3d.py` and `tests/host_test.c`: extend with 2-player
  geometry, malformed packet and disconnect assertions.
- `src/sound*.c`: serial interrupts must not stall music updates.

## Milestones with entry/exit criteria

1. **L0 interface contract + host tests (implemented, CI pending):** `src/link_proto.h/.c`
   now implement a compact 12-byte GBC-only packet, little-endian seed/tick,
   CRC8, explicit Ready and Active flags, wrap-aware sequence rejection, timeout
   and manual Ready reset after disconnect. `make link-test` exercises 10,000
   round trips and two peers. This is transport-independent code: it does
   **not** exchange actual cable bytes, show the second ship or save rewards.
2. **L1 real link handshake:** connect/ready/leave and failure message on
   two linked emulators. Verify re-entry to solo without a reboot.
3. **L2 real gameplay:** 3D ship movement, two viewpoints, shots, authoritative
   hits, damage, result, docking, and safe return. No unimplemented actions.
4. **L3 race mode:** reuse existing kart controls and finish order with 2
   connected devices. Synchronize lap transitions and item effects.
5. **L4 hardware validation:** test two cartridges + cable, save continuity,
   visual readability during fast turns and boss effects, long session
   memory/CPU, audio timing, unplug/replug and power cycle.

## What GBC cross-play means

GBC-to-GBC over cable is the first supported mode. GBA-to-GBC, online
PC-to-GBC and Android-to-GBC do not become possible automatically; they
would require a host-side serial-link gateway, a tested protocol converter
and either compatible emulator networking or dedicated bridge hardware.
None is implemented yet.

## Parallel 3D polish (after the shared engine station improvement)

Improve near-plane stability and distant object depth cues without exceeding
sprite limits. Keep world coordinates identical across the flat 2D and 3D
cameras, and do not make station docking depend on a cosmetic projection.
Benchmark the existing `src/space3d.c` projection and player/foe priorities
before adding a second player sprite or changing planet draw order.
