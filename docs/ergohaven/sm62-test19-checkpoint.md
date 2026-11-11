# SM62 TEST19 accepted local checkpoint

This branch checkpoints the exact production inputs of `SM62_TEST19_ROWS28_COMMON.uf2`.

- Firmware UF2 SHA256: `164f01b438774280a9ea9588c91fdf080f365d15e47adbfdeaf8affd04e2918d`.
- ELF SHA256: `3119470fef5cde09777c9da62efb14f34a958a1af03d4017cd0c7c9eb2c0ce55`.
- Full delivered source manifest SHA256: `d141fac94cf5bfb3d07f51529a3df4c2e521d82d0c9744bb8b9bd5b218904a81` (44215 files, including dependency sources).
- Parent project checkpoint: `1e2857fe9ebb3e2dcd3468a1803ac5b4a330de49`.
- Hardware target: existing SM62 Pico prototype, 2 MiB, runtime GP29 handedness; one COMMON image for both halves.
- Entropy unchanged.

## Included production work

Preserve current core1 graphics ownership, core0 input/Tap Dance path, flash/power coordination, cooperative frame/timer lifetime fixes, per-half settings preservation and Vial extension, bounded HID/native-name handling, persistence/idle races and the RPC ID/macro playback recursion guards. The only production change from delivered TEST18 to TEST19 is `QP_LVGL_BUFFER_ROWS` 4 to 28. Keep the cooperative budget at 4 ms; this is a work budget, not an artificial 4 ms sleep. Pending frames run without sleeping on core1.

The 28-row single RGB565 buffer is 13440 bytes, 11520 bytes more heap than the 4-row buffer. This is not additional stack or double buffering. Larger strips can delay the flash admission poll; the existing timeout fails closed. Do not infer physical admission bounds or MCU heap high-water from native tests/linker memory.

## Dependency provenance

Patched dependencies remain Git submodules, not flattened vendor trees:

- ChibiOS base `8bd61b804303f1614d574546c2dd735eeabb09f5`, local patch commit `58e8198ea28d19c30903ba832fb62111c8370bac` (core1 startup preparation).
- LVGL base `e19410f8f8a256609da72cff549598e0df6fa4cf`, local patch commit `0e00e1a8461e8486ca68648642c27409c633cc35` (five exact cooperative overlays, including approved timer repair).

These commits and the project checkpoint are local only. Publishing the parent requires publishing the referenced dependency commits to accessible remotes or an explicitly reviewed packaging strategy; do not push a dangling submodule reference.

## Verification and physical result

The delivered source passed normal `make -j8 ergohaven/sm62:v1 VERBOSE=true SKIP_GIT=yes`, affected native scanner/owner/TD/panel/marquee/suspend/flash/pipeline tests, normal ARM macro/callsite inspection and full UF2 block/payload/flash-reserve checks. Those are carried-forward results for identical production inputs, not a new build from this Git commit. The checkpoint process verifies every delivered file byte against the new worktree and committed project/submodule blobs.

A source-backed comparison of current KEYMAP at 4 and 28 rows produced identical final panels in six comparisons. Full-screen flush callbacks 70 to 10 and render-object invocations 3298 to 496 are native operation counts, not physical timing ratios.

After installing TEST19, the user reported: «вот теперь отлично работает». Record this as user-confirmed redraw responsiveness; it does not establish unrelated protocol, persistence, suspend or safety acceptance.

## Remaining limits

This is an accepted development checkpoint, not a complete safety/production certification. Full renderer/event/libc/IRQ transitive stack proof and physical PSP/MSP high-water remain incomplete. Additional malformed RPC metadata-length safety remains unresolved after a provider refusal; no PASS inferred and blocked workflows were not retried. Hardware durability and all unreported acceptance scenarios remain unverified. Keep these separate from the confirmed redraw improvement.

No EEPROM reset/migration was requested. No automatic flash, remote push, PR or merge is authorized by this checkpoint.
