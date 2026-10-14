# SPDX-License-Identifier: GPL-2.0-or-later
"""Durable project-server statistics for the observed retail write task.

Only replacement writes (mode 0) with the observed four typed fields are supported.
Unsupported schemas and writes for another account fail before any database mutation.
"""
import sqlite3


class Store:
    def __init__(self, path):
        self.database = sqlite3.connect(path)
        self.database.execute('CREATE TABLE IF NOT EXISTS ratings (board INTEGER NOT NULL, '
                              'account TEXT NOT NULL, rating INTEGER NOT NULL, '
                              'PRIMARY KEY(board, account))')
        self.database.commit()

    def write(self, params, account):
        if not params or len(params) % 4:
            raise ValueError('unsupported statistics record length')
        records = []
        for i in range(0, len(params), 4):
            values = params[i:i + 4]
            if [p[0] for p in values] != [8, 10, 3, 9]:
                raise ValueError('unsupported statistics field types')
            board, entity, mode, rating = [p[1] for p in values]
            if entity != account or mode != 0 or not 0 <= board <= 0xffffffff:
                raise ValueError('statistics account or write mode rejected')
            records.append((board, str(entity), rating))
        with self.database:
            self.database.executemany('INSERT INTO ratings VALUES (?, ?, ?) ON CONFLICT(board,account) '
                                      'DO UPDATE SET rating=excluded.rating', records)

    def close(self):
        self.database.close()
