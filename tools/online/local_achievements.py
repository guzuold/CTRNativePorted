# SPDX-License-Identifier: GPL-2.0-or-later
"""Project-server Wumpa Challenge (achievement engine, service 125) state per account.

Observed retail v01.21 requests ((development notes)), all struct
buffers in the "octane_psn" context:
- task 1: event report {1: context, 2: {1: name, 2: UTC, 3: {1: key, 2: value}, 4: 0}, 3: {}},
  sent after login with the event "login";
- task 3: achievement states of one kind {1: context, 2: kind 0..7, 3: limit 100, 4: page
  token}; the retail Pit Stop readiness check waits until the states of all eight kinds
  have been received;
- task 9: user state {1: context}.
The project has no challenge schedule yet, so an account owns no achievement states: the
state pages are empty (no repeated field 1, no next page token). Reported events are
recorded for later use and acknowledged.
"""
import json, sqlite3, time

CONTEXT = b'octane_psn'


class Store:
    def __init__(self, path, fields):
        self.fields = fields
        self.database = sqlite3.connect(path)
        with self.database:
            self.database.execute('CREATE TABLE IF NOT EXISTS events (account TEXT NOT NULL, '
                                  'received INTEGER NOT NULL, event TEXT NOT NULL)')

    def _request(self, params):
        if len(params) != 1 or params[0][0] != 23:
            raise ValueError('unsupported achievement request')
        values = self.fields(bytes.fromhex(params[0][1]))
        if [v for t, w, v in values if t == 1] != [CONTEXT]:
            raise ValueError('unsupported achievement context')
        return values

    def report_events(self, params, account):
        values = self._request(params)
        events = []
        for tag, wire, value in values:
            if tag == 2 and wire == 2:
                event = {t: v for t, w, v in self.fields(value) if w == 0 or t == 1}
                if not isinstance(event.get(1), bytes):
                    raise ValueError('achievement event without name')
                events.append({'name': event[1].decode('utf-8'), 'time': event.get(2)})
        if not events:
            raise ValueError('achievement report without events')
        with self.database:
            self.database.executemany('INSERT INTO events VALUES (?, ?, ?)',
                                      [(str(account), int(time.time()), json.dumps(e)) for e in events])
        return events

    def states_message(self, params, account):
        values = self._request(params)
        kinds = [v for t, w, v in values if t == 2 and w == 0]
        limit = [v for t, w, v in values if t == 3 and w == 0]
        token = [v for t, w, v in values if t == 4]
        if len(kinds) != 1 or not 0 <= kinds[0] < 8 or limit != [100] or token not in ([], [b'']):
            raise ValueError('unsupported achievement states request')
        if any(t in (5, 6, 7) for t, w, v in values):
            raise ValueError('achievement state filters not supported')
        return b''  # no states, no next page

    def close(self):
        self.database.close()
