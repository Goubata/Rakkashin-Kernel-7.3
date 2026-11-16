#!/usr/bin/env python3
"""Decompress LZ4 legacy frames (magic 0x184C2102, used by Android vendor ramdisks).

usage: lz4_legacy.py IN OUT   (pure Python; no external lz4 needed)
"""
import struct
import sys


def block(src, out):
    i, n = 0, len(src)
    while i < n:
        token = src[i]; i += 1
        lit = token >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1; lit += b
                if b != 255:
                    break
        out += src[i:i + lit]; i += lit
        if i >= n:
            break
        off = src[i] | (src[i + 1] << 8); i += 2
        ml = token & 15
        if ml == 15:
            while True:
                b = src[i]; i += 1; ml += b
                if b != 255:
                    break
        ml += 4
        start = len(out) - off
        if off >= ml:
            out += out[start:start + ml]
        else:
            for k in range(ml):
                out.append(out[start + k])


def main():
    data = open(sys.argv[1], 'rb').read()
    out = bytearray()
    i = 0
    while i + 4 <= len(data):
        if struct.unpack_from('<I', data, i)[0] != 0x184C2102:
            break
        i += 4
        while i + 4 <= len(data):
            size = struct.unpack_from('<I', data, i)[0]
            if size == 0x184C2102:
                break
            i += 4
            block(data[i:i + size], out)
            i += size
    open(sys.argv[2], 'wb').write(out)
    print(f'{len(data)} -> {len(out)} bytes')


if __name__ == '__main__':
    main()
