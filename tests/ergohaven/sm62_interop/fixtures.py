"""Deterministic FULL4560 inputs shared by interop, restart and USB probes.
These are synthetic test inputs, never firmware or simulated test results.
"""
import argparse
import binascii
import hashlib
import os
from pathlib import Path
import struct
import zlib


def fixture(slot):
    identity = hashlib.sha1(f'c-interop-{slot}'.encode()).digest()
    name = f'App-{slot:03}-'.encode().ljust(22, b'N')
    keys = b''.join(struct.pack('<H', 4 + slot + l * 62 + k) for l in range(16) for k in range(62))
    visuals = bytes((slot + i) % 17 for i in range(992))
    stacks = b''
    crcstacks = b''
    for l in range(16):
        crcstacks += bytes([4])
        for s in range(4):
            n = f'S{slot:03}L{l:02}X{s}'.encode().ljust(12, b'x')
            codes = struct.pack('<HH', 4 + slot + s, 9 + l + s)
            stacks += bytes([4, 12]) + codes + n
            crcstacks += codes + bytes([12]) + n
    layers = b''.join(bytes([22]) + f'L{l:02}-Slot{slot:03}'.encode().ljust(22, b'L') for l in range(16))
    crc = binascii.crc_hqx(keys + visuals + crcstacks + bytes([22]) + name + layers, 0xffff)
    begin = bytearray(32)
    begin[:3] = bytes([0xe1, 8, 1])
    begin[3:7] = struct.pack('<I', slot + 1)
    begin[7:10] = bytes([62, 16, 22])
    begin[10:32] = name
    commit = bytearray(32)
    commit[:3] = bytes([0xe3, 8, 1])
    commit[3:7] = begin[3:7]
    commit[7:9] = struct.pack('<H', crc)
    commit[9:11] = b'W\2'
    commit[11:31] = identity
    raw = bytes(begin + commit) + keys + visuals + stacks + layers
    assert len(raw) == 4560
    return raw, identity, zlib.crc32(raw)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args(argv)
    out = args.output.resolve()
    cache = Path(os.environ['AGENT_BUILD_CACHE']).resolve()
    if not args.output.is_absolute() or os.path.lexists(args.output) or cache not in out.parents:
        parser.error('output must be fresh, absolute and inside AGENT_BUILD_CACHE')
    out.mkdir()
    for slot in range(100):
        (out / f'full{slot:02}.bin').write_bytes(fixture(slot)[0])
    print('Generated 100 deterministic FULL4560 protocol test inputs')


if __name__ == '__main__':
    main()
