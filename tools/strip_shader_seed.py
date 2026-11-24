#!/usr/bin/env python3
"""Export XBE references from a local FFR Direct3D recipes.bin recording.
Usage: strip_shader_seed.py recipes.bin default.xbe output.bin
Only programmable vertex recipes found verbatim in the verified retail XBE
are eligible. Pixel combiner definitions, pipeline structs, raw GPU programs,
SPIR-V and driver cache bytes must stay local. NV2A uses a separate format.
"""
import argparse
import hashlib
from pathlib import Path
import struct

from take_d3d import XBE_SHA256

MAGIC = b'FFRD3DC1'
RECORD = struct.Struct('<cQI')


def strip(data, xbe):
    if hashlib.sha256(xbe).hexdigest() != XBE_SHA256:
        raise ValueError('wrong XBE revision')
    if not data.startswith(MAGIC):
        raise ValueError('expected an FFR Direct3D recipe recording')
    out = bytearray(MAGIC)
    pos = len(MAGIC)
    while pos < len(data):
        if len(data) - pos < RECORD.size:
            raise ValueError('truncated recipe header')
        kind, key, length = RECORD.unpack_from(data, pos)
        pos += RECORD.size
        if length > len(data) - pos:
            raise ValueError('truncated recipe')
        payload = data[pos:pos + length]
        pos += length
        if kind in (b'F', b'P', b'K'):
            continue
        if kind != b'V' or length < 15:
            raise ValueError('unknown recipe')
        regs, decode, quad, words = struct.unpack_from('<HQBI', payload)
        if quad != 0 or not words or words % 4 or length != 15 + words * 4:
            raise ValueError('invalid programmable vertex recipe')
        offset = xbe.find(payload[15:])
        if offset >= 0:
            ref = struct.pack('<HQBII', regs, decode, quad, offset, words)
            out.extend(RECORD.pack(b'W', key, len(ref)))
            out.extend(ref)
    if len(out) == len(MAGIC):
        raise ValueError('no XBE-backed vertex recipes found')
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('recording', 'xbe', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    result = strip(args.recording.read_bytes(), args.xbe.read_bytes())
    with args.output.open('xb') as stream:
        stream.write(result)
