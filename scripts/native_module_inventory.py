#!/usr/bin/env python3
"""Select current native firmware modules from Kbuild, ignoring stale .ko files."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def firmware_modules():
    out = ROOT / 'out/native-gki'
    order = {p[:-2] + '.ko' if p.endswith('.o') else p
             for p in (out / 'modules.order').read_text().splitlines()}
    paths = [out / 'drivers/firmware/qcom' / name for name in
             ['qcom_tzmem.ko', 'qcom_pas.ko', 'qcom-scm.ko']]
    selected = [p for p in paths if p.relative_to(out).as_posix() in order]
    if not any(p.name == 'qcom-scm.ko' for p in selected):
        raise ValueError('Current modular SCM build is required')
    for path in selected:
        if not path.is_file():
            raise ValueError('Kbuild selected a missing module: ' + str(path))
    return selected


def pmic_ipc_modules():
    """Require every selected real native IPC provider; never pick stale .ko files."""
    out = ROOT / 'out/native-gki'
    order = {p[:-2] + '.ko' if p.endswith('.o') else p
             for p in (out / 'modules.order').read_text().splitlines()}
    names = ['net/qrtr/qrtr.ko', 'net/qrtr/qrtr-smd.ko',
             'drivers/soc/qcom/qmi_helpers.ko', 'drivers/soc/qcom/pdr_interface.ko',
             'drivers/soc/qcom/qcom_pdr_msg.ko']
    for name in names:
        if name not in order or not (out / name).is_file():
            raise ValueError('Current native IPC module was not built/selected: ' + name)
    return [out / name for name in names]
