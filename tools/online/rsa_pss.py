# SPDX-License-Identifier: GPL-2.0-or-later
"""RSA for the project server, standard library only.

- generate(bits): RSA key (e = 65537) with Miller-Rabin primes from `secrets`.
- spki_der(n, e): X.509 SubjectPublicKeyInfo DER (the format the game's Demonware client
  imports: 0x126 bytes for RSA-2048).
- sign_pss_sha256(key, message, salt_length=0): RSASSA-PSS with SHA-256 and MGF1-SHA-256
  (RFC 8017 9.1.1). The client verifies auth3 replies with libtomcrypt
  rsa_verify_hash_ex(..., PSS, sha256, saltlen 0) (reports/online/r8_phase3_auth3.md).
"""
import hashlib, secrets


def _probable_prime(n, rounds=40):
    if n < 2:
        return False
    for p in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
        if n % p == 0:
            return n == p
    d, r = n - 1, 0
    while d % 2 == 0:
        d //= 2
        r += 1
    for _ in range(rounds):
        a = secrets.randbelow(n - 3) + 2
        x = pow(a, d, n)
        if x in (1, n - 1):
            continue
        for _ in range(r - 1):
            x = pow(x, 2, n)
            if x == n - 1:
                break
        else:
            return False
    return True


def _prime(bits, e):
    while True:
        candidate = secrets.randbits(bits) | (3 << (bits - 2)) | 1
        if _probable_prime(candidate) and (candidate - 1) % e:
            return candidate


def generate(bits=2048, e=65537):
    while True:
        p, q = _prime(bits // 2, e), _prime(bits // 2, e)
        n = p * q
        if p != q and n.bit_length() == bits:
            return {'n': n, 'e': e, 'd': pow(e, -1, (p - 1) * (q - 1))}


def _der_length(length):
    if length < 0x80:
        return bytes([length])
    raw = length.to_bytes((length.bit_length() + 7) // 8, 'big')
    return bytes([0x80 | len(raw)]) + raw


def _der(tag, content):
    return bytes([tag]) + _der_length(len(content)) + content


def _der_integer(value):
    raw = value.to_bytes(value.bit_length() // 8 + 1, 'big')  # leading 0 keeps it positive
    return _der(0x02, raw)


def spki_der(n, e):
    rsa_public_key = _der(0x30, _der_integer(n) + _der_integer(e))
    algorithm = _der(0x30, bytes.fromhex('06092a864886f70d010101') + b'\x05\x00')
    return _der(0x30, algorithm + _der(0x03, b'\x00' + rsa_public_key))


def _mgf1(seed, length):
    out = b''
    counter = 0
    while len(out) < length:
        out += hashlib.sha256(seed + counter.to_bytes(4, 'big')).digest()
        counter += 1
    return out[:length]


def sign_pss_sha256(key, message, salt_length=0):
    n, d = key['n'], key['d']
    mod_bits = n.bit_length()
    em_bits = mod_bits - 1
    em_len = (em_bits + 7) // 8
    m_hash = hashlib.sha256(message).digest()
    salt = secrets.token_bytes(salt_length)
    h = hashlib.sha256(b'\x00' * 8 + m_hash + salt).digest()
    db = b'\x00' * (em_len - salt_length - 32 - 2) + b'\x01' + salt
    masked = bytearray(a ^ b for a, b in zip(db, _mgf1(h, em_len - 33)))
    masked[0] &= 0xff >> (8 * em_len - em_bits)
    em = bytes(masked) + h + b'\xbc'
    signature = pow(int.from_bytes(em, 'big'), d, n)
    return signature.to_bytes((mod_bits + 7) // 8, 'big')
