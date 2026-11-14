#!/usr/bin/env python3
"""Reconstruct only observed IPCLite signal-slot constants from source and saved DT."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from audit_boot_tree import fdt

ROOT=Path(__file__).resolve().parents[1]
NAMES=['IPCLITE_MSG_SIGNAL','IPCLITE_MEM_INIT_SIGNAL','IPCLITE_VERSION_SIGNAL',
       'IPCLITE_TEST_SIGNAL','IPCLITE_SSR_SIGNAL','IPCLITE_DEBUG_SIGNAL','MAX_CHANNEL_SIGNALS']

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write',action='store_true')
    args=parser.parse_args()
    source=ROOT/'ports/qti-fence/synx/msm/synx/ipclite.h'
    values={name:int(re.search(r'^#define\s+'+name+r'\s+(\d+)\s*$',source.read_text(),re.M).group(1)) for name in NAMES}
    if [values[n] for n in NAMES]!=list(range(7)):
        raise ValueError('Review changed driver signal-slot definitions')
    saved=ROOT/'artifacts/dt-input-audit/stock-merged.dtb'
    nodes,_=fdt(saved.read_bytes());observed=[]
    for path,props in nodes.items():
        if not path.startswith('/soc/ipclite/') or 'index' not in props:continue
        raw=props['index']
        if len(raw)!=4:raise ValueError('Malformed signal slot: '+path)
        number=int.from_bytes(raw,'big')
        match=re.search(r'/ipclite_signal_(\d+)$',path)
        if not match or number!=int(match[1]) or number>=values['MAX_CHANNEL_SIGNALS']:
            raise ValueError('Saved slot does not match driver contract: '+path)
        observed.append({'node':path,'index':number})
    if {x['index'] for x in observed}!=set(range(6)):
        raise ValueError('Saved DT does not cover all six signal slots')
    content='''/* SPDX-License-Identifier: GPL-2.0-only */
/* AVR reconstruction of signal SLOT indices from the migrated IPCLite driver
 * and 42 saved DT signal entries. Hardware IPCC signal numbers are separately
 * supplied by the pinned qcom-ipcc-canoe.h, not by these slot indices.
 * This is owned compatibility data; the absent vendor header is not recovered.
 */
#ifndef AVR_DT_IPCLITE_SIGNALS_H
#define AVR_DT_IPCLITE_SIGNALS_H
'''
    content+=''.join('#define '+name+' '+str(values[name])+'\n' for name in NAMES)+'#endif\n'
    target=ROOT/'ports/device-tree/compat/dt-bindings/ipclite-signals.h'
    if args.write:
        target.parent.mkdir(parents=True,exist_ok=True);target.write_text(content)
    if target.read_text()!=content:raise ValueError('Compatibility binding differs from verified inputs')
    report={'scope':'observed signal-slot contract only; not firmware IPC operation or recovery of the absent vendor header',
            'passed':True,'vendor_header_recovered':False,'constants':values,
            'driver_header_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'saved_dtb_sha256':hashlib.sha256(saved.read_bytes()).hexdigest(),
            'saved_signal_entries':observed,'output':str(target.relative_to(ROOT)),
            'output_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
            'hardware_ipcc_signal_ids':'Use the pinned qcom-ipcc-canoe.h; do not infer them from the 0..5 slot indices'}
    (ROOT/'reports/ipclite-dt-binding-reconstruction.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Verified/reconstructed six signal slots against',len(observed),'saved DT entries')

if __name__=='__main__':main()
