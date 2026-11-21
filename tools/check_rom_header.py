#!/usr/bin/env python3
"""Sanity-check the final built GBC ROM, beyond C tests and ROM-bank limits.

This does not replace booting the ROM in a Game Boy Color emulator, but
catches broken/repackaged cartridges, incompatible mapper flags, and truncated
artifacts before the CI pipeline publishes them.
"""
from pathlib import Path
import sys

rom = Path(sys.argv[1] if len(sys.argv) > 1 else "bugstar.gbc")
data = rom.read_bytes()
assert len(data) == 8 * 1024 * 1024, f"Expected 8 MiB MBC5 ROM: {len(data)}"
assert data[0x134:0x13B] == b"BUGSTAR", "Cartridge title changed"
assert data[0x143] == 0xC0, "GBC-only color mode flag was removed"
assert data[0x147] == 0x1E, "Expected MBC5 + rumble + RAM + battery"
assert data[0x148] == 0x08, "The 8-MiB MBC5 ROM size header changed"
assert data[0x149] == 0x03, "The battery-backed SRAM size header changed"
chk = 0
for i in range(0x134, 0x14D):
    chk = (chk - data[i] - 1) & 255
assert chk == data[0x14D], "Invalid Nintendo cartridge header checksum"
total = (sum(data[:0x14E]) + sum(data[0x150:])) & 0xFFFF
expected = (data[0x14E] << 8) | data[0x14F]
assert total == expected, "Invalid global ROM checksum"
print(f"OK: {rom}: 8 MiB, CGB-only, MBC5 rumble/battery, valid header and ROM checksums")
