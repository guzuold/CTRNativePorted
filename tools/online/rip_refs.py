# SPDX-License-Identifier: GPL-2.0-or-later
"""Finds strings in a flat PS4 ELF and the RIP-relative LEA instructions that reference them.

Usage: python -B tools/online/rip_refs.py <flat.elf> <string>...
Prints, for each occurrence of each string, its virtual address and every
`lea reg, [rip+disp32]` (REX.W 8D /r, mod=00 rm=101) whose target is that address, with
the bytes following the instruction so nearby immediates (sizes, key lengths) are visible.
"""
import re, struct, sys


def segments(data):
    phoff, stride, count = struct.unpack_from('<Q', data, 32)[0], *struct.unpack_from('<HH', data, 54)
    out = []
    for i in range(count):
        p = phoff + i * stride
        kind, flags, offset, vaddr, _, filesz, memsz = struct.unpack_from('<IIQQQQQ', data, p)
        if kind in (1, 0x61000010) and filesz:
            out.append((offset, vaddr, filesz, flags))
    return out


def to_vaddr(segs, offset):
    for o, v, n, _ in segs:
        if o <= offset < o + n:
            return v + offset - o
    return None


def lea_targets(data, segs):
    for o, v, n, flags in segs:
        if not flags & 1:  # executable segments only
            continue
        blob = data[o:o + n]
        for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', blob):
            i = m.start()
            disp = struct.unpack_from('<i', blob, i + 3)[0]
            yield v + i, v + i + 7 + disp


def main():
    data = open(sys.argv[1], 'rb').read()
    segs = segments(data)
    wanted = {}
    for text in sys.argv[2:]:
        for m in re.finditer(re.escape(text.encode()) + b'\x00', data):
            va = to_vaddr(segs, m.start())
            if va is not None:
                wanted[va] = text
                print('string %-24s at 0x%x' % (text, va))
    by_target = {}
    for at, target in lea_targets(data, segs):
        if target in wanted:
            by_target.setdefault(target, []).append(at)
    for target, sites in sorted(by_target.items()):
        for at in sites:
            offset = next(o + at - v for o, v, n, _ in segs if v <= at < v + n)
            print('  lea 0x%x -> 0x%x (%s) next: %s' % (at, target, wanted[target],
                                                    data[offset + 7:offset + 39].hex()))


if __name__ == '__main__':
    main()
