#!/usr/bin/env python3
"""Lock and replay the real PMIC pinctrl ports against pinned Motorola source."""
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
INPUTS = json.loads((ROOT / 'reports/pmic-pinctrl-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('Diagnostic source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/pmic-pinctrl/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def load_original(name):
    row = next((r for r in INPUTS['files'] if r['file'] == name), None)
    if row is None:
        return b''
    pins = {'qcom-soc': (REPO, COMMIT),
            'kernel-common': (ROOT.parent/'avr-upstream-audit/vendor-repos/kernel-common',
                              '479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4')}
    repo, commit = pins[row['repo']]
    if row['commit'] != commit:
        raise ValueError('Interconnect source commit mismatch')
    data = subprocess.check_output(['git','-C',str(repo),'show',commit+':'+row['source']])
    if sha(data) != row['original_sha256']:
        raise ValueError('Original source digest mismatch')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/pmic-pinctrl-source-lock.json'
    patch = ROOT / 'patches/pmic-pinctrl-drivers-7.3.patch'
    originals = {n: load_original(n) for n in FILES}
    native_commit='4dd306a07b2f456fd70ddaa28f4f65640374f202'
    native=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/android-7.3'),
                                    'show',native_commit+':drivers/pinctrl/qcom/pinctrl-spmi-gpio.c']).decode()
    port=(ROOT/'ports/pmic-pinctrl/pinctrl-spmi-gpio.c').read_text()
    def directions(text):
        a=text.index('static int pmic_gpio_direction_input(')
        b=text.index('static int pmic_gpio_get(',a)
        return text[a:b]
    if directions(port)!=directions(native):
        raise ValueError('Reviewed native GPIO direction semantics changed')

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
                  'additional_source_commit': '479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4',
                  'native_gpio_reference_commit': native_commit,
                  'native_gpio_direction_sha256': sha(directions(native).encode()),
                  'reviewed_changes': [
                      'Use native PIN_CONFIG_LEVEL and return actual write status from GPIO set',
                      'Preserve binding constants from pinned Motorola headers',
                      'Validate physical pin indices and argument count before translation',
                      'Use exact native CMOS/open-drain/open-source direction implementations',
                      'Sampling input preserves the requested output latch value',
                      'Reject invalid pin counts and defer until parent regmap exists',
                      'Derive can_sleep from the actual parent regmap; native remove callback'],
                  'known_open_items': [
                      'Physical pad subtype/voltage/pull/strength, readback and IRQ delivery',
                      'Concurrent pad cache access and partial-register-write recovery',
                      'Parent PMIC/IRQ domain and suspend/removal lifetime validation',
                      'MPP callback/probe tests are not provided by the GPIO callback suite',
                      'Target duplicate SID4 and full board/ROM/power acceptance remain open']}

        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('Diagnostic source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('Diagnostic source list mismatch')
    replay = ROOT / 'artifacts/pmic-pinctrl-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Diagnostic source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Diagnostic patch replay mismatch')
    print('Verified PMIC pinctrl source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
