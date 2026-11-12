#!/usr/bin/env python3
"""Runs src/double.c on an emulated Z80 and compares it with Python's floats.

usage: z80_test.py [--batches N] driver.bin

driver.bin is tests/double/z80_driver.c compiled for address 32768. Each
batch puts random test vectors in memory, runs the driver in SkoolKit's Z80
simulator and checks every result. Python's float is an IEEE 754 binary64
with round to nearest, ties to even, as Java's double is.
"""

import argparse
import contextlib
import io
import math
import random
import struct
import sys

from skoolkit import CSimulator
from skoolkit.simulator import Simulator
from skoolkit.trace import Tracer

ORG = 32768
COUNT = 0xBFFE
VECTORS = 0xC000
VECTOR_SIZE = 25
PER_BATCH = (0x10000 - VECTORS) // VECTOR_SIZE
STOP = 0xBFF0           # where main returns to, as to BASIC after USR
STACK = 0xBFE0

OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_CMPL, OP_CMPG, OP_D2I, OP_I2D, OP_NEG = range(9)
OP_NAMES = ['add', 'sub', 'mul', 'div', 'cmpl', 'cmpg', 'd2i', 'i2d', 'neg']

SPECIALS = [
    0x0000000000000000, 0x8000000000000000,     # zeros
    0x0000000000000001, 0x8000000000000001,     # smallest subnormals
    0x000FFFFFFFFFFFFF, 0x0010000000000000,     # largest subnormal, smallest normal
    0x7FEFFFFFFFFFFFFF, 0xFFEFFFFFFFFFFFFF,     # largest finite
    0x7FF0000000000000, 0xFFF0000000000000,     # infinities
    0x7FF8000000000000, 0x7FF0000000000001,     # NaNs
    0x3FF0000000000000, 0xBFF0000000000000,     # 1, -1
    0x3FF0000000000001, 0x3FEFFFFFFFFFFFFF,     # around 1
    0x4340000000000000, 0x41DFFFFFFFC00000,     # 2^53, 2^31 - 1
    0xC1E0000000000000, 0x41E0000000000000,     # -2^31, 2^31
    0x3FB999999999999A, 0x3FC999999999999A,     # 0.1, 0.2
]


def to_float(u):
    return struct.unpack('<d', struct.pack('<Q', u))[0]


def to_bits(d):
    return struct.unpack('<Q', struct.pack('<d', d))[0]


def random_double(rng, near):
    """Doubles of several shapes, so that rounding, cancellation and subnormals all occur."""
    r = rng.getrandbits(64)
    shape = rng.randrange(8)
    if shape == 0:
        return r
    if shape == 1:
        return rng.choice(SPECIALS)
    if shape == 2:      # exponent close to the other operand
        exp = min(max(((near >> 52) & 0x7FF) + rng.randrange(-3, 4), 0), 0x7FF)
        return (exp << 52) | (r & 0x800FFFFFFFFFFFFF)
    if shape == 3:      # subnormal or tiny
        return r & 0x801FFFFFFFFFFFFF
    if shape == 4:      # few significant bits, for exact ties
        return (r & 0xFFFFF00000000000) | rng.choice([0, 0x0000080000000000])
    if shape == 5:      # near overflow
        return (r & 0x800FFFFFFFFFFFFF) | ((0x7FD + rng.randrange(3)) << 52)
    if shape == 6:      # a small integer
        return to_bits(float(rng.randrange(-1000, 1001)))
    return (r & 0x800FFFFFFFFFFFFF) | ((0x3F0 + rng.randrange(32)) << 52)


def java_div(a, b):
    if b == 0.0:
        if a == 0.0 or math.isnan(a):
            return math.nan
        return math.copysign(math.inf, a) * math.copysign(1.0, b)
    return a / b


def java_d2i(a):
    if math.isnan(a):
        return 0
    if a >= 2147483647.0:
        return 2147483647
    if a <= -2147483648.0:
        return -2147483648
    return int(a)


def java_cmp(a, b, nan_result):
    if math.isnan(a) or math.isnan(b):
        return nan_result
    return -1 if a < b else 1 if a > b else 0


def expected(op, a, b):
    """The expected 64-bit result: a double's bits, or an int's in the high word."""
    x, y = to_float(a), to_float(b)
    if op == OP_ADD:
        return to_bits(x + y)
    if op == OP_SUB:
        return to_bits(x - y)
    if op == OP_MUL:
        return to_bits(x * y)
    if op == OP_DIV:
        return to_bits(java_div(x, y))
    if op == OP_CMPL:
        return (java_cmp(x, y, -1) & 0xFFFFFFFF) << 32
    if op == OP_CMPG:
        return (java_cmp(x, y, 1) & 0xFFFFFFFF) << 32
    if op == OP_D2I:
        return (java_d2i(x) & 0xFFFFFFFF) << 32
    if op == OP_I2D:
        i = a >> 32
        return to_bits(float(i - (1 << 32) if i >= 1 << 31 else i))
    return a ^ 0x8000000000000000         # OP_NEG


def pack_double(u):
    return struct.pack('<II', u >> 32, u & 0xFFFFFFFF)


def unpack_double(data):
    hi, lo = struct.unpack('<II', data)
    return (hi << 32) | lo


def is_nan(u):
    return (u & 0x7FFFFFFFFFFFFFFF) > 0x7FF0000000000000


def run_batch(rom, driver, vectors):
    memory = bytearray(0x10000)
    memory[:len(rom)] = rom
    memory[ORG:ORG + len(driver)] = driver
    memory[COUNT:COUNT + 2] = struct.pack('<H', len(vectors))
    for i, (op, a, b) in enumerate(vectors):
        addr = VECTORS + i * VECTOR_SIZE
        memory[addr:addr + 17] = bytes([op]) + pack_double(a) + pack_double(b)
    memory[STACK:STACK + 2] = struct.pack('<H', STOP)
    simulator = (CSimulator or Simulator)(memory, {'SP': STACK, 'IY': 0x5C3A})
    tracer = Tracer(simulator, 7, 0, 0, [None] * 16, 0, None)
    simulator.set_tracer(tracer)
    with contextlib.redirect_stdout(io.StringIO()):      # "Stopped at ..."
        tracer.run(ORG, STOP, 0, 0, False, None, None, None, None, '', '', '')
    memory = simulator.memory       # the C simulator works on a copy
    results = []
    for i in range(len(vectors)):
        addr = VECTORS + i * VECTOR_SIZE + 17
        results.append(unpack_double(memory[addr:addr + 8]))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--batches', type=int, default=8)
    parser.add_argument('driver')
    args = parser.parse_args()

    import skoolkit
    rom_path = skoolkit.__file__.rpartition('/')[0] + '/resources/48.rom'
    with open(rom_path, 'rb') as f:
        rom = f.read()
    with open(args.driver, 'rb') as f:
        driver = f.read()

    rng = random.Random(1)
    vectors = [(op, a, b) for a in SPECIALS for b in SPECIALS for op in range(9)]
    total = 0
    failures = 0
    for batch in range(args.batches + (len(vectors) + PER_BATCH - 1) // PER_BATCH):
        if not vectors:
            while len(vectors) < PER_BATCH:
                op = rng.randrange(9)
                if op == OP_I2D:
                    i = rng.getrandbits(32) >> rng.choice([0, 0, rng.randrange(32)])
                    vectors.append((op, i << 32, 0))
                else:
                    a = random_double(rng, 0)
                    vectors.append((op, a, random_double(rng, a)))
        chunk, vectors = vectors[:PER_BATCH], vectors[PER_BATCH:]
        for (op, a, b), got in zip(chunk, run_batch(rom, driver, chunk)):
            want = expected(op, a, b)
            if op in (OP_CMPL, OP_CMPG, OP_D2I):
                got &= 0xFFFFFFFF00000000
            if got != want and not (is_nan(got) and is_nan(want)):
                failures += 1
                if failures <= 20:
                    print(f'FAIL {OP_NAMES[op]} {a:016x} {b:016x}: got {got:016x}, want {want:016x}')
        total += len(chunk)
    print(f'{total} operations on the Z80, {failures} failures')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
