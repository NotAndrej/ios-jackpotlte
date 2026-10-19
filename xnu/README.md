# XNU build targets

The A5XELTE fork builds XNU 8796 locally with `MACHINE_CONFIG=A5XELTE` and 4 KiB pages. Its Samsung-specific changes begin at `patches/0082-a5xelte-4k-entry.patch`; the earlier SWEET patches still supply shared XNU and userland scaffolding. The physical phone has displayed dynamic C/`kprintf` output, passed CPU0 SGI1, registered pthread and HFS, and selected an in-memory `md0` root. The next observed panic was the absent `IOBlockStorageDriverClass` assertion; patch `0103` is under device test. HFS root mount and iOS userland remain unverified. See [the A5XELTE progress log](../A5XELTE_PROGRESS.md) and [migration map](../docs/a5xelte-migration-map.md).

## Original SWEET build notes

Built on GitHub's macOS arm64 runners by `.github/workflows/xnu.yml`, using
[blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build) (MIT) at a pinned commit.

- `MACOS_VERSION=13.5` -> xnu-8796 (same generation as iOS 16.6/16.7, the last iOS for iPhone 8).
- `patches/*.patch` are copied into the build script's patch dir and applied to the xnu tree with `git apply`.
- Output: `kernel.<config>.vmapple` (later: our own SWEET machine config) as a workflow artifact.

Fetch the latest build: `gh run download -R dipa-studio/ios-sweet -n xnu-kernel -D xnu/out`
