"""Refresh compile-only shader evidence from the batch compiler results."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--checked-at', required=True)
    parser.add_argument('--support', action='store_true')
    parser.add_argument('--settings', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    def digest(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    results_path = 'build/CTRPort-data/shader_support_results.json' if args.support else 'build/CTRPort-data/shader_compile_results.json'
    if args.settings:
        results_path='build/CTRPort-data/shader_settings_results.json'
    results = json.loads((root / results_path).read_text(encoding='utf-8-sig'))
    records, failures = [], collections.Counter()
    for result in results:
        raw = Path(result['log']).read_bytes()
        log = raw.decode('utf-16') if raw.startswith(b'\xff\xfe') else raw.decode('utf-8-sig', errors='replace')
        record = dict(source=result['source'], source_sha256=digest(root / 'engine/shadPS4' / result['source']), exit_code=result['exit_code'], log_sha256=digest(result['log']))
        if result['exit_code']:
            match = re.search(r'fatal error: ([^\r\n]+)', log) or re.search(r'error: ([^\r\n]+)', log)
            record['failure'] = match.group(1) if match else 'inspect compiler log'
            failures[record['failure']] += 1
        else:
            record['object_sha256'] = digest(result['object'])
        records.append(record)
    report = dict(status='compile_only', checked_at=args.checked_at, attempted=len(results), passed=sum(r['exit_code'] == 0 for r in results), failure_counts=dict(failures), files=records, build_script_sha256=digest(root / 'ctrport/video/build_shader.ps1'), linked=False, retail_shaders_tested=0, R3=False, method='Unmodified pinned upstream sources; local include roots. COFF compile only, no shader correctness credit.')
    output_path = 'reports/reconstruction/data/shader_support_compile.json' if args.support else 'reports/reconstruction/data/shader_full_compile_diagnostic.json'
    if args.settings:
        output_path='reports/reconstruction/data/shader_settings_compile.json'
        report['method']='Five unmodified upstream sources compiled with explicit ctrport/video/shim/core/emulator_settings.h include substitution; compile only, no shader correctness credit.'
        report['shim_hashes']={p:digest(root/p) for p in ['ctrport/video/shim/core/emulator_settings.h','ctrport/video/shader_settings.h']}
    if args.support:
        report['license_reviews'] = [dict(path=path, sha256=digest(root / path), observed_license=license_name) for path, license_name in [('engine/shadPS4/externals/sirit/LICENSE.txt', 'BSD-3-Clause'), ('engine/shadPS4/externals/fmt/LICENSE', 'MIT'), ('engine/shadPS4/externals/spdlog/LICENSE', 'MIT'), ('engine/shadPS4/externals/zydis/LICENSE', 'MIT'), ('engine/shadPS4/externals/zydis/dependencies/zycore/LICENSE', 'MIT')]]
        report['upstream_review'] = dict(paths=['src/video_core/amdgpu/pixel_format.cpp', 'src/common/decoder.cpp'], revision='e3ce810f3a653f43ac64ebab63023de281a4103a', license='GPL-2.0-or-later', assert_cpp_deferred='Pulls Core::Emulator shutdown; requires explicit fatal host adapter instead of duplicate emulator.')
    (root / output_path).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(attempted=report['attempted'], passed=report['passed'], failures=dict(failures))))


if __name__ == '__main__':
    main()
