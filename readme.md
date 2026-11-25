# Ergohaven QMK firmware

Firmware maintained by [Ergohaven](https://eh.works) for our keyboards, macropads and pointing devices. This fork is based on QMK and Vial, with support for configuration through Entropy.

## Firmware and configuration

Download published firmware from [Releases](https://github.com/ergohaven/vial-qmk/releases). Choose the file for your exact model and hardware revision.

Use [Entropy](https://github.com/ergohaven/entropy) to edit your layout, encoder assignments and supported device settings. Vial clients are also supported. Available settings depend on the device and firmware version.

Before updating firmware, export your current layout to a file in Entropy so you can restore it if needed.

HPD v2 release builds package the supported module variants in one ZIP, with instructions for selecting and flashing the correct variant.

## Current release targets

- Phenom, Phenom Mini and Phenom Micro
- K:03 v3/v4
- K:03 Pro v2/v3
- Imperial44 v3/v4
- HPD v2
- Velvet v2/v3
- Macropad v2/v3
- SM30 and SM62
- Trackball v1/v2

Device sources are in [keyboards/ergohaven](keyboards/ergohaven/). The [release-scope validator](util/ci_ergohaven_release_scope.py) defines the official build inventory.

Retired models are preserved in [archive/ergohaven](archive/ergohaven/README.md) and excluded from current builds. Their historical firmware remains available in Releases. Archived sources are frozen snapshots, not build targets in the current tree.

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
