# SPDX-License-Identifier: GPL-2.0-only
"""Isolated working directories for concurrent retail runtime instances."""
import contextlib
import json
import os
import re
import subprocess
from pathlib import Path


def prepare(root, name):
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', name):
        raise ValueError('instance name must contain 1-32 letters, digits, _ or -')
    root = root.resolve()
    work = root / 'runtime/user/instances' / name
    work.mkdir(parents=True, exist_ok=True)
    if not work.resolve().is_relative_to((root / 'runtime/user').resolve()):
        raise ValueError('instance directory escapes runtime/user')
    # All existing relative runtime/user paths now land in this private instance tree.
    (work / 'runtime/user/logs').mkdir(parents=True, exist_ok=True)
    link = work / 'build'
    target = root / 'build'
    if not link.exists():
        # Junctions need no developer-mode symlink permission. Both paths are validated.
        subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(target)],
                       check=True, capture_output=True)
    if link.resolve() != target.resolve():
        raise ValueError('instance build junction points to an unexpected directory')
    return work


@contextlib.contextmanager
def lock(work):
    path = work / 'instance.lock'
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL)
    try:
        with os.fdopen(fd, 'w') as stream:
            json.dump({'pid': os.getpid()}, stream)
        yield
    finally:
        path.unlink()
