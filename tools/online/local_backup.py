# SPDX-License-Identifier: GPL-2.0-only
"""Immutable pre/post-run copies for the command-line game runner."""
import datetime
import shutil
import uuid
from pathlib import Path


def backup_savedata(user_root: Path, phase: str):
    source = user_root / 'savedata'
    if not source.exists():
        return None
    name = f'{phase}_{datetime.datetime.now():%Y%m%d_%H%M%S}_{uuid.uuid4().hex}'
    target = user_root / 'savedata_backups' / name
    partial = target.with_name(target.name + '.partial')
    shutil.copytree(source, partial)
    partial.rename(target)
    return target
