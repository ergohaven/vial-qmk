# Ergohaven QMK firmware

Firmware maintained by [Ergohaven](https://eh.works) for our keyboards, macropads and pointing devices. This fork is based on QMK and Vial, with support for configuration through Entropy.

## Firmware and configuration

Download published firmware from [Releases](https://github.com/ergohaven/vial-qmk/releases). Choose the file for your exact model and hardware revision.

Use [Entropy](https://github.com/ergohaven/entropy) to edit your layout, encoder assignments and supported device settings. Vial clients are also supported. Available settings depend on the device and firmware version.

Before updating firmware, export your current layout to a file in Entropy so you can restore it if needed.

For split keyboards, update both halves with matching firmware. Macropad is a standalone device.

## QMK 1.0

Starting with **1.0**, we are restarting QMK release numbering as we bring our QMK and RMK firmware toward feature parity. This is a new numbering series, not a rollback to older firmware. The two implementations retain their own device-specific features and update schedules.

This release adds Entropy-managed application layouts across the Phenom family, Macropad and SM62, refines the SM62 keymap display, and improves pointing-mode controls and encoder behavior. See the [release notes](keyboards/ergohaven/changelog_ru.md#10) for details.

Application layouts require Entropy running on the connected computer. Factory-layout and default-setting changes do not overwrite your saved configuration.

## Release downloads

The official release includes six model-specific UF2 files:

- Phenom rev1 — `1.0_phenom_v1.uf2`
- Phenom Mini rev1 — `1.0_phenom_mini_v1.uf2`
- Phenom Micro rev1 — `1.0_phenom_micro_v1.uf2`
- Macropad v2 — `1.0_macropad_v2.uf2`
- Macropad v3 — `1.0_macropad_v3.uf2`
- SM62 — `1.0_sm62_v1.uf2`

Choose the file matching your model and hardware revision; the files are not interchangeable. Each split model uses the same image for its two halves. Use `SHA256SUMS.txt` from the release to verify your download.

Device sources are in [keyboards/ergohaven](keyboards/ergohaven/). The [release-scope validator](util/ci_ergohaven_release_scope.py) defines the official build inventory.

## Building firmware

Maintainers can run the [Build Ergohaven workflow](https://github.com/ergohaven/vial-qmk/actions/workflows/build-ergohaven.yml) to produce test firmware for the current release targets without publishing a release.

For local builds, install the QMK CLI and use the ARM toolchain pinned in [the build workflow](.github/workflows/build-ergohaven.yml). From the repository root, initialize the submodules and compile the required target. For example, to build Phenom:

```sh
git submodule update --init --recursive
qmk compile -kb ergohaven/phenom/rev1 -km v1
```

List the official keyboard and keymap combinations with:

```sh
python3 util/ci_ergohaven_release_scope.py --matrix
```

## Upstream and license

This project builds on [QMK](https://github.com/qmk/qmk_firmware) and [Vial](https://github.com/vial-kb/vial-qmk). General firmware development documentation is available at [docs.qmk.fm](https://docs.qmk.fm).

See [LICENSE](LICENSE) for the repository license. Individual files and bundled libraries may carry their own notices.
