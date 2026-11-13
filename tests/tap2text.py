#!/usr/bin/env python3
"""
Runs a tape headlessly in SkoolKit's Z80 simulator and prints the text on the
final screen, read from screen memory by matching each 8x8 character cell
against the font in the 48K ROM.

The tape is loaded on a 48K Spectrum until it jumps to the JVM at 25600, then
run until the ROM waits for a key (WAIT-KEY):

- At the scroll? prompt, or when the JVM waits for a key after showing
  graphics, the screen is printed, followed by a "--- key pressed" line, and
  ENTER is pressed.
- When the program reads a line in the input line (INPUT mode), the next line
  of the input file is typed one key at a time. Python escapes in the file
  give other key codes, such as \\x0c for DELETE or \\xc7 for the <= keyword.
  Before ENTER, the screen is printed with the typed line on it, followed by
  "--- typed: " and the line from the file.
- When the BASIC editor waits for a key, the program has returned and its
  report, such as 0 OK, is on the screen. That final screen is printed.

Cells that match no character, in normal or inverse video, are printed as
U+FFFD.

usage: tap2text.py [--max-seconds N] [--png FILE] [--screen FILE] [--input FILE] tape.tap

--max-seconds  emulated time to allow between key presses (default 60)
--png          also write an image of the final screen
--screen       write the screen memory, 6912 bytes, as it is when the JVM
               waits for a key after showing graphics
--input        lines to type, one per input request; all must be used

Exit status: 0 finished, 3 did not finish in time (the screen is still
printed), 1 failure, 2 usage. Needs SkoolKit 10 (pip install skoolkit).
"""

import argparse
import codecs
import contextlib
import io
import os
import sys
import tempfile

import skoolkit
from skoolkit import tap2sna, trace
from skoolkit.snapshot import Snapshot

JVM_ADDR = 25600       # JVM_ADDR in the Makefile
WAIT_KEY = 0x15D4
SCROLL_RETURN = 0x0CB6  # after the call to WAIT-KEY in the scroll? prompt
EDITOR_RETURN = 0x0F3B  # after the call to WAIT-KEY in the BASIC editor
KEY_ENTER = 13
LAST_K = 23560          # the last key pressed
FLAGS = 23611           # bit 5: a new key has been pressed
FLAGX = 23665           # bit 5: INPUT mode
MAX_STEPS = 10000       # keys pressed, to stop runaway programs
SHOW_TSTATES = 140000   # two frames, for the editor to show the line before ENTER
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


def read_input(path):
    if path is None:
        return []
    try:
        with open(path, encoding='utf-8') as f:
            return f.read().splitlines()
    except OSError as e:
        raise Failure(str(e))


def line_keys(line):
    """The key codes that type a line of the input file, then ENTER."""
    try:
        keys = [ord(c) for c in codecs.decode(line, 'unicode_escape')]
    except UnicodeDecodeError as e:
        raise Failure('bad escape in input line {!r}: {}'.format(line, e))
    if any(k > 255 for k in keys):
        raise Failure('not a Spectrum key code in input line {!r}'.format(line))
    return keys + [KEY_ENTER]


def press(key, snapshot):
    """trace.py options that press a key as the keyboard interrupt would report it."""
    return ['--poke', '{},{}'.format(LAST_K, key),
            '--poke', '{},{}'.format(FLAGS, snapshot.memory[FLAGS] | 0x20)]


def main():
    parser = argparse.ArgumentParser(
        usage='tap2text.py [--max-seconds N] [--png FILE] [--screen FILE] [--input FILE] tape.tap')
    parser.add_argument('tape')
    parser.add_argument('--max-seconds', type=int, default=60)
    parser.add_argument('--png')
    parser.add_argument('--screen')
    parser.add_argument('--input')
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding='utf-8')

    transcript = []     # earlier screens, each followed by what happened next
    try:
        lines = read_input(args.input)
        with tempfile.TemporaryDirectory() as tmp:
            snapshot_file = os.path.join(tmp, 'loaded.z80')
            run_tool(tap2sna.main, ['--start', str(JVM_ADDR), args.tape, snapshot_file])
            if Snapshot.get(snapshot_file).pc != JVM_ADDR:
                raise Failure('the tape did not start the JVM at {}'.format(JVM_ADDR))

            font = load_font()
            keys = []       # what is left to type of the current input line
            line = None
            resume = []
            for step in range(MAX_STEPS):
                previous = snapshot_file
                snapshot_file = os.path.join(tmp, 'step{}.z80'.format(step))
                outputs = [snapshot_file] + ([args.png] if args.png else [])
                run_tool(trace.main, resume + [
                    '--stop', str(WAIT_KEY),
                    '--max-tstates', str(args.max_seconds * T_STATES_PER_SECOND),
                    previous] + outputs)
                snapshot = Snapshot.get(snapshot_file)
                screen = screen_text(snapshot.memory, font)
                if snapshot.pc != WAIT_KEY:
                    transcript.append(screen)
                    print('\n'.join(transcript))
                    print('tap2text.py: did not finish within {} s'.format(args.max_seconds),
                          file=sys.stderr)
                    return 3

                caller = snapshot.memory[snapshot.sp] | snapshot.memory[snapshot.sp + 1] << 8
                if caller >= JVM_ADDR and args.screen:
                    with open(args.screen, 'wb') as f:
                        f.write(bytes(snapshot.memory[SCREEN_ADDR:SCREEN_ADDR + 6912]))
                if caller == SCROLL_RETURN or caller >= JVM_ADDR:
                    transcript.append(screen + '\n--- key pressed')
                    key = KEY_ENTER
                elif caller == EDITOR_RETURN and snapshot.memory[FLAGX] & 0x20:
                    if not keys:
                        if not lines:
                            transcript.append(screen)
                            raise Failure('the program wants more input than the input file has')
                        line = lines.pop(0)
                        keys = line_keys(line)
                    key = keys.pop(0)
                    if not keys:
                        # Before ENTER, let the editor show the line, and record it
                        previous = snapshot_file
                        snapshot_file = os.path.join(tmp, 'step{}-shown.z80'.format(step))
                        run_tool(trace.main, ['--max-tstates', str(SHOW_TSTATES),
                                              previous, snapshot_file])
                        snapshot = Snapshot.get(snapshot_file)
                        transcript.append(screen_text(snapshot.memory, font)
                                          + '\n--- typed: ' + line)
                elif caller == EDITOR_RETURN:
                    transcript.append(screen)
                    break
                else:
                    transcript.append(screen)
                    raise Failure('waiting for a key, called from {}'.format(caller))
                resume = press(key, snapshot)
            else:
                raise Failure('more than {} keys pressed'.format(MAX_STEPS))
            if lines:
                raise Failure('{} input lines were not read'.format(len(lines)))
    except Failure as e:
        if transcript:
            print('\n'.join(transcript))
        print('tap2text.py: {}'.format(e), file=sys.stderr)
        return 1
    print('\n'.join(transcript))
    return 0


if __name__ == '__main__':
    sys.exit(main())
