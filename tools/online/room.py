# SPDX-License-Identifier: GPL-2.0-or-later
"""Shared room selector encoding for the launcher, runtime and project server."""
import hashlib
import re


def token(password):
    encoded = password.encode('utf8')
    if not 1 <= len(encoded) <= 256:
        raise ValueError('room password must contain 1..256 UTF-8 bytes')
    return hashlib.sha256(b'ctr-room-v1\0' + encoded).hexdigest()


def valid_token(value):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value) is not None
