#!/usr/bin/env python3
"""Exact user-approved pixels, immutable provenance and both Entropy catalogs."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import unittest
import struct
import zlib


def preview_rgb():
    """Decode the approved noninterlaced RGB PNG with stdlib only."""
    raw = (GOLDEN / 'pixel-preview.png').read_bytes()
    width, height, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', raw[16:29])
    assert (depth, color, compression, filtering, interlace) == (8, 2, 0, 0, 0)
    compressed = b''
    offset = 8
    while offset < len(raw):
        size = struct.unpack_from('>I', raw, offset)[0]
        if raw[offset+4:offset+8] == b'IDAT': compressed += raw[offset+8:offset+8+size]
        offset += size + 12
    stream = zlib.decompress(compressed)
    rows = []
    previous = bytearray(width * 3)
    for y in range(height):
        offset = y * (width * 3 + 1)
        kind = stream[offset]
        row = bytearray(stream[offset+1:offset+1+width*3])
        for x in range(len(row)):
            a = row[x-3] if x >= 3 else 0
            b = previous[x]
            c = previous[x-3] if x >= 3 else 0
            p = a + b - c
            pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
            predictor = (0, a, b, (a+b)//2, a if pa <= pb and pa <= pc else b if pb <= pc else c)[kind]
            row[x] = (row[x] + predictor) & 255
        rows.append(row)
        previous = row
    return rows

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = Path(__file__).parent / 'golden'
ENTROPY = ROOT.parent / 'entropy-pictograms-fix'

class Parity(unittest.TestCase):
    def test_every_preview_pixel_matches_packed_master(self):
        rows = preview_rgb()
        for direction, left in (('prev', 50), ('next', 350)):
            data = (GOLDEN / f'layer_{direction}_icon.bin').read_bytes()
            for y in range(35):
                for x in range(35):
                    bit = y * 35 + x
                    expected = bytes([255 if data[bit//8] & (0x80 >> (bit%8)) else 0]) * 3
                    for dy in range(8):
                        row = rows[40+y*8+dy]
                        for dx in range(8):
                            start = (left+x*8+dx)*3
                            self.assertEqual(bytes(row[start:start+3]), expected, (direction,x,y,dx,dy))

    def test_approved_pixels_everywhere(self):
        manifest = json.loads((GOLDEN / 'provenance.json').read_text())
        for name, digest in manifest['sha256'].items():
            self.assertEqual(hashlib.sha256((GOLDEN / name).read_bytes()).hexdigest(), digest, name)
        header = (ROOT / 'keyboards/ergohaven/macropad/layer_navigation_icons.h').read_text()
        spec = importlib.util.spec_from_file_location('generator', ROOT / 'tools/generate_layer_navigation_icons.py')
        assert spec is not None and spec.loader is not None
        generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(generator)
        catalog = (ENTROPY / 'assets/firmware-builtin-icons.bin').read_bytes()
        self.assertEqual(len(catalog), 22 * 154)
        self.assertEqual(hashlib.sha256(catalog[:20*154]).hexdigest(), manifest['unchanged_catalog_prefix_sha256'])
        for i, direction in enumerate(('prev', 'next')):
            golden = (GOLDEN / f'layer_{direction}_icon.bin').read_bytes()
            body = re.search(rf'layer_{direction}_icon\[154\] = \{{(.*?)\}};', header, re.S)[1]
            actual = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', body))
            with self.subTest(direction=direction, source='firmware'): self.assertEqual(actual, golden)
            with self.subTest(direction=direction, source='generator'): self.assertEqual(generator.icon(bool(i)), golden)
            with self.subTest(direction=direction, source='display'): self.assertEqual((ENTROPY / f'assets/display-layer-{direction}.bin').read_bytes(), golden)
            with self.subTest(direction=direction, source='picker'): self.assertEqual(catalog[(20+i)*154:(21+i)*154], golden)

if __name__ == '__main__': unittest.main()
