#!/usr/bin/env python3
"""Two *real* emulated GBC instances with a shared serial link (PyBoy v2.9+).

Checks the TEST_LINK diagnostic ROM's actual on-screen tilemap, not a C
protocol simulation. P1 selects A/MASTER, P2 selects B/GUEST, both confirm A.
Produces separate host/guest PNGs; a screenshot is not a pass by itself.
No SRAM writes or fake connection state is injected.
"""
import argparse
import multiprocessing as mp
from pathlib import Path
import queue
import sys
import traceback


def _text(pyboy, row):
    return "".join(chr(int(pyboy.tilemap_background[x, row]) & 127)
                   for x in range(1, 19)).rstrip()


def _player(rom, out_dir, primary, shared, barrier, messages):
    from pyboy import PyBoy
    role = "host" if primary else "guest"
    pyboy = None
    try:
        pyboy = PyBoy(str(rom), window="null", sound_emulated=False,
                      serial_shared_memory=shared,
                      serial_interrupt_based=True)
        pyboy.set_emulation_speed(0)
        linked = False
        both_ready = False
        last = ""
        for t in range(1800):
            # The fixed ROM diagnostic uses A=MASTER, B=GUEST.
            if t == 80:
                pyboy.button("a" if primary else "b", 6)
            # After a dozen packet transmissions (12 bytes at 60 Hz),
            # confirm player readiness and leave the link running.
            if t == 240:
                pyboy.button("a", 6)
            pyboy.tick(1, t % 8 == 0, False)
            # Both emulators advance in near lock-step without racing through
            # unrelated hundreds of frames while the peer is scheduling.
            barrier.wait(timeout=15)
            if t % 12 == 0 and t > 110:
                line = _text(pyboy, 4)
                status = _text(pyboy, 10)
                last = f"t={t} connection={line!r} mission={status!r}"
                if "LINKED" in line:
                    linked = True
                if "BOTH READY" in status:
                    both_ready = True
            # Never leave this loop before the other process: both must keep
            # clocking the serial link and reaching the shared frame barrier.
        # Force a final rendered frame before taking screenshots.
        pyboy.tick(1, True, False)
        path = out_dir / f"{role}-link.png"
        pyboy.screen.image.save(path)
        messages.put((role, linked, both_ready, str(path), last))
    except BaseException:
        messages.put((role, False, False, "", traceback.format_exc()))
    finally:
        if pyboy is not None:
            pyboy.stop(save=False)


def run():
    p = argparse.ArgumentParser()
    p.add_argument("rom", type=Path)
    p.add_argument("out", type=Path)
    args = p.parse_args()
    if not args.rom.is_file():
        raise SystemExit(f"missing link diagnostic ROM: {args.rom}")
    args.out.mkdir(parents=True, exist_ok=True)

    # Uses the same supported PyBoy shared serial buffer implementation
    # as PyBoy's official two-console Pokemon link demonstration.
    from pyboy.core.serial import SerialSharedMemoryBuffer
    shared = SerialSharedMemoryBuffer()
    context = mp.get_context("fork")
    barrier = context.Barrier(2)
    messages = context.Queue()
    workers = [
        context.Process(target=_player,
                        args=(args.rom, args.out, role, shared, barrier, messages))
        for role in (True, False)
    ]
    try:
        for w in workers:
            w.start()
        results = []
        for _ in workers:
            try:
                results.append(messages.get(timeout=55))
            except queue.Empty:
                break
        for w in workers:
            w.join(timeout=3)
        if any(w.is_alive() for w in workers):
            raise RuntimeError("serial diagnostic emulator hung during paired run")
        for row in results:
            print("GBC link:", *row, flush=True)
        if len(results) != 2 or not all(r[1] and r[2] for r in results):
            raise AssertionError("Both real emulated GBCs must show LINKED and BOTH READY")
        print("GBC serial: two independently running linked ROMs reached BOTH READY PASS")
    finally:
        for w in workers:
            if w.is_alive():
                w.terminate()
                w.join()
        shared.close()


if __name__ == "__main__":
    try:
        run()
    except Exception as exc:
        print(f"GBC linked emulator proof FAILED: {exc}", file=sys.stderr)
        raise
