"""Address map between the EU (CUSA14876) and US (CUSA13795) eboots of the same version.

The two regions are built from the same code: the US eboot holds the same functions in the same
order, some of them shifted by 0x10..0x40, and the same data at a few constant shifts. This tool
proves the correspondence from the two eboots and writes the ranges where it holds, so that the
runtime can move a patch written for one region onto the other (ctrport/region_port.hpp):

- functions: the unwind table (eh_frame_hdr) lists every function; the n-th EU function and the
  n-th US function must have the same size, the same bytes apart from relative branch and
  RIP-relative displacements, and every such reference must reach the corresponding place (code
  through this map, data through the data ranges, consistent across all references);
- the import stubs after the last function: same instructions, references consistent;
- data: every address that the paired functions reference gives one EU -> US pair; runs of pairs
  with the same shift make the data ranges. Inside them, the spans whose bytes differ between the
  regions (after pointers and self-relative offsets into mapped code or data are matched) are
  listed, so that a patch writing data there is refused.

The output holds offsets and sizes only, no game bytes.

  python tools/region_map.py --pair <EU eboot> <US eboot> [--pair ...] [--out ctrport/region_map_data.hpp]
  python tools/region_map.py --check-decoder <x86_length_check.exe> <eboot> [...]

--check-decoder runs ctrport/x86_length_check.cpp (built with
  cl /nologo /O2 /EHsc /std:c++17 /D_CRT_SECURE_NO_WARNINGS ctrport/x86_length_check.cpp)
on every function of each eboot and compares each instruction's length and displacement and
immediate fields with capstone. Needs capstone and numpy.
"""
import argparse, bisect, collections, hashlib, struct, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
from capstone import x86_const
sys.path.insert(0, str(Path(__file__).parent))
import retail_anchors as ra

RIP = x86_const.X86_REG_RIP
VERSIONS = {  # sha256 -> (region, version)
    'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1': ('EU', '01.00'),
    '39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5': ('EU', '01.21'),
    '23ddf3909d87a42a4c74be395865b130e4128b892de59d71082d8620cd8cd858': ('US', '01.00'),
    'e3b7a068b5e7be0f976102aa223a74425e7817c67e26e21416163291750c1b83': ('US', '01.21'),
}


def functions(image):
    """(start, size) of every function in the eh_frame_hdr search table."""
    m = image.memory
    at = m.find(b'\x01\x1b\x03\x3b')
    while at >= 0:
        count = struct.unpack_from('<I', m, at + 8)[0]
        if 1000 < count < 1000000:
            break
        at = m.find(b'\x01\x1b\x03\x3b', at + 1)
    if at < 0:
        raise ValueError('no eh_frame_hdr search table')
    out = []
    for k in range(count):
        loc, fde = struct.unpack_from('<ii', m, at + 12 + k * 8)
        start, fde = at + loc, at + fde
        begin, size = struct.unpack_from('<ii', m, fde + 8)
        if fde + 8 + begin != start:
            raise ValueError(f'FDE of {start:#x} does not match the table')
        out.append((start, size))
    return out


def references(insn):
    """(field offset in the instruction, kind, target) of a disp32/rel32 reference."""
    if any(op.type == x86_const.X86_OP_MEM and op.mem.base == RIP for op in insn.operands):
        # capstone reports 2 displacement bytes under a 66 prefix; RIP-relative is always disp32
        return insn.disp_offset, 'data', insn.address + insn.size + insn.disp
    if insn.imm_size == 4 and (insn.group(x86_const.X86_GRP_CALL) or insn.group(x86_const.X86_GRP_JUMP)) \
            and insn.operands and insn.operands[0].type == x86_const.X86_OP_IMM:
        return insn.imm_offset, 'code', insn.operands[0].imm
    return None


def decode(image, cs, start, size):
    """Masked bytes and references of [start, start+size), or None when it does not decode."""
    code = image.memory[start:start + size]
    mask = bytearray(code)
    refs, at = [], start
    for insn in cs.disasm(code, start):
        at = insn.address + insn.size
        ref = references(insn)
        if ref:
            field = insn.address - start + ref[0]
            mask[field:field + 4] = b'\0' * 4
            refs.append((field, ref[1], ref[2]))
    if at != start + size:
        return None
    return hashlib.blake2b(bytes(mask), digest_size=16).digest(), refs


class RangeMap:
    """EU offset -> US offset by sorted, non-overlapping ranges (lo, hi, delta)."""
    def __init__(self, ranges=()):
        self.ranges = sorted(ranges)
        self.starts = [r[0] for r in self.ranges]

    def __call__(self, eu):
        i = bisect.bisect_right(self.starts, eu) - 1
        if i >= 0 and eu < self.ranges[i][1]:
            return eu + self.ranges[i][2]
        return None


def merge(ranges, gap_ok):
    out = []
    for lo, hi, d in sorted(ranges):
        if out and out[-1][2] == d and (out[-1][1] == lo or (lo > out[-1][1] and gap_ok(out[-1][1], lo, d))):
            out[-1][1] = hi
        else:
            out.append([lo, hi, d])
    return [tuple(r) for r in out]


def build(eu_path, us_path):
    eu, us = ra.Image(eu_path), ra.Image(us_path)
    (r1, v1), (r2, v2) = VERSIONS.get(eu.sha256, ('?', '?')), VERSIONS.get(us.sha256, ('?', '?'))
    if (r1, r2) != ('EU', 'US') or v1 != v2:
        sys.exit(f'need the EU and US eboots of one version, got {r1} {v1} and {r2} {v2}')
    print(f'{v1}: EU {eu.sha256[:12]}, US {us.sha256[:12]}', flush=True)
    cs = ra.disassembler()
    fe, fu = functions(eu), functions(us)
    if len(fe) != len(fu):
        sys.exit(f'function counts differ ({len(fe)} EU, {len(fu)} US): the pairing by order does not hold')
    print(f'  {len(fe)} functions each', flush=True)
    pairs = []  # (eu start, us start, size, eu refs, us refs)
    rejected = []
    for n, ((a, la), (b, lb)) in enumerate(zip(fe, fu)):
        da, db = decode(eu, cs, a, la), decode(us, cs, b, lb)
        if la != lb or not da or not db or da[0] != db[0] or [r[:2] for r in da[1]] != [r[:2] for r in db[1]]:
            rejected.append((a, 'code differs'))
            continue
        pairs.append((a, b, la, da[1], db[1]))
        if n % 20000 == 0:
            print(f'  decoded {n}', flush=True)
    # references must agree; a function whose code reference lands elsewhere is dropped and the
    # check repeats, since dropping it changes the map
    while True:
        code = RangeMap((a, a + size, b - a) for a, b, size, _, _ in pairs)
        bad = set()
        data, stubs = collections.defaultdict(set), collections.defaultdict(set)
        for a, b, size, ra_, rb in pairs:
            for (_, kind, ta), (_, _, tb) in zip(ra_, rb):
                mapped = code(ta)
                if mapped is not None:
                    if mapped != tb:
                        bad.add(a)
                elif kind == 'data':
                    data[ta].add(tb - ta)
                else:
                    stubs[ta].add(tb - ta)  # import stubs and other code outside the functions
        if not bad:
            break
        rejected += [(a, 'reference lands elsewhere') for a in bad]
        pairs = [p for p in pairs if p[0] not in bad]
    print(f'  paired {len(pairs)}, rejected {len(rejected)}: ' +
          ', '.join(f'{a:#x} ({why})' for a, why in rejected[:8]), flush=True)
    # data shifts from the references, runs of equal shift; ambiguous targets are left out
    single = sorted((t, next(iter(s))) for t, s in data.items() if len(s) == 1)
    runs = []
    for t, d in single:
        if runs and runs[-1][2] == d:
            runs[-1][1] = t
            runs[-1][3] += 1
        else:
            runs.append([t, t, d, 1])
    noise = [r for r in runs if r[3] < 16]
    runs = [r for r in runs if r[3] >= 16]
    print(f'  data runs {len(runs)} (+{len(noise)} short ones left out): ' +
          ', '.join(f'{r[0]:#x}-{r[1]:#x} {r[2]:+#x} n={r[3]}' for r in runs), flush=True)
    last_function = max(a + size for a, _, size, _, _ in pairs)
    code_ranges = [(a, a + size, b - a) for a, b, size, _, _ in pairs]

    def same_gap(lo, hi, d):
        return eu.memory[lo:hi] == us.memory[lo + d:hi + d]
    code_ranges = merge(code_ranges, same_gap)
    # import stubs: from the last function to the first data run, if they all decode the same
    if runs and runs[0][0] >= last_function:
        stub_end = runs[0][0]
        stub_shift = collections.Counter(d for t, s in stubs.items() if last_function <= t < stub_end for d in s)
        if stub_shift:
            d = stub_shift.most_common(1)[0][0]
            ia = list(cs.disasm(eu.memory[last_function:stub_end], last_function))
            ib = list(cs.disasm(us.memory[last_function + d:stub_end + d], last_function + d))
            dmap = RangeMap((r[0], r[1] + 1, r[2]) for r in runs)
            ok = len(ia) == len(ib) and all(
                x.size == y.size and x.mnemonic == y.mnemonic and
                (references(x) is None) == (references(y) is None) and
                (references(x) is None or (dmap(references(x)[2]) or code(references(x)[2])) == references(y)[2])
                for x, y in zip(ia, ib))
            if ok:
                code_ranges.append((last_function, stub_end, d))
                print(f'  import stubs {last_function:#x}-{stub_end:#x} {d:+#x}: {len(ia)} instructions', flush=True)
            else:
                print('  import stubs do not match; left out', flush=True)
    code_ranges = merge(code_ranges, same_gap)
    # data ranges: each run from the end of what precedes it (or its first segment's start) to the
    # next run (or its last segment's end), cut where segments end
    segments = eu.segments
    data_ranges = []
    for i, (lo, hi, d, _) in enumerate(runs):
        first = next(s for s in segments if s[0] <= lo < s[1])
        last = next(s for s in segments if s[0] <= hi < s[1])
        begin = max([first[0]] + [r[1] for r in code_ranges if r[1] <= lo] + [r[1] for r in data_ranges])
        end = min([last[1], len(us.memory) - d] + [r[0] for r in runs[i + 1:]])
        for s_lo, s_hi in segments:
            if max(begin, s_lo) < min(end, s_hi):
                data_ranges.append((max(begin, s_lo), min(end, s_hi), d))
    data_map = RangeMap(data_ranges)
    code = RangeMap(code_ranges)
    # spans whose bytes differ between the regions, pointers matched through the map
    E = np.frombuffer(eu.memory, np.uint8)
    U = np.frombuffer(us.memory, np.uint8)
    differs = []
    for lo, hi, d in data_ranges:
        diff = np.nonzero(E[lo:hi] != U[lo + d:hi + d])[0] + lo
        # pointers (8 bytes, after relocation) and 32-bit self-relative offsets (unwind and
        # exception tables) that reach the corresponding place in both regions are the same
        def mapped(target):
            return code(target) if code(target) is not None else data_map(target)
        same = set()
        for q in sorted(set(int(x) & ~7 for x in diff)):
            if q + 8 <= hi and mapped(struct.unpack_from('<Q', eu.memory, q)[0]) ==                     struct.unpack_from('<Q', us.memory, q + d)[0]:
                same.update(range(q, q + 8))
        for q in sorted(set(int(x) & ~3 for x in diff) - same):
            if q + 4 <= hi and mapped(q + struct.unpack_from('<i', eu.memory, q)[0]) ==                     q + d + struct.unpack_from('<i', us.memory, q + d)[0]:
                same.update(range(q, q + 4))
        spans = []
        for x in diff:
            x = int(x)
            if x in same:
                continue
            if spans and x <= spans[-1][1] + 8:
                spans[-1][1] = x + 1
            else:
                spans.append([x, x + 1])
        differs += [tuple(s) for s in spans]
    print(f'  code ranges {len(code_ranges)}, data ranges {len(data_ranges)}, differing data spans {len(differs)} '
          f'({sum(h - l for l, h in differs)} bytes)', flush=True)
    return dict(version=v1, eu=eu.sha256, us=us.sha256, code=code_ranges, data=data_ranges, differs=differs,
                functions=len(fe), paired=len(pairs))


def emit(maps, path):
    lines = [
        '// SPDX-License-Identifier: GPL-2.0-only',
        '// Generated by tools/region_map.py from the EU (CUSA14876) and US (CUSA13795) eboots. Do not edit.',
        '// Offsets in the EU image; the US offset is EU + delta. Code ranges cover functions proven',
        '// identical in both regions (references included); data ranges are shifted as the code that',
        '// reads them shows; differs lists data spans whose bytes are not the same in both regions.',
        '#pragma once',
        '#include <cstddef>',
        '#include <cstdint>',
        'namespace ctrport::region_map {',
        'struct Range { std::uint32_t eu, size; std::int32_t delta; };',
        'struct Span { std::uint32_t eu, size; };',
        'struct Pair {',
        '    const char* version;',
        '    const char* eu_sha256;',
        '    const char* us_sha256;',
        '    const Range* code; std::size_t code_count;',
        '    const Range* data; std::size_t data_count;',
        '    const Span* differs; std::size_t differs_count;',
        '};',
    ]
    for m in maps:
        tag = m['version'].replace('.', '')
        lines.append(f'// {m["version"]}: {m["paired"]} of {m["functions"]} functions paired')
        for name, rows, fmt in (('code', m['code'], 'range'), ('data', m['data'], 'range'), ('differs', m['differs'], 'span')):
            lines.append(f'inline constexpr {"Range" if fmt == "range" else "Span"} {name}_{tag}[] = {{')
            row = []
            for r in rows:
                row.append(f'{{{r[0]:#x},{r[1] - r[0]:#x},{r[2]}}}' if fmt == 'range' else f'{{{r[0]:#x},{r[1] - r[0]}}}')
                if len(row) == 6:
                    lines.append('    ' + ','.join(row) + ',')
                    row = []
            if row:
                lines.append('    ' + ','.join(row) + ',')
            if not rows:
                lines.append('    {0, 0' + (', 0' if fmt == 'range' else '') + '},')
            lines.append('};')
    lines.append('inline constexpr Pair pairs[] = {')
    for m in maps:
        tag = m['version'].replace('.', '')
        lines.append(f'    {{"{m["version"]}", "{m["eu"]}", "{m["us"]}",')
        lines.append(f'     code_{tag}, {len(m["code"])}, data_{tag}, {len(m["data"])}, differs_{tag}, {len(m["differs"])}}},')
    lines += ['};', '} // namespace ctrport::region_map', '']
    Path(path).write_text('\n'.join(lines), encoding='utf-8', newline='\n')


def check_decoder(exe, paths):
    cs = ra.disassembler()
    failures = 0
    for path in paths:
        image = ra.Image(path)
        funcs = functions(image)
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            (tmp / 'image.bin').write_bytes(image.memory)
            (tmp / 'functions.bin').write_bytes(b''.join(struct.pack('<QQ', a, s) for a, s in funcs))
            subprocess.run([exe, str(tmp / 'image.bin'), str(tmp / 'functions.bin'), str(tmp / 'out.bin')], check=True)
            records = (tmp / 'out.bin').read_bytes()
        pos, total, bad = 0, 0, collections.Counter()
        for start, size in funcs:
            mine = []
            while records[pos:pos + 6] != b'\xff' * 6:
                mine.append(records[pos:pos + 6])
                pos += 6
            pos += 6
            for k, insn in enumerate(cs.disasm(image.memory[start:start + size], start)):
                total += 1
                ref = references(insn)
                disp = (insn.disp_offset, 4 if ref and ref[1] == 'data' else insn.disp_size) if insn.disp_size else (0, 0)
                imm = (insn.imm_offset, insn.imm_size) if insn.imm_size else (0, 0)
                want = (insn.size,) + disp + imm
                got = tuple(mine[k][:5]) if k < len(mine) else None
                if got is not None and want[2] == 2 and got[2] == 4 and got[:2] == want[:2] and got[3:] == want[3:]:
                    want = got  # capstone: 2-byte displacement reported under a 66 prefix
                if got is not None and got[:3] == want[:3] and insn.mnemonic in ('insertq', 'extrq') and                         got[3] <= want[3] and got[3] + got[4] >= want[3] + want[4]:
                    want = got  # capstone lists one of the two immediates of the SSE4a forms
                rip = bool(ref and ref[1] == 'data')
                if got != want or bool(mine[k][5] & 1) != rip:
                    bad[insn.bytes[:3].hex() + ' ' + insn.mnemonic] += 1
                    break
        print(f'{path}: {total} instructions in {len(funcs)} functions, {sum(bad.values())} functions differ')
        for key, n in bad.most_common(20):
            print(f'  {n:6} {key}')
        failures += sum(bad.values())
    return failures


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pair', nargs=2, action='append', metavar=('EU', 'US'))
    parser.add_argument('--out')
    parser.add_argument('--check-decoder', nargs='+', metavar=('EXE', 'EBOOT'))
    args = parser.parse_args()
    if args.check_decoder:
        sys.exit(1 if check_decoder(args.check_decoder[0], args.check_decoder[1:]) else 0)
    maps = [build(eu, us) for eu, us in args.pair or []]
    if args.out:
        emit(maps, args.out)
        print('written', args.out)


if __name__ == '__main__':
    main()
