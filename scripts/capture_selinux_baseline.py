#!/usr/bin/env python3
"""Read the running ROM's SELinux mode, policy hash and relevant device labels."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def query(arguments):
    result = subprocess.run(['su', '-c', shlex.join(arguments)], capture_output=True, text=True, check=False)
    return {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}

def main():
    commands = {'mode': ['/system/bin/getenforce'],
                'policy_version': ['/system/bin/cat', '/sys/fs/selinux/policyvers'],
                'policy_sha256': ['/system/bin/sha256sum', '/sys/fs/selinux/policy'],
                'device_labels': ['/system/bin/ls', '-lZ', '/dev/oplus_sync', '/dev/synx_device',
                                  '/dev/kgsl-3d0', '/dev/ntsync']}
    data = {'captured_at_utc': datetime.now(timezone.utc).isoformat(),
            'scope': 'running 6.12 port ROM only; not a 7.3 SELinux validation',
            'queries': {key: query(args) for key, args in commands.items()}}
    baseline = ROOT / 'device-baseline/selinux-baseline.json'
    baseline.write_text(json.dumps(data, indent=2) + '\n')
    labels = {}
    for line in data['queries']['device_labels']['stdout'].splitlines():
        fields = line.split()
        if len(fields) >= 5 and fields[-1].startswith('/dev/'):
            labels[fields[-1]] = next((field for field in fields if field.startswith('u:object_r:')), None)
    report = {'scope': data['scope'], 'captured_at_utc': data['captured_at_utc'],
              'mode': data['queries']['mode']['stdout'].strip(),
              'policy_version': data['queries']['policy_version']['stdout'].strip(),
              'device_labels': labels,
              'baseline_sha256': hashlib.sha256(baseline.read_bytes()).hexdigest(),
              'query_exit_codes': {key: value['exit_code'] for key, value in data['queries'].items()},
              'policy_modified': False, 'runtime_73_verified': False,
              'pending': ['Preserve labels through new ramdisk/ueventd configuration',
                          'Check actual Android service domains and ioctl/xperm requirements',
                          'Diagnose 7.3 AVCs during approved physical tests; no blanket permissive change']}
    policy = data['queries']['policy_sha256']
    if policy['exit_code'] == 0:
        report['running_policy_sha256'] = policy['stdout'].split()[0]
    (ROOT / 'reports/selinux-port-baseline.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    if data['queries']['mode']['exit_code'] or data['queries']['policy_version']['exit_code']:
        raise SystemExit('Baseline capture incomplete')

if __name__ == '__main__':
    main()
