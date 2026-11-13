#!/usr/bin/env python3
"""Resolve every project in the reviewed official manifest to a commit."""
import concurrent.futures
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT.parent / 'avr-upstream-audit/android73-default.xml'
COMMON = '4dd306a07b2f456fd70ddaa28f4f65640374f202'

def resolve(item):
    name, revision = item
    url = 'https://android.googlesource.com/' + name
    output = subprocess.check_output(
        ['git', 'ls-remote', '--exit-code', url, 'refs/heads/' + revision],
        text=True, timeout=120)
    rows = output.strip().splitlines()
    if len(rows) != 1:
        raise RuntimeError(f'Expected one ref for {name}: {revision}')
    return name, rows[0].split()[0]

def main():
    tree = ET.parse(INPUT)
    manifest = tree.getroot()
    default = manifest.find('default').attrib['revision']
    projects = manifest.findall('project')
    jobs = [(p.attrib['name'], p.attrib.get('revision', default)) for p in projects
            if p.attrib['name'] != 'kernel/common']
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        pins = dict(pool.map(resolve, jobs))
    pins['kernel/common'] = COMMON
    for project in projects:
        project.set('revision', pins[project.attrib['name']])
    for node in manifest.findall('superproject'):
        manifest.remove(node)
    manifest.find('remote').set('fetch', 'https://android.googlesource.com/')
    ET.indent(tree, space='  ')
    tree.write(ROOT / 'manifests/android-7.3-pinned.xml', encoding='utf-8', xml_declaration=True)
    (ROOT / 'reports/aosp-source-lock.json').write_text(json.dumps(pins, indent=2) + '\n')
    print(f'Pinned {len(projects)} projects, common={COMMON}')

if __name__ == '__main__':
    main()
