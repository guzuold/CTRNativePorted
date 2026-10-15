# SPDX-License-Identifier: GPL-2.0-or-later
"""Demonware lobby tasks: request parsing and replies of the project server (R8 phase 3).

Client side (retail v01.21, reports/online/r8_phase3_auth3.md): a task is a lobby record of
payload type 0x86 holding the service ID (raw byte), the task ID (typed u8) and the task
parameters as a typed byte buffer. The reply is a record of payload type 1 holding a typed
byte buffer: transaction ID u64 | error u32 | task ID u8 | results (u32 count, then
objects). Typed values are a type byte followed by the little-endian value.
"""
import itertools, struct, time
import demonware_rest

# Typed byte buffer data types (as numbered in h1-mod byte_buffer, public source).
BOOL, INT8, UINT8, WCHAR16, INT16, UINT16, INT32, UINT32, INT64, UINT64 = range(1, 11)
FLOAT32, FLOAT64 = 13, 14
STRING, USTRING, MBSTRING, BLOB = 16, 17, 18, 19
# Struct buffer (protobuf message) parameter, observed in retail requests of services 80, 125
# and 194: type byte 23, typed u32 size, message bytes.
STRUCT = 23

# Names: "anticheat" is the project's; reportExtendedAuthInfo appears in the retail eboot.
SERVICE_NAMES = {38: 'anticheat'}
TASK_NAMES = {(38, 6): 'reportExtendedAuthInfo'}
LSG_TASK_REPLY = 1
_transactions = itertools.count(1)
statistics_store = None
matchmaking_store = None
marketplace_store = None
achievement_store = None


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def raw_u8(self):
        if self.offset >= len(self.data):
            raise ValueError('truncated typed byte')
        value = self.data[self.offset]
        self.offset += 1
        return value

    def value(self):
        kind = self.raw_u8()
        sizes = {BOOL: 1, INT8: 1, UINT8: 1, WCHAR16: 2, INT16: 2, UINT16: 2, INT32: 4,
                 UINT32: 4, INT64: 8, UINT64: 8, FLOAT32: 4, FLOAT64: 8}
        if kind in sizes:
            size = sizes[kind]
            raw = self.data[self.offset:self.offset + size]
            if len(raw) != size:
                raise ValueError('truncated typed number')
            self.offset += size
            if kind in (FLOAT32, FLOAT64):
                return kind, struct.unpack('<f' if size == 4 else '<d', raw)[0]
            return kind, int.from_bytes(raw, 'little', signed=kind in (INT8, INT16, INT32, INT64))
        if kind in (STRING, USTRING, MBSTRING):
            end = self.data.index(0, self.offset)
            text = self.data[self.offset:end].decode('utf-8', 'replace')
            self.offset = end + 1
            return kind, text
        if kind == BLOB:
            length_kind, length = self.value()
            if length_kind != UINT32 or length > len(self.data) - self.offset:
                raise ValueError('invalid typed blob length')
            blob = self.data[self.offset:self.offset + length]
            self.offset += length
            return kind, blob.hex()
        if kind == STRUCT:
            length_kind, length = self.value()
            if length_kind != UINT32 or length > len(self.data) - self.offset:
                raise ValueError('invalid struct buffer length')
            message = self.data[self.offset:self.offset + length]
            self.offset += length
            return kind, message.hex()
        raise ValueError('unknown data type %d at %d' % (kind, self.offset - 1))

    def values(self):
        out = []
        while self.offset < len(self.data):
            if self.data[self.offset] == 0:  # trailing zero byte after the last value
                self.offset += 1
                continue
            out.append(self.value())
        return out


class Writer:
    def __init__(self):
        self.data = bytearray()

    def u8(self, value):
        self.data += bytes([UINT8, value])
        return self

    def u32(self, value):
        self.data += bytes([UINT32]) + struct.pack('<I', value)
        return self

    def u64(self, value):
        self.data += bytes([UINT64]) + struct.pack('<Q', value)
        return self

    def blob(self, value):
        self.data += bytes([BLOB])
        self.u32(len(value))
        self.data += value
        return self

    def struct(self, message):
        self.data += bytes([STRUCT])
        self.u32(len(message))
        self.data += message
        return self

    def string(self, value):
        self.data += bytes([STRING]) + value.encode('utf8') + b'\0'
        return self

    def boolean(self, value):
        self.data += bytes([BOOL,1 if value else 0])
        return self


def parse_request(payload):
    reader = Reader(payload)
    service = reader.raw_u8()
    _, task = reader.value()
    if (service, task) in ((194, 1), (193, 8)):
        return service, task, demonware_rest.parse_request(payload[reader.offset:])
    try:
        params = reader.values()
    except (ValueError, IndexError) as error:
        params = [('unparsed', payload[reader.offset:].hex(), str(error))]
    return service, task, params


def reply(task, results=None, error=0):
    """Typed reply buffer for a task; results: Writer with the serialized result objects."""
    writer = Writer().u64(next(_transactions)).u32(error).u8(task)
    if not error:
        if results is None:
            writer.u32(0)
        else:
            writer.data += results.data
    return bytes(writer.data)


def struct_reply(task, message):
    """Reply carrying a struct buffer: header, struct buffer, end marker (as the REST proxy)."""
    writer = Writer().u64(next(_transactions)).u32(0).u8(task).struct(message)
    return bytes(writer.data) + bytes([0])


SERVICE_NAMES[194] = 'REST over LSG'
# Services the project does not serve yet answer with a task error (non-zero code) rather
# than an empty success the client would fail to parse.
UNSERVED_ERROR = 1


def handle(service, task, params, user):
    """Returns the reply payload for a task. Reports are acknowledged with no results."""
    if (service, task) in ((194, 1), (193, 8)):
        writer = Writer().u64(next(_transactions)).u32(0).u8(task)
        writer.data += demonware_rest.response(params)
        return bytes(writer.data)
    if service == 38 and task == 6:
        return reply(task)
    if service == 12 and task == 6 and not params:
        # One result, one total result, UTC seconds. Validate with the retail client.
        return reply(task, Writer().u32(1).u32(1).u32(int(time.time())))
    if service == 4 and task == 1 and statistics_store is not None:
        try:
            statistics_store.write(params, user['user_id'])
        except (ValueError, KeyError):
            return reply(task, error=UNSERVED_ERROR)
        return reply(task)
    if service == 80 and task == 165 and marketplace_store is not None:
        try:
            items = marketplace_store.inventory_page(params, user['user_id'])
        except (ValueError, KeyError, TypeError, NotImplementedError):
            return reply(task, error=UNSERVED_ERROR)
        return reply(task) if not items else reply(task, error=UNSERVED_ERROR)
    if service == 80 and task == 245 and marketplace_store is not None:
        try:
            message = marketplace_store.balances_message(params, user['user_id'], demonware_rest.fields)
        except (ValueError, KeyError, TypeError):
            return reply(task, error=UNSERVED_ERROR)
        return struct_reply(task, message)
    if service == 125 and task == 1 and achievement_store is not None:
        try:
            achievement_store.report_events(params, user['user_id'])
        except (ValueError, KeyError, TypeError, UnicodeError):
            return reply(task, error=UNSERVED_ERROR)
        return reply(task)
    if service == 125 and task == 3 and achievement_store is not None:
        try:
            message = achievement_store.states_message(params, user['user_id'])
        except (ValueError, KeyError, TypeError):
            return reply(task, error=UNSERVED_ERROR)
        return struct_reply(task, message)
    if service == 145 and task == 2 and matchmaking_store is not None:
        try:
            matchmaking_store.advertise(params, user['user_id'], user['room_token'])
        except (ValueError, KeyError, TypeError):
            return reply(task, error=UNSERVED_ERROR)
        return reply(task)
    if service == 145 and task == 3 and not params and matchmaking_store is not None:
        try:
            token = matchmaking_store.player_token(user['user_id'])
        except (ValueError, KeyError):
            return reply(task, error=UNSERVED_ERROR)
        return reply(task, Writer().u32(1).u32(1).u64(token))
    if service == 138 and task == 1 and matchmaking_store is not None:
        try:
            session_id = matchmaking_store.register_session(params, user['user_id'], user['room_token'])
        except (ValueError, KeyError, TypeError):
            return reply(task, error=UNSERVED_ERROR)
        return reply(task, Writer().u32(1).u32(1).blob(session_id))
    if service == 138 and task == 12 and matchmaking_store is not None:
        try:
            matchmaking_store.update_session_players(params, user['user_id'], user['room_token'])
        except (ValueError, KeyError, TypeError):
            return reply(task, error=UNSERVED_ERROR)
        return reply(task)
    if service == 145 and task == 18 and matchmaking_store is not None:
        try:
            search_id = matchmaking_store.start_search(params, user['user_id'], user['room_token'])
        except (ValueError, KeyError, TypeError):
            return reply(task, error=UNSERVED_ERROR)
        return reply(task, Writer().u32(1).u32(1).string('{"mm_id":'+str(search_id)+'}'))
    if service == 145 and task == 15 and matchmaking_store is not None:
        try:
            success=matchmaking_store.sync_document(params,user['user_id'],user['room_token'])
        except (ValueError,KeyError,TypeError):
            return reply(task,error=UNSERVED_ERROR)
        return reply(task,Writer().u32(1).u32(1).boolean(success))
    writer = Writer().u64(next(_transactions)).u32(UNSERVED_ERROR).u8(task)
    return bytes(writer.data)
