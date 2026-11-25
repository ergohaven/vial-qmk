# Frozen historical sources

- Do not edit, reformat, rename symbols, bump firmware versions or update these device sources.
- Do not add this directory to live keyboard discovery, include paths or firmware builds.
- Do not restore retired targets or their sources into `keyboards/` as a fallback.
- Preserve the exact files and modes recorded in `manifest.json`.
- The manifest digest is pinned in `util/ci_ergohaven_release_scope.py`. A mismatch is a blocker, not permission to refresh hashes.
- Historical builds use the complete snapshot commit and its pinned dependencies, not current shared code. New work or a deliberate archive extension needs explicit maintainer authorization.
