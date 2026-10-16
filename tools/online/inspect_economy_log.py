# SPDX-License-Identifier: GPL-2.0-only
"""Inspect recorded retail marketplace/achievement tasks without changing state.

Usage: python tools/online/inspect_economy_log.py <online_server.jsonl> [...]
Reports exact event attribute fields; unknown fields remain numeric/raw hex.
No reward amounts or challenge semantics are inferred by this reader.
"""
import argparse
import json
from pathlib import Path
from demonware_rest import fields


def wire_fields(data):
    return [{'tag': tag, 'wire': wire, 'value': value if isinstance(value, int) else value.hex()}
            for tag, wire, value in fields(data)]


def event_summary(data):
    result = {'attributes': [], 'wire_hex': data.hex()}
    for tag, wire, value in fields(data):
        if tag == 1 and wire == 2:
            result['name'] = value.decode('utf-8')
        elif tag == 2 and wire == 0:
            result['time'] = value
        elif tag == 3 and wire == 2:
            attribute = {'fields': wire_fields(value)}
            for key_tag, key_wire, key_value in fields(value):
                if key_tag == 1 and key_wire == 2:
                    attribute['key'] = key_value.decode('utf-8')
                elif key_tag == 2 and key_wire == 0:
                    attribute['value'] = key_value
            result['attributes'].append(attribute)
    return result


def inspect(path):
    with path.open(encoding='utf-8') as stream:
        for line_number, line in enumerate(stream, 1):
            try:
                row = json.loads(line)
                if row.get('event') != 'lobby_task' or row.get('service') not in (80, 125):
                    continue
                result = {key: row.get(key) for key in ('time', 'online_id', 'service', 'task', 'params', 'reply')}
                result.update(source=str(path), line=line_number)
                if row['service'] == 125 and row['task'] == 1:
                    params = row['params']
                    if len(params) != 1 or params[0][0] != 23:
                        raise ValueError('expected one struct request')
                    result['events'] = [event_summary(value) for tag, wire, value in fields(bytes.fromhex(params[0][1]))
                                        if tag == 2 and wire == 2]
                print(json.dumps(result))
            except (ValueError, KeyError, TypeError, IndexError) as error:
                raise ValueError(f'{path}:{line_number}: {error}') from error


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('logs', type=Path, nargs='+')
    for log in parser.parse_args().logs:
        inspect(log)
