# SPDX-License-Identifier: GPL-2.0-or-later
"""Plain (unencrypted, uncompressed) PS4 SELF -> flat ELF, as ctrport/flat_loader.hpp does.

Usage: python -B tools/online/self_to_flat.py <eboot.bin> <output.elf>
The output is derived from the user's dump: write it only under runtime/user (git-ignored).
"""
import hashlib, struct, sys


def little(data, offset, size):
    return int.from_bytes(data[offset:offset + size], 'little')


def convert(data):
    if little(data, 0, 4) != 0x1d3d154f:
        raise SystemExit('not a SELF')
    descriptors = little(data, 24, 2)
    elf = 32 + descriptors * 32
    phoff, stride, count = little(data, elf + 32, 8), little(data, elf + 54, 2), little(data, elf + 56, 2)
    if stride != 56 or count > 128:
        raise SystemExit('program table rejected')
    size = phoff + count * stride
    for i in range(count):
        p = elf + phoff + i * stride
        size = max(size, little(data, p + 8, 8) + little(data, p + 32, 8))
    out = bytearray(size)
    out[:phoff + count * stride] = data[elf:elf + phoff + count * stride]
    for i in range(descriptors):
        d = 32 + i * 32
        flags = little(data, d, 8)
        if not flags & 0x800:
            continue
        if flags & 10:
            raise SystemExit('compressed/encrypted segment')
        index = (flags >> 20) & 4095
        p = elf + phoff + index * stride
        target, length = little(data, p + 8, 8), little(data, p + 32, 8)
        source = little(data, d + 8, 8)
        out[target:target + length] = data[source:source + length]
    return bytes(out)


if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    flat = convert(data)
    open(sys.argv[2], 'wb').write(flat)
    print('eboot', hashlib.sha256(data).hexdigest(), 'flat', hashlib.sha256(flat).hexdigest(), len(flat))
