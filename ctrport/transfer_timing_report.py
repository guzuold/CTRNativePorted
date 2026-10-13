# SPDX-License-Identifier: GPL-2.0-only
"""Metadata aggregation for the opt-in transfer diagnostic; no GPU timing claim."""
import hashlib
import re

_PATTERN = re.compile(r'CTR_TRANSFER_PHASE operation=(\w+) bytes=(\d+) phase=(\w+) ms=(\d+(?:\.\d+)?) status=(-?\d+)')

def aggregate_transfer_phases(lines):
    aggregates = {}
    memory_types = {}
    devices = []
    malformed = []
    for line in lines:
        if "CTR_TRANSFER_DEVICE " in line:
            device = re.search(r"CTR_TRANSFER_DEVICE name=(.*?) vendor=(\d+) device=(\d+) type=(\d+) queue=(\d+)", line)
            if device is None:
                malformed.append(hashlib.sha256(line.encode()).hexdigest())
            else:
                name, *numbers = device.groups()
                identity = dict(zip(("vendor", "device", "type", "queue"), map(int, numbers)))
                identity["name"] = name
                if identity not in devices:
                    devices.append(identity)
        if 'CTR_TRANSFER_MEMORY ' in line:
            memory = re.search(r'CTR_TRANSFER_MEMORY operation=(\w+) buffer=(\d+) type=(\d+) flags=(\d+) compatible_bits=(\d+) bytes=(\d+)', line)
            if memory is None:
                malformed.append(hashlib.sha256(line.encode()).hexdigest())
            else:
                operation, buffer, index, flags, compatible, size = memory.groups()
                key = '/'.join((operation, buffer, index, flags, compatible))
                entry = memory_types.setdefault(key, {'operation': operation,
                    'buffer': int(buffer), 'type': int(index), 'flags': int(flags),
                    'compatible_bits': int(compatible), 'count': 0,
                    'min_bytes': int(size), 'max_bytes': int(size)})
                entry['count'] += 1
                entry['min_bytes'] = min(entry['min_bytes'], int(size))
                entry['max_bytes'] = max(entry['max_bytes'], int(size))
        if 'CTR_TRANSFER_PHASE ' not in line:
            continue
        match = _PATTERN.search(line)
        if match is None:
            malformed.append(hashlib.sha256(line.encode()).hexdigest())
            continue
        operation, size, phase, duration, status = match.groups()
        size_key = 'operand_sum' if operation in ('graphics_pipeline', 'graphics_draw') else 'bytes_sum'
        key = operation + '/' + phase
        entry = aggregates.setdefault(key, {'operation': operation, 'phase': phase,
            'count': 0, 'ms': 0.0, size_key: 0, 'max_ms': 0.0, 'statuses': {}})
        entry['count'] += 1
        entry['ms'] += float(duration)
        entry[size_key] += int(size)
        if size_key == 'operand_sum':
            entry['operand_unit'] = 'stage_count' if operation == 'graphics_pipeline' else 'vertex_or_index_count'
        entry['max_ms'] = max(entry['max_ms'], float(duration))
        entry['statuses'][status] = entry['statuses'].get(status, 0) + 1
    return {'aggregates': aggregates, 'memory_types': list(memory_types.values()), 'selected_devices': devices, 'malformed_line_hashes': malformed,
        'limits': 'Host wall time; queued work may contribute to fence wait. Timeout can interrupt the final call. Status -1 is pending phase result, not failure. Do not add parent pipeline timing to nested phases.'}
