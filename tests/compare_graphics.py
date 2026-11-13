#!/usr/bin/env python3
"""
Compares what a graphics program drew on the Spectrum with what the desktop's
Java2D draws for it.

The Spectrum's screen is the --screen dump of tap2text.py. Render.java paints
the program's Canvas on an image, in the paper colour of the Spectrum's
screen. Pixels are compared as drawn or not drawn. The ink of each 8x8 cell
is compared too where Java drew in one colour only in that cell, as the
Spectrum has one ink per cell.

usage: compare_graphics.py [--diff FILE.png] screen.bin classdir ClassName

Prints the number of pixels drawn only by Java and only by the Spectrum,
and of cells with the wrong ink, one count per line. --diff writes an image:
pixels both drew in black, only Java in red, only the Spectrum in blue.
"""

import argparse
import collections
import os
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def spectrum_pixels(screen):
    """The bits of the screen, as 192 rows of 256 booleans"""
    rows = []
    for y in range(192):
        addr = ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)
        row = []
        for col in range(32):
            byte = screen[addr + col]
            row.extend(bool(byte & (0x80 >> b)) for b in range(8))
        rows.append(row)
    return rows


def write_png(path, rows):
    """rows of (r, g, b) tuples"""
    raw = b''.join(b'\0' + bytes(c for px in row for c in px) for row in rows)

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF))

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', len(rows[0]), len(rows), 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw)))
        f.write(chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser(usage='compare_graphics.py [--diff FILE.png] screen.bin classdir ClassName')
    parser.add_argument('--diff')
    parser.add_argument('screen')
    parser.add_argument('classdir')
    parser.add_argument('name')
    args = parser.parse_args()

    with open(args.screen, 'rb') as f:
        screen = f.read()
    attrs = screen[6144:6912]
    paper = collections.Counter((a >> 3) & 7 for a in attrs).most_common(1)[0][0]

    java = subprocess.run(
        ['java', '-cp', args.classdir, '-Djava.awt.headless=true',
         os.path.join(HERE, 'Render.java'), args.name, str(paper)],
        check=True, capture_output=True, text=True).stdout.split('\n')[:192]
    zx = spectrum_pixels(screen)

    only_java = only_zx = 0
    diff = []
    for y in range(192):
        row = []
        for x in range(256):
            j = java[y][x] != '.'
            z = zx[y][x]
            only_java += j and not z
            only_zx += z and not j
            row.append((0, 0, 0) if j and z else (255, 0, 0) if j else (0, 0, 255) if z else (255, 255, 255))
        diff.append(row)

    wrong_ink = 0
    for cell in range(768):
        cy, cx = divmod(cell, 32)
        colours = {java[cy * 8 + y][cx * 8 + x] for y in range(8) for x in range(8)} - {'.'}
        drawn = any(zx[cy * 8 + y][cx * 8 + x] for y in range(8) for x in range(8))
        if len(colours) == 1 and drawn and colours != {str(attrs[cell] & 7)}:
            wrong_ink += 1

    if args.diff:
        write_png(args.diff, diff)
    print('only Java: {}'.format(only_java))
    print('only Spectrum: {}'.format(only_zx))
    print('wrong ink: {}'.format(wrong_ink))
    return 0


if __name__ == '__main__':
    sys.exit(main())
