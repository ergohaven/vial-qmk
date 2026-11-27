"""Immutable 4.2.0 controls, NOT shipping firmware qualification.

Read only the checked-in historical fixture closure, verify byte digests before
extraction, and materialize headers in managed output (never a source checkout).
The closure is copied byte-for-byte from the pinned commit. This stays usable
with a shallow CI checkout; missing or changed bytes are a hard failure.
"""
from pathlib import Path
import hashlib
import json
import subprocess

PIN = 'f6d3d4d50d4b78d18622ba345d5617d3f20e4e6a'
SHA256 = {'keyboards/ergohaven/hid.c': '4cc2d57a27bc3d8f5b4519b56316cdc14ae097f28a8785abd022e6b596835f3b', 'quantum/via.c': 'ba5ecb71b1abb040aae706d07e9fdf39d081d812f5a92d5597efd222c72fb130', 'quantum/vial.c': '8421256acc6ffa561dc5b6bf7fe4d7059ddac7904896658f4f6652593de783f9', 'quantum/vial.h': '60db52651f02d887714cf9ae2f1abb2abfdcd4fc76d0158b5c5b4610bd656cf7', 'quantum/keycodes.h': '0a4cf54e4d2bd33fdbc12492fca88446545769abce7ab38bbd790c4aa019f523', 'keyboards/ergohaven/sm62/compact_usb.h': '27d3e88a86d756066893a211b6c7b5bf61474d99072c67c4d14f5171e9ed8330', 'keyboards/ergohaven/sm62/resident_cache.h': 'b807c900ebfebcd70fc9981fed8a630ac7430d57e29dd24083ee14033304b357', 'keyboards/ergohaven/sm62/resident_cache_flash.h': 'e163b62ad0e9661fa732acfdd61b914f608db23b352fa7cd0b05ac9c11776172', 'keyboards/ergohaven/sm62/application_geometry.h': '77f26c5a286db4f66589f921faa38680b53098e6d99fabc4f5408fc0b5f8522f', 'tests/ergohaven/resident100_native.c': 'f43d628433bcc15509af7fd9a15a9066f98930656a7ed149d7e83c106e724614', 'tests/ergohaven/test_resident100_restart.py': '7a62bc7c0fa667384770cb1ad4b9da4cea991e72b822bd7ca5412ccf80feca66', 'tests/ergohaven/sm62_locked_boot/cases.c': '0880a5702efd588d22b41b4965686835662844fc3e056eea234835fd824bfef0', 'quantum/nvm/eeprom/nvm_dynamic_keymap.c': '7431d6f9ceac4adefdd797e108c4324ee619a31724127a7c099d005d989c8798', 'quantum/dynamic_keymap.c': '2be8993b7d27c30881487c81b57590b05b9769267ee6b522668f625f0a5ed6e9', 'quantum/via.h': '5e2dcad39cb63b482a302131b679ff3a9d7e3492a5cfd9dce279fba6417e2696'}


def historical_source(root, path):
    expected = SHA256[path]
    blob = (Path(root) / 'tests/ergohaven/fixtures/resident_4_2_0' / path).read_bytes()
    assert hashlib.sha256(blob).hexdigest() == expected, 'historical source digest: ' + path
    return blob.decode()


def receipt(output):
    Path(output).write_text(json.dumps(dict(
        classification='historical-4.2.0-only; not shipping qualification',
        commit=PIN, source_sha256=SHA256), indent=2) + '\n')
