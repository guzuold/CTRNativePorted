# SPDX-License-Identifier: GPL-2.0-or-later
"""AES-128 (FIPS-197) in CBC mode without padding, standard library only.

The Demonware lobby records are AES-128-CBC with a random IV per record and lengths that are
multiples of 16 (libtomcrypt cbc_encrypt/cbc_decrypt in the client); see
tools/online/demonware_lobby.py.
"""


def _xtime(a):
    return ((a << 1) ^ 0x1b) & 0xff if a & 0x80 else a << 1


def _mul(a, b):
    result = 0
    while b:
        if b & 1:
            result ^= a
        a = _xtime(a)
        b >>= 1
    return result


def _sbox():
    sbox = [0] * 256
    inverse = [0] * 256
    p = q = 1
    while True:
        p = p ^ (p << 1) ^ (0x1b if p & 0x80 else 0)
        p &= 0xff
        q ^= q << 1
        q ^= q << 2
        q ^= q << 4
        q &= 0xff
        if q & 0x80:
            q ^= 0x09
        x = q ^ (q << 1 | q >> 7) ^ (q << 2 | q >> 6) ^ (q << 3 | q >> 5) ^ (q << 4 | q >> 4)
        x = (x ^ 0x63) & 0xff
        sbox[p] = x
        inverse[x] = p
        if p == 1:
            break
    sbox[0] = 0x63
    inverse[0x63] = 0
    return sbox, inverse


SBOX, INV_SBOX = _sbox()


def expand_key(key):
    assert len(key) == 16
    words = [list(key[i:i + 4]) for i in range(0, 16, 4)]
    rcon = 1
    for i in range(4, 44):
        t = list(words[i - 1])
        if i % 4 == 0:
            t = [SBOX[b] for b in t[1:] + t[:1]]
            t[0] ^= rcon
            rcon = _xtime(rcon)
        words.append([a ^ b for a, b in zip(words[i - 4], t)])
    return [sum(words[r * 4:r * 4 + 4], []) for r in range(11)]


def _add(state, round_key):
    return [a ^ b for a, b in zip(state, round_key)]


def _shift(state, inverse=False):
    out = [0] * 16
    for c in range(4):
        for r in range(4):
            source = (c + r) % 4 if not inverse else (c - r) % 4
            out[c * 4 + r] = state[source * 4 + r]
    return out


def _mix(state, matrix):
    out = []
    for c in range(4):
        column = state[c * 4:c * 4 + 4]
        for r in range(4):
            out.append(_mul(column[0], matrix[r][0]) ^ _mul(column[1], matrix[r][1]) ^
                       _mul(column[2], matrix[r][2]) ^ _mul(column[3], matrix[r][3]))
    return out


MIX = [[2, 3, 1, 1], [1, 2, 3, 1], [1, 1, 2, 3], [3, 1, 1, 2]]
INV_MIX = [[14, 11, 13, 9], [9, 14, 11, 13], [13, 9, 14, 11], [11, 13, 9, 14]]


def encrypt_block(round_keys, block):
    state = _add(list(block), round_keys[0])
    for r in range(1, 11):
        state = _shift([SBOX[b] for b in state])
        if r != 10:
            state = _mix(state, MIX)
        state = _add(state, round_keys[r])
    return bytes(state)


def decrypt_block(round_keys, block):
    state = _add(list(block), round_keys[10])
    for r in range(9, -1, -1):
        state = [INV_SBOX[b] for b in _shift(state, inverse=True)]
        state = _add(state, round_keys[r])
        if r:
            state = _mix(state, INV_MIX)
    return bytes(state)


def cbc_encrypt(key, iv, data):
    assert len(data) % 16 == 0
    keys = expand_key(key)
    out, previous = b'', iv
    for i in range(0, len(data), 16):
        previous = encrypt_block(keys, bytes(a ^ b for a, b in zip(data[i:i + 16], previous)))
        out += previous
    return out


def cbc_decrypt(key, iv, data):
    assert len(data) % 16 == 0
    keys = expand_key(key)
    out, previous = b'', iv
    for i in range(0, len(data), 16):
        block = data[i:i + 16]
        out += bytes(a ^ b for a, b in zip(decrypt_block(keys, block), previous))
        previous = block
    return out
