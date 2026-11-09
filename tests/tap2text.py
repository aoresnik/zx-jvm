#!/usr/bin/env python3
"""
Runs a tape headlessly in SkoolKit's Z80 simulator and prints the text on the
final screen, read from screen memory by matching each 8x8 character cell
against the font in the 48K ROM.

The tape is loaded on a 48K Spectrum until it jumps to the JVM at 32768, then
run until BASIC waits for a key (WAIT-KEY in the ROM). The JVM never calls
WAIT-KEY, so by then the program has returned and its report, such as 0 OK, is
on the screen. Cells that match no character, in normal or inverse video, are
printed as U+FFFD.

usage: tap2text.py [--max-seconds N] [--png FILE] tape.tap

--max-seconds  emulated time to allow the program, after loading (default 60)
--png          also write an image of the final screen

Exit status: 0 finished, 3 did not finish in time (the screen is still
printed), 1 failure, 2 usage. Needs SkoolKit 10 (pip install skoolkit).
"""

import argparse
import contextlib
import io
import os
import sys
import tempfile

import skoolkit
from skoolkit import tap2sna, trace
from skoolkit.snapshot import Snapshot

JVM_ADDR = 32768
WAIT_KEY = 0x15D4
FONT_ADDR = 0x3D00      # characters 32 to 127
SCREEN_ADDR = 0x4000
T_STATES_PER_SECOND = 3500000
ROM = os.path.join(os.path.dirname(skoolkit.__file__), 'resources', '48.rom')

# Where the Spectrum character set differs from ASCII
SPECIAL = {0x5E: '↑', 0x60: '£', 0x7F: '©'}


class Failure(Exception):
    pass


def run_tool(main, args):
    """Runs a SkoolKit command in this process, keeping its output for errors."""
    out = io.StringIO()
    try:
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            main(args)
    except SystemExit as e:
        if e.code not in (None, 0):
            raise Failure(out.getvalue().strip())
    except Exception as e:
        raise Failure('{}{}'.format(out.getvalue(), e).strip())


def load_font():
    with open(ROM, 'rb') as f:
        rom = f.read()
    font = {}
    for code in range(32, 128):
        glyph = tuple(rom[FONT_ADDR + (code - 32) * 8:FONT_ADDR + (code - 31) * 8])
        c = SPECIAL.get(code, chr(code))
        font[glyph] = c
        font.setdefault(tuple(b ^ 0xFF for b in glyph), c)
    return font


def screen_text(memory, font):
    lines = []
    for row in range(24):
        line = ''
        for col in range(32):
            # Screen memory interleaves rows: thirds, then pixel lines, then rows
            addr = SCREEN_ADDR + ((row & 0x18) << 8) + ((row & 7) << 5) + col
            glyph = tuple(memory[addr + (y << 8)] for y in range(8))
            line += font.get(glyph, '�')
        lines.append(line.rstrip())
    return '\n'.join(lines).rstrip()


def main():
    parser = argparse.ArgumentParser(usage='tap2text.py [--max-seconds N] [--png FILE] tape.tap')
    parser.add_argument('tape')
    parser.add_argument('--max-seconds', type=int, default=60)
    parser.add_argument('--png')
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding='utf-8')

    try:
        with tempfile.TemporaryDirectory() as tmp:
            loaded = os.path.join(tmp, 'loaded.z80')
            final = os.path.join(tmp, 'final.z80')

            run_tool(tap2sna.main, ['--start', str(JVM_ADDR), args.tape, loaded])
            if Snapshot.get(loaded).pc != JVM_ADDR:
                raise Failure('the tape did not start the JVM at {}'.format(JVM_ADDR))

            outputs = [final] + ([args.png] if args.png else [])
            run_tool(trace.main, ['--stop', str(WAIT_KEY),
                                  '--max-tstates', str(args.max_seconds * T_STATES_PER_SECOND),
                                  loaded] + outputs)
            snapshot = Snapshot.get(final)
            print(screen_text(snapshot.memory, load_font()))
            if snapshot.pc != WAIT_KEY:
                print('tap2text.py: did not finish within {} s'.format(args.max_seconds),
                      file=sys.stderr)
                return 3
    except Failure as e:
        print('tap2text.py: {}'.format(e), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
