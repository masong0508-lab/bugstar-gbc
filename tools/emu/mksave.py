#!/usr/bin/env python3
"""Patch slot 1 of a BUGSTAR GBC save image (and its mirror) and fix the checksum, so a test can start anywhere.
usage: mksave.py IN.sav OUT.sav field=value ...   fields: star level gold xp hp mp zone cores story potions friends deepest fuel up.N=V (N = SK_/UP_ index) fame
Layout: tools/emu/mksave.py mirrors struct Save in src/save.h (version 5, 148 bytes at SRAM offset 0 and 4096 of bank 0)."""
import sys, struct
OFF = dict(level=(11, 2), hp=(13, 2), mp=(15, 2), gold=(17, 4), xp=(21, 4), zone=(10, 1), friends=(72, 1), deepest=(73, 1), potions=(74, 1),
           star=(75, 1), cores=(76, 1), story=(77, 1), fuel=(116, 1), fame=(124, 2))
def fnv(b):
    h = 2166136261
    for x in b: h = ((h ^ x) * 16777619) & 0xFFFFFFFF
    return h
def main():
    d = bytearray(open(sys.argv[1], 'rb').read())
    for base in (0, 4096):
        s = bytearray(d[base:base + 148])
        if struct.unpack_from('<I', s, 0)[0] != 0x43475542: print('slot 1 not a save at', base); continue
        for kv in sys.argv[3:]:
            k, v = kv.split('='); v = int(v)
            if k.startswith('up.'): s[92 + int(k[3:])] = v; continue
            o, n = OFF[k]; s[o:o + n] = v.to_bytes(n, 'little')
        struct.pack_into('<I', s, 144, fnv(s[:144]))
        d[base:base + 148] = s
    open(sys.argv[2], 'wb').write(d)
main()
