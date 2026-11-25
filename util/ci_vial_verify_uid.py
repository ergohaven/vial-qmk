#!/usr/bin/env python3
from glob import glob
from pathlib import Path
import re
import sys
import struct
from collections import defaultdict

def find_uid(config_h, pattern, visited=None):
    """Read literal UIDs from config.h and its repository-local quoted includes."""
    config_h = config_h.resolve()
    visited = set() if visited is None else visited
    if (not config_h.is_relative_to(Path("keyboards").resolve())
            or config_h in visited or not config_h.is_file()
            or config_h.stat().st_size >= 100000):
        return None
    visited.add(config_h)
    content = config_h.read_text()
    uid = pattern.search(content)
    if uid:
        return uid
    for include in re.finditer(r'^\s*#\s*include\s+"([^"\r\n]+)"', content, re.M):
        uid = find_uid(config_h.parent / include[1], pattern, visited)
        if uid:
            return uid
    return None


def main():

    VIAL_UID_REGEX = re.compile(r"#\s*define\s+VIAL_KEYBOARD_UID\s+(?:\\(?:\n|\r)\s*)*{\s*((?:0(?:x|X)(?:[0-9a-fA-F]){2}\s*,\s*){7}(?:0(?:x|X)(?:[0-9a-fA-F]){2}))\s*}")

    error = 0
    uid_to_keyboards = defaultdict(set)

    for filename in glob("keyboards/**/vial.json", recursive=True):
        keyboard = filename[10:-10].split("/keymaps/")[0]

        dirname = Path(filename).parents[0]
        uid = None

        while True:
            config_h = dirname.joinpath("config.h")
            uid = find_uid(config_h, VIAL_UID_REGEX)
            if uid:
                break
            dirname = dirname.parents[0]
            if dirname.match("keyboards"):
                break

        if not uid:
            print("Keyboard {} does not define a VIAL_KEYBOARD_UID".format(keyboard))
            error = 1
            continue

        uid = uid[1].split(",")
        uid.reverse()
        uid = [int(x, 16) for x in uid]
        uid = struct.pack("BBBBBBBB", *uid).hex()
        uid_to_keyboards[uid].add(keyboard)

        print("{} uses UID {}".format(keyboard, uid))

    for uid, keyboards in uid_to_keyboards.items():
        if len(keyboards) > 1:
            print("UID {} duplicated: {}".format(uid, keyboards))
            error = 1

    return error

if __name__ == "__main__":
    sys.exit(main())
