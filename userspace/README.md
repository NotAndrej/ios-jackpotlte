# userspace

The phone's side of the GPU work. Everything here is arm64 iOS, so it builds on
macOS with the iPhoneOS SDK and nowhere else; CI does it (`.github/workflows/userspace.yml`).
All of it is MIT (see `LICENSE`).

## Why this exists

M2 and M3 put the GPU in the kernel: power, clocks, the MMU, address space 0,
one job at a time, interrupts. That is where `xnu/patches/0152`-`0154` stop and
where the reference (HakiOS's `redwood_mali.c`) also stops — its own notes say a
Bifrost Mali needs no firmware and it runs one AS0 and one job at a time. The
remaining distance to a fast home screen is userspace, in three pieces:

1. `jackpotlte-maliprobe` (here): prove a Darwin client can reach the GPU
   through `JackpotlteMali` and share memory with it.
2. the shim: the reference's `redwood_drm.c` retargeted from `RedwoodMali` to
   `JackpotlteMali`, so a Linux-shaped GPU userspace (Mesa's panfrost) can open
   the GPU through libdrm.
3. Mesa with Bifrost **v6**, then the `JackpotlteMetal` bundle
   (`0155` publishes the service it plugs into; `JackpotlteMetalDevice`).

## jackpotlte-maliprobe

The first tool, and the smallest thing that answers "does any of this work from
userspace?". It finds the `JackpotlteMali` service, opens it, makes a 16 KB
buffer, maps it, checks it reads back, asks `get_param` for the GPU's identity
and features, and closes the buffer.

```
userspace/jackpotlte-maliprobe/maliprobe.c
userspace/jackpotlte-maliprobe/com.jackpotlte.maliprobe.plist
```

- `maliprobe` — on the phone. Needs the GPU up, so boot with the engine args
  (`jtmalipwr=1 jtmalijob=1 jtmalimetal=1`); without them the service is still
  published but the user client answers `kIOReturnNotReady`.
- `maliprobe --abi` — prints the ABI (selectors, structure sizes and offsets)
  and touches no IOKit, so it also runs on the build machine. **This is the
  point of the CI job:** the numbers are grepped against
  `xnu/patches/0154`, and a structure or selector that drifts on either side
  turns the build red instead of failing on the phone.
- `maliprobe --help`.

It prints to stdout *and* to `/dev/console`: a launchd job's stdout is a file
nobody can read back on this phone, and the panel is photographable.

Its IOKit entry points are declared in the source rather than included — the iOS
SDK ships no IOKit headers — and it links with `-Wl,-undefined,dynamic_lookup`,
so the device's own IOKit resolves them when the tool loads.

The phone's copy is unsigned. It does not have to be signed: the root boots with
`cs_enforcement_disable=1` and `launchd_unsecure_cache=1`, and the kernel
carries the built-in entitlements this needs.

## Build

```
userspace/build.sh [DEST]      # DEST defaults to userspace/out
```

Needs macOS: `out/maliprobe` is arm64 iOS (iPhoneOS SDK), `out/maliprobe.host`
is the local copy for `--abi`. CI runs the same script on `macos-15` and
uploads `out/` as an artifact, so a phone copy can be pulled from a run without
a Mac in the loop.

## Getting a tool into the iOS root

The root is `/var/tmp/IOSRoot.img` (raw master; the sparse copy flashed to
`USERDATA` is derived from it). It is edited on the host, with the mount
scaffolding in `tools/jackpotlte/apply-root-tweaks.sh`; anything that goes in
belongs there, so there is one fsck/mount-rw/umount pass for the whole root.

For `maliprobe` that is:

```
/usr/local/bin/maliprobe
/System/Library/LaunchDaemons/com.jackpotlte.maliprobe.plist
```

and `tools/jackpotlte/apply-root-tweaks.sh` does both, in the mount it already
does, from `userspace/out/` (override with `MALIPROBE_OUT`). It skips them with
a note when the built copy is absent, which is the normal case on a host without
the iPhoneOS SDK.

The probe is quiet on an ordinary boot: it reads `kern.bootargs` first and
exits without touching the panel unless the boot carries the GPU arguments
(`jtmalimetal`/`jtmalipwr`), because the user client answers
`kIOReturnNotReady` otherwise and a line on the panel at every boot is noise.
On a GPU boot it prints its report to the console *before* iOS draws its first
frame, so a photo of the panel is the readback.
