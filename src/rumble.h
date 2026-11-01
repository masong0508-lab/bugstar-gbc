#ifndef RUMBLE_H
#define RUMBLE_H
#include <gb/gb.h>
/* BUGSTAR GBC - the motor of a rumble cartridge (header type 0x1E: MBC5 + RUMBLE + RAM + BATTERY). Port of the GBA game's rumble engine.
   On MBC5 rumble carts bit 3 of the RAM bank register (0x4000) drives the motor; the RAM banks we use are 0..3, so the bit is free.
   Plain carts and emulators without a motor ignore it. Every event is a PATTERN (a peak strength, a length in frames, a shape and a class);
   the strength becomes a duty cycle (the motor is slow, so it is pulsed 60 times a second) and the whole thing ticks from the vblank
   interrupt, so a long load or a save can never leave the motor stuck on (the motor is switched off while the cartridge RAM is open).
   shapes: 0 flat, 1 dies away (hits), 2 swells (stairs), 3 pulses (fanfares, quakes).   classes: 0 small, 1 fights, 2 big. */
enum { RP_BITE, RP_HIT, RP_PICK, RP_FIRE, RP_HURT, RP_STAIRS, RP_FANFARE, RP_COIN, RP_SPLASH, RP_DIE, RP_SPARK, RP_STEP, RP_THUD, RP_CRUNCH,
       RP_QUAKE, RP_BOSS, RP_TEST, RP_CRASH, RP_CATCH, NRP };
extern volatile UINT8 rum_bit, rum_bank, sram_open;
/* Every file that touches the cartridge RAM goes through these three (they replace gbdk's): the bank is remembered so the motor bit can ride along. */
#undef SWITCH_RAM
#undef ENABLE_RAM
#undef DISABLE_RAM
#define SWITCH_RAM(b) (rum_bank = (b), *(volatile UINT8 *)0x4000 = rum_bank)
#define ENABLE_RAM    (sram_open = 1, *(volatile UINT8 *)0x0000 = 0x0A)
#define DISABLE_RAM   (*(volatile UINT8 *)0x0000 = 0x00, sram_open = 0, *(volatile UINT8 *)0x4000 = rum_bank | rum_bit)
/* the three options live in opt.cam: bits 0-2 strength (0 OFF .. 5 EXTREME), bits 3-4 events (0 all, 1 fights and big, 2 big only), bit 5 engine hum */
#define RUM_LVL() (opt.cam & 7)
#define RUM_EV()  ((opt.cam >> 3) & 3)
#define RUM_HUM() ((opt.cam >> 5) & 1)
#define RUM_DEFAULT (2 | (1 << 3) | (1 << 5))
void rumble(UINT8 id);               /* fire a pattern (a new one takes over only if it is at least as strong as what is left of the old one) */
void rumble_sfx(UINT8 n);            /* sfx(n) -> pattern */
void rum_hum(UINT8 amp);             /* a steady rumble that lasts while you keep calling it every frame (engines, drifts) */
void rumble_tick(void);              /* once per frame, from the vblank interrupt */
#endif
