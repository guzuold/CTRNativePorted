"""Code signatures that find the EU v01.21 bindings in another retail eboot.

The settings, the native graphics options and other runtime features patch addresses of the
EU v01.21 eboot (CUSA14876, 39d716b7...). The other retail eboots (US CUSA13795 v01.00 and
v01.21, EU v01.00) contain the same functions at shifted addresses. For every binding this
tool cuts a window of the EU v01.21 code around it, masks the bytes that change when code
moves (call targets, RIP-relative displacements, jumps leaving the window) and checks that the
window is unique. ctrport/retail_map.hpp searches these windows in the running eboot.

  python tools/retail_anchors.py <EU v01.21 eboot.bin> [--out ctrport/retail_anchors.hpp]
  python tools/retail_anchors.py <EU v01.21 eboot.bin> --check <other eboot.bin>

--check resolves every binding in another eboot with the same rules as the runtime, and the
addresses already known for EU v01.00 (retail_profiles.hpp, d3d/engine.h, unlock_all.hpp).
Needs capstone (pip install capstone) and numpy; the runtime needs neither.
"""
import argparse, hashlib, struct, sys
from pathlib import Path
import numpy as np
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, x86_const

REFERENCE_SHA = '39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5'
EU100_SHA = 'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1'

# name, EU v01.21 image offset, kind. A data binding is found through the code that uses it.
POINTS = [
    ('retail_graphics.screen_space_reflections', 0x36d470, 'code'),
    ('retail_graphics.bloom', 0x963d00, 'code'),
    ('retail_graphics.motion_blur', 0xaf2540, 'code'),
    ('retail_graphics.vignette', 0xaf204a, 'code'),
    ('retail_graphics.depth_of_field', 0xaf8dfb, 'code'),
    ('graphics.aa_gate', 0x961b51, 'code'),
    ('geometry.shadow_branch', 0x3576a1, 'code'),
    ('geometry.distance_setter', 0x34f200, 'code'),
    ('quality.mesh_branch', 0xeadfce, 'code'),
    ('quality.texture_load', 0x141be9c, 'code'),
    ('quality.world_encoder', 0x348350, 'code'),
    ('vfx.producer', 0xaf056e, 'code'),
    ('hero.publisher', 0x950781, 'code'),
    ('hero.serializer', 0x950503, 'code'),
    ('shadow.configure', 0x357794, 'code'),
    ('shadow.publisher', 0x357072, 'code'),
    ('bridge.selector_slot0', 0x127f4c0, 'code'),
    ('bridge.event', 0x127f500, 'code'),
    ('bridge.index', 0x127f5d0, 'code'),
    ('bridge.setter', 0x125d880, 'code'),
    # Found after relocation as the table whose slots 0, 25 and 33 point at these functions.
    ('bridge.selector_vtable', 0x27746c8, 'vtable:bridge.selector_slot0@0,bridge.event@25,bridge.index@33'),
    ('bridge.load_event_type', 0x2a21ee8, 'data'),
    ('native.render_static', 0x2a7f420, 'data'),
    ('native.fxaa_static', 0x2a608b0, 'data'),
    ('native.smaa_static', 0x2a27865, 'data'),
    ('online.reward_system', 0x2ad9c38, 'data'),
    ('online.marketplace', 0x2ac8348, 'data'),
    ('online.shop_constant_price', 0xab21a0, 'code'),
    ('online.shop_bundle', 0xab1620, 'code'),
    # Cross-checks only: these also have offsets recorded per eboot (retail_profiles.hpp),
    # which the runtime compares with what it finds.
    ('check.startup_worker', 0x14cfb10, 'code'),
    ('check.fps60_store', 0x13a9d80, 'code'),
    ('check.scale4k_load', 0x13aa652, 'code'),
]
# EU v01.21 -> EU v01.00 offsets established elsewhere, used by --check on EU v01.00.
KNOWN_EU100 = {
    0x13c9e40: 0x1306ba0, 0x13cb1c0: 0x1307f20, 0x2aebce0: 0x26e7920,  # d3d/engine.h
    0x9f0cf0: 0x9de850, 0x2ab1bd0: 0x26b1320,                           # unlock_all.hpp
    0x14cfb10: 0x1409af0, 0x2a0cb50: 0x260cb50, 0x13aa652: 0x12e74c2,  # retail_profiles.hpp
}
KNOWN_KINDS = {0x2aebce0: 'data', 0x2ab1bd0: 'data', 0x2a0cb50: 'data'}


class Image:
    """The eboot as flat_loader.hpp maps it: image offset = ELF virtual address."""
    def __init__(self, path):
        data = Path(path).read_bytes()
        self.sha256 = hashlib.sha256(data).hexdigest()
        if data[:4] == b'O\x15=\x1d':
            count = struct.unpack_from('<H', data, 24)[0]
            elf = 32 + count * 32
            phoff, = struct.unpack_from('<Q', data, elf + 32)
            phnum, = struct.unpack_from('<H', data, elf + 56)
            headers = [struct.unpack_from('<II6Q', data, elf + phoff + i * 56) for i in range(phnum)]
            size = max(phoff + phnum * 56, max(h[2] + h[5] for h in headers))
            logical = bytearray(size)
            logical[:phoff + phnum * 56] = data[elf:elf + phoff + phnum * 56]
            for i in range(count):
                flags, source, available, _ = struct.unpack_from('<4Q', data, 32 + i * 32)
                if not flags & 0x800:
                    continue
                if flags & 10:
                    raise ValueError('encrypted or compressed SELF')
                h = headers[(flags >> 20) & 4095]
                logical[h[2]:h[2] + h[5]] = data[source:source + h[5]]
            data = bytes(logical)
        phoff, = struct.unpack_from('<Q', data, 32)
        phnum, = struct.unpack_from('<H', data, 56)
        headers = [struct.unpack_from('<II6Q', data, phoff + i * 56) for i in range(phnum)]
        loads = [h for h in headers if h[0] in (1, 0x61000010)]
        memory = bytearray(max(h[3] + h[6] for h in loads))
        for h in loads:
            memory[h[3]:h[3] + h[5]] = data[h[2]:h[2] + h[5]]
        # Pointers as the loader relocates them, with the image at 0: a data pointer holds an
        # image offset (the runtime subtracts the image base).
        dynamic = next(h for h in headers if h[0] == 2)
        link = next(h for h in headers if h[0] == 0x61000000)
        tags = dict(struct.iter_unpack('<qQ', data[dynamic[2]:dynamic[2] + dynamic[5]]))
        blob = data[link[2]:link[2] + link[5]]
        for address_tag, size_tag in ((0x61000029, 0x6100002d), (0x6100002f, 0x61000031)):
            if address_tag not in tags:
                continue
            for offset, info, addend in struct.iter_unpack('<QQq', blob[tags[address_tag]:tags[address_tag] + tags[size_tag]]):
                value = None
                if info & 0xffffffff == 8:
                    value = addend
                elif info & 0xffffffff == 1:
                    _, _, _, section, address, _ = struct.unpack_from('<IBBHQQ', blob, tags[0x61000039] + (info >> 32) * 24)
                    value = address + addend if section else None
                if value is not None and offset + 8 <= len(memory):
                    struct.pack_into('<Q', memory, offset, value)
        self.memory = bytes(memory)
        self.code = [(h[3], h[3] + h[5]) for h in loads if h[1] & 1]


def disassembler():
    cs = Cs(CS_ARCH_X86, CS_MODE_64)
    cs.detail = True
    return cs


def function_decode(image, cs, address, inside=False):
    """Start of the function containing address ('push rbp; mov rbp,rsp'), decoded so that an
    instruction starts at address, or with inside=True contains it."""
    start = address
    for _ in range(64):
        start = image.memory.rfind(b'\x55\x48\x89\xe5', max(0, start - 0x40000), start + 1)
        if start < 0:
            break
        out, at = [], start
        for insn in cs.disasm(image.memory[start:address + 0x800], start):
            out.append(insn)
            at = insn.address + insn.size
            if insn.address >= address:
                break
        if any(i.address == address or (inside and i.address < address < i.address + i.size) for i in out):
            return start
        start -= 1
    raise ValueError(f'no instruction boundary found at {address:#x}')


def window(image, cs, address, pre, post):
    """Masked window of whole instructions covering [address-pre, address+post)."""
    start = function_decode(image, cs, address)
    insns = []
    for insn in cs.disasm(image.memory[start:address + post + 64], start):
        if insn.address + insn.size <= address - pre:
            continue
        insns.append(insn)
        if insn.address + insn.size >= address + post:
            break
    if not insns or insns[-1].address + insns[-1].size < address + post:
        raise ValueError(f'code at {address:#x} does not decode far enough')
    begin, end = insns[0].address, insns[-1].address + insns[-1].size
    mask = bytearray(b'x' * (end - begin))
    for insn in insns:
        at = insn.address - begin
        rip = any(op.type == x86_const.X86_OP_MEM and op.mem.base == x86_const.X86_REG_RIP for op in insn.operands)
        if rip and insn.disp_size:
            mask[at + insn.disp_offset:at + insn.disp_offset + insn.disp_size] = b'?' * insn.disp_size
        if insn.group(x86_const.X86_GRP_CALL) or insn.group(x86_const.X86_GRP_JUMP):
            if insn.imm_size and insn.operands and insn.operands[0].type == x86_const.X86_OP_IMM:
                target = insn.operands[0].imm
                if insn.group(x86_const.X86_GRP_CALL) or not begin <= target < end:
                    mask[at + insn.imm_offset:at + insn.imm_offset + insn.imm_size] = b'?' * insn.imm_size
    return begin, image.memory[begin:end], bytes(mask), insns


def needle(pattern, mask):
    """Longest unmasked run: the part searched for before the masked compare."""
    best, run = (0, 0), None
    for i, m in enumerate(mask + b'?'):
        if m == ord('x'):
            run = i if run is None else run
        elif run is not None:
            if i - run > best[1] - best[0]:
                best = (run, i)
            run = None
    return best


def find(image, pattern, mask, limit=2):
    """Start offsets where the masked pattern occurs in the image's code, up to limit."""
    lo, hi = needle(pattern, mask)
    key = pattern[lo:hi]
    hits = []
    for begin, end in image.code:
        at = image.memory.find(key, begin + lo, end)
        while at >= 0:
            s = at - lo
            if s >= begin and s + len(pattern) <= end and all(
                    m != ord('x') or image.memory[s + i] == pattern[i] for i, m in enumerate(mask)):
                hits.append(s)
                if len(hits) >= limit:
                    return hits
            at = image.memory.find(key, at + 1, end)
    return hits


def code_anchor(image, cs, address):
    for pre, post in ((16, 48), (32, 64), (64, 128), (128, 256), (256, 512)):
        begin, pattern, mask, _ = window(image, cs, address, pre, post)
        if needle(pattern, mask)[1] - needle(pattern, mask)[0] < 8:
            continue
        if find(image, pattern, mask) == [begin]:
            return dict(site=address - begin, pattern=pattern, mask=mask)
    raise ValueError(f'no unique window for {address:#x}')


def references(image, target):
    """Code positions whose 32-bit value is a RIP-relative displacement reaching target."""
    found, chunk = [], 1 << 22
    for begin, end in image.code:
        for lo in range(begin, end - 4, chunk):
            n = min(chunk, end - 4 - lo)
            # every byte position read as a little-endian int32, without copying
            values = np.ndarray((n,), dtype='<i4', buffer=image.memory, offset=lo, strides=(1,)).astype(np.int64)
            positions = np.arange(lo, lo + n, dtype=np.int64)
            for trailing in (0, 1, 2, 4):
                found += [int(lo + p) for p in np.nonzero(positions + 4 + trailing + values == target)[0][:400]]
    return sorted(set(found))


def in_code(image, address):
    return any(begin <= address < end for begin, end in image.code)


def pointer_slots(image, target):
    """Data qwords holding target after relocation (GOT entries and the like)."""
    key, found, at = struct.pack('<Q', target), [], image.memory.find(struct.pack('<Q', target))
    while at >= 0 and len(found) < 8:
        if at % 8 == 0 and not in_code(image, at):
            found.append(at)
        at = image.memory.find(key, at + 1)
    return found


def referencing_anchor(image, cs, position, reached):
    """Window around the RIP-relative instruction whose disp32 sits at position and reaches reached."""
    try:
        start = function_decode(image, cs, position, inside=True)
    except ValueError:
        return None
    for insn in cs.disasm(image.memory[start:position + 16], start):
        if insn.address < position < insn.address + insn.size and insn.disp_size == 4 and \
                insn.address + insn.disp_offset == position and \
                any(op.type == x86_const.X86_OP_MEM and op.mem.base == x86_const.X86_REG_RIP for op in insn.operands) and \
                insn.address + insn.size + insn.disp == reached:
            try:
                a = code_anchor(image, cs, insn.address)
            except ValueError:
                return None
            a.update(displacement=insn.disp_offset, end=insn.size)
            return a
        if insn.address > position:
            return None
    return None


def data_anchors(image, cs, address, count=3):
    """Windows around code that uses the binding: directly (RIP-relative), or through a pointer
    slot that the loader fills with its address (indirect)."""
    for delta in (0, 8, 0x10, 0x18, 0x20):
        anchors = []
        reached = [(address - delta, 0)] + [(slot, 1) for slot in pointer_slots(image, address - delta)]
        for target, indirect in reached:
            for position in references(image, target):
                if len(anchors) >= count:
                    return anchors
                a = referencing_anchor(image, cs, position, target)
                if a and all(b['pattern'] != a['pattern'] for b in anchors):
                    a.update(delta=delta, indirect=indirect)
                    anchors.append(a)
        if anchors:
            return anchors
    raise ValueError(f'no code reference to {address:#x}')


def anchors_for(image, cs, address, kind):
    if kind.startswith('vtable'):
        return []
    return [code_anchor(image, cs, address)] if kind == 'code' else data_anchors(image, cs, address)


def resolve(image, anchors, kind):
    """The runtime rule: each window must occur once; data anchors must agree."""
    results = set()
    for a in anchors:
        hits = find(image, a['pattern'], a['mask'])
        if len(hits) != 1:
            continue
        at = hits[0] + a['site']
        if kind == 'code':
            results.add(at)
        else:
            disp, = struct.unpack_from('<i', image.memory, at + a['displacement'])
            reached = at + a['end'] + disp
            if a['indirect']:
                reached, = struct.unpack_from('<Q', image.memory, reached)
            results.add(reached + a['delta'])
    return results.pop() if len(results) == 1 else None


def resolve_vtable(image, kind, resolved):
    """The table whose listed slots hold the listed functions (relocated pointers)."""
    slots = [(name, int(index)) for name, index in (part.split('@') for part in kind.split(':')[1].split(','))]
    if any(resolved.get(name) is None for name, _ in slots):
        return None
    name, index = slots[0]
    hits, at = [], image.memory.find(struct.pack('<Q', resolved[name]))
    while at >= 0 and len(hits) < 2:
        table = at - index * 8
        if at % 8 == 0 and all(struct.unpack_from('<Q', image.memory, table + i * 8)[0] == resolved[n] for n, i in slots):
            hits.append(table)
        at = image.memory.find(struct.pack('<Q', resolved[name]), at + 1)
    return hits[0] if len(hits) == 1 else None


def emit(points, path):
    def literal(data):
        return '"' + ''.join(f'\\x{b:02x}' for b in data) + '"'
    lines = [
        '// SPDX-License-Identifier: GPL-2.0-only',
        '// Generated by tools/retail_anchors.py from the EU v01.21 eboot (CUSA14876, 39d716b7...).',
        '// Each window is EU v01.21 code around a binding; ? marks bytes that change when code',
        '// moves (call targets, RIP-relative displacements, jumps leaving the window). Do not edit.',
        '#pragma once',
        '#include <cstdint>',
        'namespace ctrport::retail_map {',
        'struct Window {',
        '    std::uint32_t site;          // code: the binding\'s offset in the window; data: the referencing instruction\'s',
        '    std::uint16_t displacement;  // data: disp32 offset in that instruction',
        '    std::uint16_t end;           // data: that instruction\'s length',
        '    std::uint32_t delta;         // data: binding minus the referenced address',
        '    std::uint32_t indirect;      // data: the referenced address is a pointer slot holding the binding',
        '    std::uint32_t size;',
        '    const char* bytes;',
        '    const char* mask;            // x compared, ? ignored',
        '};',
        '// kind: 0 code, 1 data reached through code, 2 vtable found after relocation by the points its',
        '// slots hold ("name@slot,..." in slots).',
        'struct Point { const char* name; std::uint32_t reference; unsigned kind; std::uint32_t first, count; const char* slots; };',
        'inline constexpr Window windows[] = {',
    ]
    table, index = [], 0
    for name, address, kind, anchors in points:
        for a in anchors:
            lines.append(f'    {{{a["site"]:#x},{a.get("displacement", 0)},{a.get("end", 0)},{a.get("delta", 0):#x},{a.get("indirect", 0)},{len(a["pattern"])},')
            lines.append(f'     {literal(a["pattern"])},')
            lines.append(f'     "{a["mask"].decode()}"}},')
        code = 0 if kind == 'code' else 1 if kind == 'data' else 2
        slots = f'"{kind.split(":")[1]}"' if code == 2 else 'nullptr'
        table.append(f'    {{"{name}",{address:#x},{code},{index},{len(anchors)},{slots}}},')
        index += len(anchors)
    lines += ['};', 'inline constexpr Point points[] = {'] + table + ['};', '} // namespace ctrport::retail_map', '']
    Path(path).write_text('\n'.join(lines), encoding='utf-8', newline='\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('reference')
    parser.add_argument('--out')
    parser.add_argument('--check')
    args = parser.parse_args()
    image = Image(args.reference)
    if image.sha256 != REFERENCE_SHA:
        sys.exit(f'{args.reference} is not the EU v01.21 eboot ({image.sha256})')
    cs = disassembler()
    points = []
    for name, address, kind in POINTS:
        anchors = anchors_for(image, cs, address, kind)
        if kind.startswith('vtable'):
            assert resolve_vtable(image, kind, {n: a for n, a, *_ in POINTS}) == address, name
        else:
            assert resolve(image, anchors, kind) == address, name
        points.append((name, address, kind, anchors))
        print(f'{name:44} {address:#9x} {kind} windows={len(anchors)} bytes={sum(len(a["pattern"]) for a in anchors)}')
    if args.out:
        emit(points, args.out)
        print('written', args.out)
    if args.check:
        other = Image(args.check)
        print('check', args.check, other.sha256[:12])
        found = 0
        resolved = {}
        for name, address, kind, anchors in points:
            if kind.startswith('vtable'):
                at = resolve_vtable(other, kind, resolved)
                found += at is not None
                print(f'  {name:44} {address:#9x} -> {"unresolved" if at is None else hex(at)}')
                continue
            at = resolve(other, anchors, kind)
            resolved[name] = at
            found += at is not None
            print(f'  {name:44} {address:#9x} -> {"unresolved" if at is None else hex(at)}')
        print(f'  resolved {found}/{len(points)}')
        if other.sha256 == EU100_SHA:
            right = wrong = missing = 0
            for address, expected in KNOWN_EU100.items():
                kind = KNOWN_KINDS.get(address, 'code')
                try:
                    at = resolve(other, anchors_for(image, cs, address, kind), kind)
                except ValueError as error:
                    at = None
                    print('   ', error)
                verdict = 'missing' if at is None else 'right' if at == expected else 'WRONG'
                right += verdict == 'right'; wrong += verdict == 'WRONG'; missing += verdict == 'missing'
                print(f'  known {address:#9x} -> {at if at is None else hex(at)} expected {expected:#x} {verdict}')
            print(f'  known offsets: {right} right, {wrong} wrong, {missing} unresolved')


if __name__ == '__main__':
    main()
