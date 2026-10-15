# SPDX-License-Identifier: GPL-2.0-or-later
"""Project-server Pit Stop state: Wumpa currency balances and inventory per account.

Observed retail v01.21 requests ((development notes)):
- service 80 task 165, typed [STRING context, UINT32 1, UINT32 856]: inventory page; the
  client sizes the request from its local item catalogue (856 items);
- service 80 task 245, struct buffer {1: context, 2: max results}: currency balances; the
  client asks for as many balances as its local catalogue has currencies.
The catalogue (items, prices, currencies) is game data; the server only keeps what an
account owns. Only the "octane_psn" context is accepted. Inventory entries and balances
are stored, but only empty inventory pages are answered until the retail item record
layout is validated.
"""
import sqlite3

CONTEXT = 'octane_psn'


def varint(value):
    out = bytearray()
    while value >= 128:
        out.append((value & 127) | 128)
        value >>= 7
    out.append(value)
    return bytes(out)


def zigzag(value):
    return (value << 1) ^ (value >> 63)


class Store:
    def __init__(self, path):
        self.database = sqlite3.connect(path)
        with self.database:
            self.database.execute('CREATE TABLE IF NOT EXISTS balances (account TEXT NOT NULL, '
                                  'currency INTEGER NOT NULL, amount INTEGER NOT NULL, '
                                  'PRIMARY KEY(account, currency))')
            self.database.execute('CREATE TABLE IF NOT EXISTS inventory (account TEXT NOT NULL, '
                                  'item INTEGER NOT NULL, quantity INTEGER NOT NULL, '
                                  'PRIMARY KEY(account, item))')

    def inventory_page(self, params, account):
        """Typed reply results for task 165; raises ValueError on an unexpected request."""
        if [p[0] for p in params] != [16, 8, 8] or params[0][1] != CONTEXT:
            raise ValueError('unsupported inventory request')
        if params[1][1] != 1 or not 0 < params[2][1] <= 4096:
            raise ValueError('unsupported inventory page')
        count = self.database.execute('SELECT COUNT(*) FROM inventory WHERE account=?',
                                      (str(account),)).fetchone()[0]
        if count:
            raise NotImplementedError('nonempty inventory record layout not validated')
        return []

    def balances_message(self, params, account, fields):
        """Struct reply message for task 245: repeated field 1 {1: currency, 2: zigzag amount}."""
        if len(params) != 1 or params[0][0] != 23:
            raise ValueError('unsupported balances request')
        request = {tag: value for tag, wire, value in fields(bytes.fromhex(params[0][1]))}
        if request.get(1) != CONTEXT.encode() or not 0 < request.get(2, 0) <= 16:
            raise ValueError('unsupported balances request fields')
        rows = self.database.execute('SELECT currency, amount FROM balances WHERE account=? '
                                     'ORDER BY currency LIMIT ?', (str(account), request[2])).fetchall()
        message = b''
        for currency, amount in rows:
            entry = varint(1 << 3) + varint(currency) + varint(2 << 3) + varint(zigzag(amount))
            message += varint(1 << 3 | 2) + varint(len(entry)) + entry
        return message

    def close(self):
        self.database.close()
