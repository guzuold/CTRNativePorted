# SPDX-License-Identifier: GPL-2.0-or-later
"""Project friend graph, independent of any console account or social service.

The retail schema for an empty graph is supported. Nonempty pages fail explicitly
until the original Toolkit's per-friend parser has been validated.
"""
import sqlite3


class Store:
    def __init__(self, path):
        self.database = sqlite3.connect(path)
        self.database.execute('CREATE TABLE IF NOT EXISTS friends ('
                              'owner TEXT NOT NULL, friend TEXT NOT NULL, '
                              'PRIMARY KEY(owner, friend))')
        self.database.commit()

    def page(self, account, offset, limit):
        if offset < 0 or not 1 <= limit <= 500:
            raise ValueError('invalid friend pagination')
        total = self.database.execute('SELECT COUNT(*) FROM friends WHERE owner=?',
                                      (str(account),)).fetchone()[0]
        rows = self.database.execute('SELECT friend FROM friends WHERE owner=? '
                                     'ORDER BY friend LIMIT ? OFFSET ?',
                                     (str(account), limit, offset)).fetchall()
        if rows:
            raise NotImplementedError('nonempty friend schema not retail validated')
        return {'totalResults': total, 'size': 0, 'friendList': []}

    def close(self):
        self.database.close()
