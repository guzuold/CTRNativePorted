"""Formats the project's own C++ with clang-format and .clang-format (pip install clang-format).

Third-party folders, generated files and translation tables are left alone. Formatting changes
whitespace only; `git diff -w` after a run shows nothing.

  python tools/format_cpp.py [paths...]     (default: ctrport and tools)
"""
import os, shutil, subprocess, sys

VENDOR = ('ctrport/d3d/fsr/', 'ctrport/gpu/shim/', 'ctrport/dht/', 'ctrport/video/adapted/', 'ctrport/video/shim/',
          'ctrport/gpu/upscaling/', 'ctrport/hle/shadps4/')
KEEP = {'ctrport/retail_anchors.hpp', 'ctrport/launcher_text.hpp', 'ctrport/settings/control_schema.hpp',
        'ctrport/settings/menu_catalog.hpp'}


def clang_format():
    found = shutil.which('clang-format')
    if found:
        return found
    import clang_format as package
    return os.path.join(os.path.dirname(package.__file__), 'data', 'bin', 'clang-format.exe')


def main():
    roots = sys.argv[1:] or ['ctrport', 'tools']
    files = subprocess.run(['git', 'ls-files', *roots], capture_output=True, text=True, check=True).stdout.split()
    files = [f for f in files if f.endswith(('.cpp', '.hpp', '.h', '.c')) and not f.startswith(VENDOR) and f not in KEEP]
    exe = clang_format()
    for start in range(0, len(files), 50):
        subprocess.run([exe, '-i', '--style=file', *files[start:start + 50]], check=True)
    print(len(files), 'files formatted')


if __name__ == '__main__':
    main()
