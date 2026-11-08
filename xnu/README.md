# XNU build targets

The jackpotlte (Galaxy A8 2018) fork builds XNU 8796 with `MACHINE_CONFIG=A5XELTE` plus `-DJACKPOTLTE=1` and 4 KiB pages. The Samsung-specific changes are the A5XELTE patches from `patches/0082` onward and the jackpotlte patches (`0124`+); the earlier SWEET patches supply shared XNU and userland scaffolding. See [the jackpotlte progress log](../JACKPOTLTE_PROGRESS.md) for what the phone has actually shown.

## Original SWEET build notes

Built on GitHub's macOS arm64 runners by `.github/workflows/xnu.yml`, using
[blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build) (MIT) at a pinned commit.

- `MACOS_VERSION=13.5` -> xnu-8796 (same generation as iOS 16.6/16.7, the last iOS for iPhone 8).
- `patches/*.patch` are copied into the build script's patch dir and applied to the xnu tree with `git apply`.
- Output: `kernel.<config>.vmapple` (later: our own SWEET machine config) as a workflow artifact.

Fetch the latest build: `gh run download -R dipa-studio/ios-sweet -n xnu-kernel -D xnu/out`
