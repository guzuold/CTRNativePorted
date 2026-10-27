"""Finds and optionally removes namespace-scope C++ functions that nothing refers to.

A function is dead when its name appears nowhere in the repository's text files except inside
its own definition (declarations, calls, function pointers, asm, scripts and comments all
count as uses, so the rule only errs on the side of keeping). Overloads share a name and live or
die together. Removal repeats until nothing more is dead, since removing one function can leave
the helpers only it used without users.

  python tools/dead_functions.py <file or folder>... [--remove]
"""
import os, re, subprocess, sys

IDENT = re.compile(r'[A-Za-z_]\w*')
ROOTS = {'main', 'wmain', 'WinMain', 'wWinMain', 'DllMain'}
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'catch', 'decltype', 'alignof', 'static_assert',
            'operator', 'defined', 'noexcept', 'throw', 'new', 'delete'}


def mask(text):
    """Copy of text with comments, string and char literals blanked (same length, newlines kept)."""
    out, i, n = list(text), 0, len(text)
    def blank(a, b):
        for k in range(a, b):
            if out[k] != '\n':
                out[k] = ' '
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i); j = n if j < 0 else j
            blank(i, j); i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2); j = n if j < 0 else j + 2
            blank(i, j); i = j
        elif c == 'R' and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            k = text.find('(', i + 2)
            delim = text[i + 2:k]
            j = text.find(')' + delim + '"', k)
            j = n if j < 0 else j + len(delim) + 2
            blank(i, j); i = j
        elif c in '"\'':
            if c == "'" and i > 0 and i + 1 < n and text[i + 1].isalnum():
                k = i - 1  # a digit separator (1'000, 0xFF'FF) sits inside a token that starts with a digit
                while k >= 0 and (text[k].isalnum() or text[k] in "_'"):
                    k -= 1
                if k + 1 < i and text[k + 1].isdigit():
                    i += 1; continue
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            blank(i + 1, min(j, n)); i = j + 1
        elif c == '#' and (i == 0 or text[i - 1] == '\n' or text[:i].rsplit('\n', 1)[-1].strip() == ''):
            j = i
            while True:  # preprocessor line, with continuations
                e = text.find('\n', j); e = n if e < 0 else e
                if e > 0 and text[e - 1] == '\\':
                    j = e + 1; continue
                break
            blank(i, e); i = e
        else:
            i += 1
    return ''.join(out)


def functions(text):
    """(name, start, end) of function definitions at namespace scope (also inside extern "C")."""
    m = mask(text)
    found, depth_kinds, stmt_start, i, n = [], [], 0, 0, len(m)
    while i < n:
        c = m[i]
        if c in ';':
            if all(k == 'ns' for k in depth_kinds):
                stmt_start = i + 1
        elif c == '{':
            head = m[stmt_start:i]
            at_ns = all(k == 'ns' for k in depth_kinds)
            if at_ns and (re.search(r'\bnamespace\b', head) or re.fullmatch(r'\s*extern\s*"\s*"\s*', head)):
                depth_kinds.append('ns'); stmt_start = i + 1
            elif at_ns:
                d, j = 1, i + 1  # the matching closing brace
                while j < n and d:
                    if m[j] == '{': d += 1
                    elif m[j] == '}': d -= 1
                    j += 1
                name = None
                h = head.strip()
                if '(' in h:
                    before = h[:h.index('(')]
                    ids = IDENT.findall(before)
                    if (ids and ids[-1] not in KEYWORDS and '::' not in before and '=' not in before
                            and not re.search(r'\b(class|struct|union|enum|operator)\b', before)):
                        name = ids[-1]
                if name:
                    # start: include template<> and the comment lines right above
                    s = stmt_start
                    while s < i and text[s] in ' \t\r\n':
                        s += 1
                    line_start = text.rfind('\n', 0, s) + 1
                    k = line_start if text[line_start:s].strip() == '' else s  # other code may share the line
                    while k == line_start or text[k:line_start].lstrip().startswith('//'):
                        prev = text.rfind('\n', 0, k - 1) + 1 if k > 0 else 0
                        line = text[prev:k].strip()
                        if k > 0 and k <= line_start and line.startswith('//'):
                            k = prev
                        else:
                            break
                    end = j
                    if end < n and text[end] == ';':
                        end += 1
                    nl = text.find('\n', end)
                    end = nl + 1 if nl >= 0 and text[end:nl].strip() == '' else end
                    found.append((name, k, end))
                stmt_start = j
                i = j
                continue
            else:
                depth_kinds.append('other')
        elif c == '}':
            if depth_kinds:
                depth_kinds.pop()
            if all(k == 'ns' for k in depth_kinds):
                stmt_start = i + 1
        i += 1
    if depth_kinds:
        raise ValueError('unbalanced braces')  # the masking missed something: never edit such a file
    return found


def repository_files():
    files = subprocess.run(['git', 'ls-files'], capture_output=True, text=True, check=True).stdout.split()
    return [f for f in files if os.path.splitext(f)[1].lower() in
            {'.cpp', '.hpp', '.h', '.c', '.inc', '.asm', '.py', '.ps1', '.md', '.json', '.txt', '.cmake', '.rc', '.def'}
            or os.path.basename(f) == 'CMakeLists.txt']


def main():
    remove = '--remove' in sys.argv
    targets = [a for a in sys.argv[1:] if a != '--remove']
    files = repository_files()
    texts = {f: open(f, encoding='utf-8', errors='surrogateescape').read() for f in files}
    # third-party code stays as its authors wrote it
    vendor = ('ctrport/d3d/fsr/', 'ctrport/gpu/shim/', 'ctrport/dht/', 'ctrport/video/adapted/', 'ctrport/gpu/upscaling/', 'ctrport/hle/shadps4/')
    selected = [f for f in files if f.endswith(('.hpp', '.h', '.cpp')) and not f.startswith(vendor)
                and any(f == t or f.startswith(t.rstrip('/') + '/') for t in targets)]
    total = 0
    while True:
        counts = {}
        for t in texts.values():
            for w in IDENT.findall(t):
                counts[w] = counts.get(w, 0) + 1
        dead = []
        for f in selected:
            try:
                defs = functions(texts[f])
            except ValueError as error:
                print(f'skipped: {f}: {error}')
                continue
            by_name = {}
            for name, a, b in defs:
                by_name.setdefault(name, []).append((a, b))
            for name, spans in by_name.items():
                # program and library entry points, and functions exported from a DLL, are called
                # from outside the source
                if name in ROOTS or any('dllexport' in texts[f][x:x + 400].split('{', 1)[0] for x, _ in spans):
                    continue
                inside = sum(len(re.findall(r'\b' + re.escape(name) + r'\b', texts[f][a:b])) for a, b in spans)
                if counts.get(name, 0) - inside == 0:
                    dead.append((f, name, spans))
        if not dead:
            break
        for f, name, spans in dead:
            print(f'dead: {f}: {name}')
        total += len(dead)
        if not remove:
            break
        for f in {d[0] for d in dead}:
            spans = sorted([s for d in dead if d[0] == f for s in d[2]], reverse=True)
            t = texts[f]
            for a, b in spans:
                t = t[:a] + t[b:]
            texts[f] = t
            open(f, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(t)
    print('dead functions', 'removed' if remove else 'found', total)


if __name__ == '__main__':
    main()
