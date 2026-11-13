#!/usr/bin/env python3
"""Extract module files only from the saved Android vendor_boot v4 image."""
import json
from pathlib import Path
import shlex
import struct
import subprocess

PORT = Path(__file__).resolve().parents[1]
BASE = PORT / 'device-baseline'
ROOTFS = PORT.parent / '.andronix/distros/ubuntu/rootfs'

def align(n, page):
    return (n + page - 1) // page * page

def cpio_modules(data, dest):
    offset = 0
    result = []
    while offset + 110 <= len(data):
        header = data[offset:offset+110]
        if header[:6] not in (b'070701', b'070702'):
            raise ValueError(f'Unexpected CPIO header at {offset}')
        fields = [int(header[6+i*8:14+i*8], 16) for i in range(13)]
        size, namesize = fields[6], fields[11]
        start = offset + 110
        name = data[start:start+namesize-1].decode()
        start = align(start+namesize, 4)
        if start + size > len(data):
            raise ValueError('Truncated CPIO data')
        if name == 'TRAILER!!!':
            break
        path = Path(name)
        if '..' in path.parts or path.is_absolute():
            raise ValueError('Unsafe archive path')
        if path.suffix == '.ko' or path.name.startswith('modules.'):
            target = dest/path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data[start:start+size])
            result.append(name)
        offset = align(start+size, 4)
    return result

def main():
    data = (BASE/'images/vendor_boot_a.img').read_bytes()
    assert data[:8] == b'VNDRBOOT'
    version, page = struct.unpack_from('<II', data, 8)
    assert version == 4
    rsize = struct.unpack_from('<I', data, 24)[0]
    hsize, dsize = struct.unpack_from('<II', data, 2096)
    tsize, count, esize = struct.unpack_from('<III', data, 2112)
    rstart = align(hsize, page)
    tstart = rstart + align(rsize, page) + align(dsize, page)
    assert esize >= 108 and count*esize <= tsize
    rows = []
    fragments = BASE/'vendor-fragments'
    fragments.mkdir(exist_ok=True)
    for i in range(count):
        size, offset, kind = struct.unpack_from('<III', data, tstart+i*esize)
        assert offset+size <= rsize
        part = fragments/f'{i}.lz4'
        part.write_bytes(data[rstart+offset:rstart+offset+size])
        command = ['chroot', str(ROOTFS), '/usr/bin/lz4', '-d', '-c',
                   f'/work/avr-port/device-baseline/vendor-fragments/{i}.lz4']
        unpacked = subprocess.check_output(['su', '-c', shlex.join(command)])
        names = cpio_modules(unpacked, BASE/'early-modules'/str(i))
        rows.append({'fragment': i, 'type': kind, 'files': names})
    (BASE/'early-modules.json').write_text(json.dumps(rows, indent=2)+'\n')
    print('Fragments:', count, 'early modules:', sum(n.endswith('.ko') for r in rows for n in r['files']))

if __name__ == '__main__':
    main()
