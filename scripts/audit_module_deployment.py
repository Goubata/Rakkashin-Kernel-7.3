#!/usr/bin/env python3
"""Compare archived module placement with real staged 7.3 ELF metadata; no loading."""
import argparse
from collections import defaultdict
import hashlib
import heapq
import json
from pathlib import Path
from audit_module_versions import sections

ROOT = Path(__file__).resolve().parents[1]
def normalize(name):
    return Path(name).name.removesuffix('.ko').replace('-', '_')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', default='android73-regulators-stage17')
    args = parser.parse_args()
    stage = ROOT / 'artifacts' / args.stage
    metadata = json.loads((stage / 'metadata.json').read_text())
    baseline = ROOT / 'device-baseline'
    placements = defaultdict(set)
    for row in json.loads((baseline / 'module-inventory.json').read_text()):
        placements[normalize(row['module'])].add(row['partition'] + '_dlkm')
    for fragment in json.loads((baseline / 'early-modules.json').read_text()):
        for path in fragment['files']:
            if path.endswith('.ko'):
                placements[normalize(path)].add('vendor_boot:fragment-' + str(fragment['fragment']))
    coverage = json.loads((baseline / 'module-coverage.json').read_text())
    loaded = {normalize(line.split()[0]) for line in (baseline / 'loaded-modules.txt').read_text().splitlines() if line.strip()}
    if len(loaded) != coverage['loaded_names']:
        raise ValueError('Saved loaded-module inventory count mismatch')
    staged = {}
    for relative in metadata['modules']:
        path = stage / relative
        sec = sections(path.read_bytes())
        info = defaultdict(list)
        for entry in sec.get('.modinfo', b'').split(b'\0'):
            if b'=' in entry:
                key, value = entry.decode().split('=', 1)
                info[key].append(value)
        # Use ELF module name where supplied; matching a filename is not proof of equivalence.
        name = normalize(info.get('name', [path.stem])[0])
        if name in staged:
            raise ValueError('Duplicate staged module name: ' + name)
        depends = {normalize(n) for field in info.get('depends', []) for n in field.split(',') if n}
        vermagic = info.get('vermagic', [])
        if len(vermagic) != 1 or not vermagic[0].startswith('7.3.'):
            raise ValueError('Unexpected module vermagic: ' + relative)
        staged[name] = {'file': relative, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                        'vermagic': vermagic[0], 'depends': sorted(depends), 'softdep': info.get('softdep', []),
                        'archived_placement_candidates': sorted(placements.get(name, [])),
                        'loaded_on_baseline': name in loaded}
    for row in staged.values():
        row['dependencies_absent_from_stage'] = sorted(set(row['depends']) - staged.keys())
    def ordered_graph(graph):
        remaining = {name: set(dependencies) for name, dependencies in graph.items()}
        ready = [name for name, dependencies in remaining.items() if not dependencies]
        heapq.heapify(ready)
        ordered = []
        while ready:
            name = heapq.heappop(ready)
            ordered.append(name)
            for user, dependencies in remaining.items():
                if name in dependencies:
                    dependencies.remove(name)
                    if not dependencies:
                        heapq.heappush(ready, user)
        return ordered, sorted(staged.keys() - set(ordered))
    hard_graph = {name: set(row['depends']) & staged.keys() for name, row in staged.items()}
    ordered, unresolved_order = ordered_graph(hard_graph)
    soft_graph = {name: set(dependencies) for name, dependencies in hard_graph.items()}
    for name, row in staged.items():
        soft = {'pre': set(), 'post': set()}
        for field in row['softdep']:
            direction = None
            for token in field.split():
                if token in ('pre:', 'post:'):
                    direction = token[:-1]
                elif direction:
                    soft[direction].add(normalize(token))
                else:
                    raise ValueError('Unrecognized softdep syntax: ' + field)
        row['soft_dependencies_absent_from_stage'] = sorted((soft['pre'] | soft['post']) - staged.keys())
        soft_graph[name] |= soft['pre'] & staged.keys()
        for after in soft['post'] & staged.keys():
            soft_graph[after].add(name)
    soft_order, unresolved_soft_order = ordered_graph(soft_graph)
    missing = sorted(placements.keys() - staged.keys())
    report = {'scope': 'name/ELF dependency/placement inventory only; no hardware or ABI equivalence inferred',
              'stage': args.stage, 'source_commit': metadata['source_commit'],
              'archived_unique_count': len(placements), 'staged_count': len(staged),
              'same_name_candidate_count': len(placements.keys() & staged.keys()),
              'archived_names_without_same_name_candidate_count': len(missing),
              'loaded_names_without_same_name_candidate_count': len(loaded - staged.keys()),
              'all_staged_module_dependencies_present': all(not r['dependencies_absent_from_stage'] for r in staged.values()),
              'all_staged_soft_dependencies_present': all(not r['soft_dependencies_absent_from_stage'] for r in staged.values()),
              'elf_dependency_graph_acyclic': not unresolved_order,
              'combined_hard_soft_graph_acyclic': not unresolved_soft_order,
              'combined_dependency_order': [{'name': n, 'file': staged[n]['file']} for n in soft_order],
              'combined_dependency_cycle_or_blocked_names': unresolved_soft_order,
              'elf_dependency_order': [{'name': n, 'file': staged[n]['file']} for n in ordered],
              'dependency_cycle_or_blocked_names': unresolved_order,
              'order_scope': 'ELF hard dependencies only; not DT probe readiness, init order or softdep resolution',
              'image_packaging_ready': False,
              'blockers': ['Missing hardware drivers and providers',
                           'Same-name candidates require probe and hardware validation',
                           'Init load order, softdeps, signing and ROM policy review pending',
                           'Old 6.12 modules cannot be carried over as 7.3 modules'],
              'staged_modules': staged,
              'archived_without_same_name_candidate': [{'name': n, 'placements': sorted(placements[n]),
                                                        'loaded_on_baseline': n in loaded} for n in missing],
              'loaded_without_same_name_candidate': sorted(loaded - staged.keys())}
    target = ROOT / 'reports/module-deployment-readiness.json'
    target.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in
                      ('staged_modules', 'archived_without_same_name_candidate', 'loaded_without_same_name_candidate', 'elf_dependency_order', 'combined_dependency_order')}, indent=2))

if __name__ == '__main__':
    main()
