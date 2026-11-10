#!/usr/bin/env python3
"""
Runs a tape headlessly in SkoolKit's Z80 simulator and prints the text on the
final screen, read from screen memory by matching each 8x8 character cell
against the font in the 48K ROM.

The tape is loaded on a 48K Spectrum until it jumps to the JVM at 32768, then
run until the ROM waits for a key (WAIT-KEY). If that is the scroll? prompt,
the screen is printed, followed by a "--- key pressed" line, and ENTER is
pressed. The run ends when the BASIC editor waits for a key, which is after the
program has returned and its report, such as 0 OK, is on the screen. Cells
that match no character, in normal or inverse video, are printed as U+FFFD.

usage: tap2text.py [--max-seconds N] [--png FILE] tape.tap

--max-seconds  emulated time to allow for each screen (default 60)
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
SCROLL_RETURN = 0x0CB6  # after the call to WAIT-KEY in the scroll? prompt
EDITOR_RETURN = 0x0F3B  # after the call to WAIT-KEY in the BASIC editor
KEY_ENTER = 13
LAST_K = 23560          # the last key pressed
FLAGS = 23611           # bit 5: a new key has been pressed
MAX_PAGES = 100
PAGE_BREAK = '\n--- key pressed\n'
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
            snapshot_file = os.path.join(tmp, 'loaded.z80')
            run_tool(tap2sna.main, ['--start', str(JVM_ADDR), args.tape, snapshot_file])
            if Snapshot.get(snapshot_file).pc != JVM_ADDR:
                raise Failure('the tape did not start the JVM at {}'.format(JVM_ADDR))

            font = load_font()
            pages = []
            resume = []
            while True:
                previous = snapshot_file
                snapshot_file = os.path.join(tmp, 'page{}.z80'.format(len(pages)))
                outputs = [snapshot_file] + ([args.png] if args.png else [])
                run_tool(trace.main, resume + [
                    '--stop', str(WAIT_KEY),
                    '--max-tstates', str(args.max_seconds * T_STATES_PER_SECOND),
                    previous] + outputs)
                snapshot = Snapshot.get(snapshot_file)
                pages.append(screen_text(snapshot.memory, font))
                if snapshot.pc != WAIT_KEY:
                    print(PAGE_BREAK.join(pages))
                    print('tap2text.py: did not finish within {} s'.format(args.max_seconds),
                          file=sys.stderr)
                    return 3
                sp = snapshot.sp
                caller = snapshot.memory[sp] | snapshot.memory[sp + 1] << 8
                if caller == EDITOR_RETURN:
                    break
                if caller != SCROLL_RETURN:
                    print(PAGE_BREAK.join(pages))
                    raise Failure('waiting for a key, called from {}'.format(caller))
                if len(pages) == MAX_PAGES:
                    print(PAGE_BREAK.join(pages))
                    raise Failure('more than {} screens'.format(MAX_PAGES))
                # Press ENTER as the keyboard interrupt would report it, and let
                # WAIT-KEY handle it as usual
                resume = ['--poke', '{},{}'.format(LAST_K, KEY_ENTER),
                          '--poke', '{},{}'.format(FLAGS, snapshot.memory[FLAGS] | 0x20)]
            print(PAGE_BREAK.join(pages))
    except Failure as e:
        print('tap2text.py: {}'.format(e), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
