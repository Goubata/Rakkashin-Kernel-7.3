#!/usr/bin/env python3
"""Lock and replay the real ADC and PMIC thermal ports against pinned Motorola source."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent / 'avr-upstream-audit/qcom-soc'
COMMIT = '037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
INPUTS = json.loads((ROOT / 'reports/adc-thermal-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('ADC/thermal source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-adc-thermal/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/adc-thermal-source-lock.json'
    patch = ROOT / 'patches/adc-thermal-drivers-7.3.patch'
    originals = {n: subprocess.check_output(['git', '-C', str(REPO), 'show', COMMIT + ':' + p]) if p else b''
                 for n, p in FILES.items()}
    if args.record:
        rows = []
        changes = ''
        for name, original in originals.items():
            path = name
            data = (ROOT / path).read_bytes()
            rows.append({'file': path, 'source': FILES[name], 'original_sha256': sha(original), 'sha256': sha(data)})
            changes += ''.join(difflib.unified_diff(original.decode().splitlines(True), data.decode().splitlines(True),
                                                   fromfile='a/' + path, tofile='b/' + path))
        patch.write_text(changes)
        report = {'source_commit': COMMIT, 'files': rows, 'patch_sha256': sha(patch.read_bytes()),
                  'scope': 'source and reviewed patch replay only; module link is recorded separately; physical acceptance pending',
                  'reviewed_changes': [
                      'Private firmware enum, data and DT-binding headers forced before native headers',
                      'Make unexported callbacks static and import the native IIO_CONSUMER namespace',
                      'Validate scale function, prescale index, pointers and calibration divisors before indexing',
                      'Initialize IRQ-scheduled work before requesting interrupts or registering thermal zones',
                      'Publish the provider only after successful IIO registration; serialize registry changes',
                      'Quiesce IRQs then global work then client queues before releasing thermal resources',
                      'Cover partial DT/IRQ/thermal probe failure and IIO registration failure with cleanup',
                      'Do not destroy a client workqueue while holding the mutex its worker needs',
                      'Bind new client records to their actual parameter before assigning the client ID',
                      'Propagate client allocation/reverse-scaling failures and reject non-client channels',
                      'Validate fwnode specifier arity, SDAM addresses and nonempty register array',
                      'Track requested IRQs across freeze/restore and unwind partial restore',
                      'Commit cached critical threshold and initialized state only after successful register writes',
                      'Restart DAC/LITE stage enumeration for repeated restore and preserve DT IRQ trigger mode',
                      'Reject unsupported GEN2 revision and out-of-range thermal register bases; defer missing regmap',
                      'Select explicit WQ_PERCPU affinity required by 7.3, preserving the prior queue mode',
                      'Preserve all firmware conversion tables, calibration constants and per-channel GEN4 selection',
                  ],
                  'pending': [
                      'Physical ADC/IRQ/thermal-alarm probe and calibration, threshold/overheat behavior',
                      'Supplier links for exported raw ADC handles and concurrent consumer unbind lifetime',
                      'Client notification list iteration, parameter/context lifetime and disable synchronization',
                      'Transactional hardware configuration after failed conversion/write and suspend/resume concurrency',
                      'Thermal alarm register variants, trip boundaries, fault injection and physical safety acceptance',
                      'Review remaining public legacy conversion API input contracts and unused configurations',
                  ]}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('ADC/thermal source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('ADC/thermal source list mismatch')
    replay = ROOT / 'artifacts/adc-thermal-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('ADC/thermal source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('ADC/thermal patch replay mismatch')
    print('Verified ADC/thermal source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
