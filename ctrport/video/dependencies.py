# SPDX-License-Identifier: GPL-2.0-only
"""Conservative local include closure; does not pretend to be a linker graph."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
UP = ROOT / 'engine/shadPS4'
SRC = UP / 'src'


def inventory(modules=None):
    # All implementation files in selected modules, not just headers of entrypoints.
    modules = modules or ['shader_recompiler', 'video_core', 'core/libraries/gnmdriver',
                          'core/libraries/videoout']
    seeds = sorted(p for m in modules for p in (SRC / m).rglob('*.cpp'))
    pending, visited, external = list(seeds), {}, {}
    while pending:
        path = pending.pop()
        key = path.relative_to(UP).as_posix()
        if key in visited:
            continue
        raw = path.read_bytes()
        code = raw.decode('utf-8-sig')
        edges = []
        for delimiter, include in re.findall(r'^\s*#include\s*([<"])([^>"\n]+)[>"]', code, re.M):
            found = next((p for p in (path.parent / include, SRC / include) if p.is_file()), None)
            if found:
                edges.append({'include': include, 'local': found.relative_to(UP).as_posix()})
                pending.append(found)
            else:
                edges.append({'include': include, 'local': None, 'delimiter': delimiter})
                external.setdefault(include, set()).add(key)
        visited[key] = {'sha256': hashlib.sha256(raw).hexdigest(), 'includes': edges}
    shim = ROOT / 'reports/reference_material/bloodborne_windows_runtime/gpu/shim'
    shim_files = sorted(p for p in shim.rglob('*') if p.is_file())
    return {'schema': 1, 'upstream_revision': 'e3ce810f3a653f43ac64ebab63023de281a4103a',
            'method': 'All selected module cpp seeds, recursively resolve sibling/src includes. Conditional branches all retained. External/system includes unresolved explicitly. This is not a compile/link closure.',
            'seed_count': len(seeds), 'local_file_count': len(visited),
            'files': dict(sorted(visited.items())),
            'external_or_system': {k: sorted(v) for k, v in sorted(external.items())},
            'bbport_shim': {p.relative_to(shim).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in shim_files},
            'runtime_core_headers': sorted(k for k in visited if k.startswith('src/core/') and '/libraries/gnmdriver/' not in k and '/libraries/videoout/' not in k),
            'compiled_renderer': False, 'retail_shaders_tested': 0}


def reuse_manifest(graph, previous=None):
    """Provenance of proposed inputs, without claiming they have been ported."""
    reviews = json.loads((ROOT / 'ctrport/video/license_reviews.json').read_text())['reviews']
    if previous is None:
        manifest_path = ROOT / 'ctrport/video/reuse_manifest.json'
        previous = json.loads(manifest_path.read_text()) if manifest_path.exists() else {'files': []}
    adaptations = [dict(row) for row in previous['files'] if row.get('actually_reused_in_ctr_video')]
    records = []
    for name, info in graph['files'].items():
        raw = (UP / name).read_bytes()
        identifiers = re.findall(rb'SPDX-License-Identifier:\s*([^\r\n]+)', raw)
        licenses = sorted({value.decode('ascii').strip() for value in identifiers})
        boundary = name in graph['runtime_core_headers']
        records.append({'source': 'engine/shadPS4/' + name,
                        'revision': graph['upstream_revision'],
                        'sha256': info['sha256'], 'spdx': licenses,
                        'license_status': 'declared_in_file' if licenses else 'review_required',
                        'decision': 'host_boundary_review' if boundary else 'proposed_reuse_review',
                        'actually_reused_in_ctr_video': False,
                        'destination': None})
        review = reviews.get(records[-1]['source'])
        if review:
            if review['sha256'] != info['sha256']:
                raise ValueError('License review source changed: ' + name)
            records[-1]['license_review'] = review
            records[-1]['license_status'] = 'header_notice_reviewed_no_spdx'
            records[-1]['decision'] = review['port_decision']
    sources = {row['source']: row for row in records}
    destinations = set()
    for row in adaptations:
        source = sources.get(row['source'])
        if source is None or any(row[key] != source[key] for key in ('sha256', 'revision', 'spdx')):
            raise ValueError('Adaptation source identity changed: ' + row['source'])
        destination = row.get('destination')
        if not destination or (row['source'], destination) in destinations:
            raise ValueError('Missing or duplicate adaptation destination')
        target = (ROOT / destination).resolve()
        if not target.is_relative_to((ROOT / 'ctrport/video').resolve()) or not target.is_file():
            raise ValueError('Adaptation destination missing or outside video: ' + destination)
        destinations.add((row['source'], destination))
        records.append(row)
    return {'schema': 1,
            'method': 'Per-file SPDX/source revision/hash review; proposed selections remain unapproved until marked actually reused. Adapted public helpers and compiled sources are recorded individually; no runtime support inferred from source presence.',
            'files': records, 'file_count': len(sources), 'entry_count': len(records),
            'missing_spdx_count': sum(not row['spdx'] for row in records),
            'host_boundary_count': sum(row['decision'] == 'host_boundary_review' for row in records),
            'actually_reused_count': len(adaptations),
            'actually_reused_source_count': len({row['source'] for row in adaptations}),
            'count_semantics': 'file_count counts distinct source paths; entry_count counts review/adaptation records; actually_reused_count counts adaptation records, not whole files or runtime services.',
            'external_dependency_license_review': 'pending; unresolved includes remain in dependency_inventory.json',
            'bbport_status': 'reference_only; no source copied or compiled'}


if __name__ == '__main__':
    result = inventory()
    (ROOT / 'ctrport/video/dependency_inventory.json').write_text(json.dumps(result, indent=2) + '\n')
    manifest = reuse_manifest(result)
    (ROOT / 'ctrport/video/reuse_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    shader = inventory(['shader_recompiler'])
    cmake = (UP / 'CMakeLists.txt').read_bytes()
    block = re.search(r'set\(SHADER_RECOMPILER\s+(.*?)\)', cmake.decode(), re.S).group(1)
    declared = sorted(re.findall(r'src/shader_recompiler/[^\s)]+\.cpp', block))
    discovered = sorted(p.relative_to(UP).as_posix() for p in (SRC / 'shader_recompiler').rglob('*.cpp'))
    if declared != discovered:
        raise ValueError('Shader source selection differs from pinned upstream CMake')
    shader['compile_sources'] = declared
    shader['upstream_cmake_sha256'] = hashlib.sha256(cmake).hexdigest()
    shader['selection_check'] = 'exact_cpp_set_matches_upstream_CMake_SHADER_RECOMPILER'
    (ROOT / 'ctrport/video/shader_dependency_inventory.json').write_text(json.dumps(shader, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ('seed_count', 'local_file_count', 'runtime_core_headers')}))
