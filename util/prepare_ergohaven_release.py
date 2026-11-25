#!/usr/bin/env python3
"""Prepare official QMK downloads from the exact supported firmware inventory."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from ci_ergohaven_release_scope import REQUIRED_RELEASE_TARGETS


def release_name(keyboard, keymap, version):
    raw = keyboard.replace("/", "_") + "_" + keymap + ".uf2"
    # Preserve the published single-UF2 names and Entropy's exact asset matching.
    return re.sub(r"rev\d_", "", raw, count=1).replace("ergohaven_", version + "_", 1)


def validate_uf2(data, name):
    if not data or len(data) % 512 or len(data) > 16 * 1024 * 1024:
        raise ValueError("Invalid UF2 size: " + name)
    count = len(data) // 512
    for index in range(count):
        block = data[index * 512:(index + 1) * 512]
        magic0, magic1, flags, address, size, number, total, family = struct.unpack_from("<8I", block)
        if ((magic0, magic1) != (0x0A324655, 0x9E5D5157)
                or struct.unpack_from("<I", block, 508)[0] != 0x0AB16F30
                or not flags & 0x2000 or family != 0xE48BFF56
                or size != 256 or number != index or total != count
                or address != 0x10000000 + index * size):
            raise ValueError("Invalid RP2040 UF2 block: " + name)


def validate_release_version(version, allow_patch_version=False):
    """Firmware releases use MAJOR.MINOR unless a patch release is requested."""
    if version == "latest":
        return
    component = r"(?:0|[1-9][0-9]?)"
    pattern = component + r"\." + component
    if allow_patch_version:
        pattern = "(?:" + pattern + "|" + component + r"\.[0-9]\.[0-9])"
    if not re.fullmatch(pattern, version):
        raise ValueError("Use an exact MAJOR.MINOR firmware tag; a third component requires --allow-patch-version")


def prepare(input_dir, output_dir, version, source_commit, allow_patch_version=False):
    validate_release_version(version, allow_patch_version)
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        raise ValueError("The exact 40-character firmware source commit is required")
    expected = {
        kb.replace("/", "_") + "_" + km + ".uf2": (kb, km)
        for kb, km in REQUIRED_RELEASE_TARGETS
    }
    if len(expected) != len(REQUIRED_RELEASE_TARGETS):
        raise ValueError("Colliding input firmware filenames")
    found = {}
    root = input_dir.resolve(strict=True)
    for path in sorted(input_dir.rglob("*.uf2")):
        if path.is_symlink() or not path.resolve(strict=True).is_relative_to(root):
            raise ValueError("Unsafe firmware input path: " + str(path))
        if path.name in found:
            raise ValueError("Duplicate firmware input: " + path.name)
        found[path.name] = path
    missing, extra = set(expected) - set(found), set(found) - set(expected)
    if missing or extra:
        raise ValueError("Firmware inventory mismatch; missing=" + repr(sorted(missing)) + "; extra=" + repr(sorted(extra)))
    assets, inputs = {}, []
    for upstream, (keyboard, keymap) in sorted(expected.items()):
        data = found[upstream].read_bytes()
        validate_uf2(data, upstream)
        name = release_name(keyboard, keymap, version)
        inputs.append({"target": keyboard + ":" + keymap, "input": upstream,
                       "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        if name in assets:
            raise ValueError("Colliding release filename: " + name)
        assets[name] = data
    if output_dir.exists() and (not output_dir.is_dir() or any(output_dir.iterdir())):
        raise FileExistsError("Preserve the existing output; select an empty directory")
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs = []
    for name, data in sorted(assets.items()):
        path = output_dir / name
        with path.open("xb") as stream:
            stream.write(data)
        if path.read_bytes() != data:
            raise OSError("Release asset readback failed: " + name)
        outputs.append({"name": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    if {path.name for path in output_dir.iterdir()} != set(assets):
        raise ValueError("Unexpected release output")
    return {"version": version, "firmware_source_commit": source_commit, "firmware_count": len(inputs),
            "asset_count": len(outputs),
            "inputs": inputs, "assets": outputs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--allow-patch-version", action="store_true",
                        help="Only for an explicitly requested three-component firmware release")
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    if args.manifest.exists():
        raise FileExistsError("Preserve the existing manifest: " + str(args.manifest))
    manifest = prepare(args.input, args.output, args.version, args.source_commit, args.allow_patch_version)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    with args.manifest.open("x", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")
    print("Prepared {} firmware variants as {} downloads".format(
        manifest["firmware_count"], manifest["asset_count"]))


if __name__ == "__main__":
    main()
