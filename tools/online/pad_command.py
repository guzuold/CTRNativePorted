# SPDX-License-Identifier: GPL-2.0-only
"""Send a bounded diagnostic button pulse to one --instance retail process.

Requires CTR_PAD_TEST_INPUT=1 at launch and a genuinely connected XInput controller.
No foreground-window change or global keyboard injection is performed.
"""
import argparse
import ctypes
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('instance')
parser.add_argument('button', choices=['a', 'b', 'x', 'y', 'up', 'down', 'left', 'right', 'start', 'release'])
parser.add_argument('--ms', type=int, default=180)
args = parser.parse_args()
if not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', args.instance) or not 0 <= args.ms <= 2000:
    parser.error('invalid instance or duration (0..2000 ms)')
root = Path(__file__).resolve().parents[2]
work = root / 'runtime/user/instances' / args.instance
if not work.is_dir() or not work.resolve().is_relative_to((root / 'runtime/user').resolve()):
    parser.error('instance directory missing or outside runtime/user')
buttons = dict(a=0x1000, b=0x2000, x=0x4000, y=0x8000, up=1, down=2, left=4, right=8, start=0x10, release=0)
ctypes.windll.kernel32.GetTickCount64.restype = ctypes.c_ulonglong
deadline = ctypes.windll.kernel32.GetTickCount64() + args.ms
path = work / 'runtime/user/input/pad.txt'
path.parent.mkdir(parents=True, exist_ok=True)
temporary = path.with_suffix('.tmp')
temporary.write_text('%d %d\n' % (buttons[args.button], deadline))
temporary.replace(path)
print(args.instance, args.button, args.ms, 'ms')
