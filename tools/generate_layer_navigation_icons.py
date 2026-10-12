#!/usr/bin/env python3
"""Reproduce exact approved Equal-Corner bytes; never re-rasterize the SVGs.

The SVGs and pixel-preview are provenance, not a platform-dependent rasterizer
input. The independently approved packed masters are hash-checked on every run.
"""
import argparse
import hashlib
import json
from pathlib import Path

FIRMWARE_ROOT = Path(__file__).resolve().parents[1]
MASTER = FIRMWARE_ROOT / 'tools/artwork/layer-navigation'


def icon(next_layer: bool) -> bytes:
    name = 'layer_next_icon.bin' if next_layer else 'layer_prev_icon.bin'
    manifest = json.loads((MASTER / 'provenance.json').read_text())
    data = (MASTER / name).read_bytes()
    assert len(data) == 154
    assert hashlib.sha256(data).hexdigest() == manifest['sha256'][name]
    return data


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--entropy-assets', type=Path, required=True)
    assets = parser.parse_args().entropy_assets
    previous, following = icon(False), icon(True)
    output = ['#pragma once', '#include <stdint.h>',
              '// Approved Equal-Corner 35x35 pictograms, row-major, MSB first.',
              '// Generated from hash-checked tools/artwork/layer-navigation masters.']
    for name, bitmap in (('layer_prev_icon', previous), ('layer_next_icon', following)):
        output.append(f'static const uint8_t {name}[154] = {{')
        output.extend('    ' + ', '.join(f'0x{byte:02X}' for byte in bitmap[start:start + 14]) + ','
                      for start in range(0, len(bitmap), 14))
        output.append('};')
    (FIRMWARE_ROOT / 'keyboards/ergohaven/macropad/layer_navigation_icons.h').write_text('\n'.join(output) + '\n')
    assets.mkdir(parents=True, exist_ok=True)
    (assets / 'display-layer-prev.bin').write_bytes(previous)
    (assets / 'display-layer-next.bin').write_bytes(following)
    # Only replace layer entries 20/21; preserve every other catalog byte/order.
    catalog_path = assets / 'firmware-builtin-icons.bin'
    catalog = catalog_path.read_bytes()
    assert len(catalog) == 22 * 154
    catalog_path.write_bytes(catalog[:20 * 154] + previous + following)


if __name__ == '__main__':
    main()
