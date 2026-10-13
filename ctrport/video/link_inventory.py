"""Inventory external COFF references; this is not a linker success check."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--checked-at', required=True)
    parser.add_argument('--diagnostic', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    if args.diagnostic:
        build = root / 'build/CTRPort-data'
        result = json.loads((build / 'shader_link_result.json').read_text(encoding='utf-8-sig'))
        raw = (build / 'shader_link_diagnostic.log').read_bytes()
        log = raw.decode('utf-16') if raw.startswith(b'\xff\xfe') else raw.decode('utf-8-sig', errors='replace')
        codes = {}
        for code in re.findall(r'(?:error|errore) (LNK[0-9]+)', log):
            codes[code] = codes.get(code, 0) + 1
        symbols = sorted(set(re.findall(r'\((\?[^\s]+)\) non risolto', log)))
        undecorated = sorted(set(re.findall(r'simbolo esterno ([A-Za-z_][A-Za-z_0-9]*) non risolto', log)))
        report = dict(checked_at=args.checked_at, result=result, diagnostic_codes=codes, unresolved_symbols=symbols + undecorated, log_sha256=hashlib.sha256(raw).hexdigest(), build_script_sha256=hashlib.sha256((root / 'ctrport/video/build_shader.ps1').read_bytes()).hexdigest(), linked=result['exit_code'] == 0, retail_shaders_tested=0, R3=False, scope=f"{result['objects']} hash-checked objects; DLL NOENTRY OPT:NOREF with kernel32/user32/advapi32/libucrt/libvcruntime and default object CRT directives. No shader entrypoint; unresolved names retained. Not a runtime readiness test.")
        (root / 'reports/reconstruction/data/shader_link_diagnostic.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report))
        return
    tool = root / 'tools/toolchains/llvm-19.1.1/bin/llvm-nm.exe'
    evidence = root / 'reports/reconstruction/data/shader_full_compile_diagnostic.json'
    report = json.loads(evidence.read_text())
    definitions, references = set(), {}
    for item in report['files']:
        if item['exit_code']:
            raise RuntimeError('Complete compile required')
        obj = root / 'build/CTRPort-data' / (item['source'].replace('/', '_').replace('.cpp', '.obj'))
        if hashlib.sha256(obj.read_bytes()).hexdigest() != item['object_sha256']:
            raise RuntimeError('Stale object: ' + str(obj))
        for mode in ('--defined-only', '--undefined-only'):
            result = subprocess.run([str(tool), '--extern-only', mode, '--format=just-symbols', str(obj)], check=True, capture_output=True, text=True)
            for symbol in result.stdout.splitlines():
                if mode == '--defined-only':
                    definitions.add(symbol)
                else:
                    references.setdefault(symbol, set()).add(item['source'])
    unresolved = sorted(set(references) - definitions)
    rows = []
    for symbol in unresolved:
        demangled = subprocess.run([str(root / 'tools/toolchains/llvm-19.1.1/bin/llvm-undname.exe'), symbol], check=False, capture_output=True, text=True).stdout.strip().splitlines()
        rows.append(dict(symbol=symbol, demangled=demangled[-1] if demangled else symbol, referenced_by=sorted(references[symbol])))
    output = dict(checked_at=args.checked_at, method='Union of external undefined COFF names minus union of external defined names across64 objects; includes unused COMDAT references. This is a conservative inventory, not actual linker reachability or runtime readiness.', objects=len(report['files']), defined_names=len(definitions), referenced_names=len(references), external_names=len(rows), compile_report_sha256=hashlib.sha256(evidence.read_bytes()).hexdigest(), tool_sha256=hashlib.sha256(tool.read_bytes()).hexdigest(), symbols=rows, linked=False, retail_shaders_tested=0, R3=False)
    (root / 'reports/reconstruction/data/shader_external_symbols.json').write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps({k:output[k] for k in ('objects','defined_names','referenced_names','external_names')}))


if __name__ == '__main__':
    main()
