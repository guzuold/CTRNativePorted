# SPDX-License-Identifier: GPL-2.0-only
"""Run CTRPort with byte-preserving console and persistent stdout/stderr copies.
Usage: python -B ctrport/run_logged.py [CTRPort arguments...]
"""
import os
from pathlib import Path
import subprocess
import sys
import threading

ROOT = Path(__file__).resolve().parents[1]
def main():
    logs = ROOT / 'runtime/user/logs'
    logs.mkdir(parents=True, exist_ok=True)
    child = subprocess.Popen([str(ROOT / 'build/CTRPort-infra/ctrport.exe'), *sys.argv[1:]], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    errors = []
    def copy(source, console, path):
        try:
            with path.open('wb') as log:
                while True:
                    data = os.read(source.fileno(), 65536)
                    if not data: break
                    log.write(data); log.flush()
                    console.buffer.write(data); console.buffer.flush()
        except BaseException as error:
            errors.append(error)
            child.kill()
    threads = [threading.Thread(target=copy, args=(child.stdout, sys.stdout, logs/'startup.stdout.log')), threading.Thread(target=copy, args=(child.stderr, sys.stderr, logs/'startup.stderr.log'))]
    for thread in threads: thread.start()
    result = child.wait()
    for thread in threads: thread.join()
    if errors: raise RuntimeError('CTRPort log/console copy failed') from errors[0]
    return result
if __name__ == '__main__':
    raise SystemExit(main())
