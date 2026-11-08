# iOS on the Samsung Galaxy A8 2018

**XNU (xnu-8796, the iOS 16 generation) on the SM-A530F (Exynos 7885, `jackpotlte`).**

This repository carries the Galaxy A8 2018 bring-up. It is derived from the
finished Galaxy S8 (`dreamlte`, Exynos 8895) port,
[Floppikrob/ios-galaxy-s8](https://github.com/Floppikrob/ios-galaxy-s8), which
reaches the iOS lock screen. The A8 reuses that kernel patch set, shim and
tooling, and replaces the SoC-specific parts (display hand-off, GIC, timer,
storage, ADT).

> ⚠️ Early bring-up, not a usable port yet. **On the A8, XNU boots, maps the
> GIC and prints to the framebuffer console, then stops inside
> `pmap_startup()`.** No iOS userland has run on the A8 yet. Installing the
> root image **erases USERDATA** (all Android data). SM-A530F only — the
> Snapdragon A530W/U are different machines.

The living status, with every image hash and photo reference, is
**[JACKPOTLTE_PROGRESS.md](JACKPOTLTE_PROGRESS.md)**. The readings still to be
collected from the phone are in
**[docs/jackpotlte-bringup.md](docs/jackpotlte-bringup.md)**.

## Stage status

A stage counts as reached only with a photo, a serial log or screen output
from the phone. Build success is never a boot claim.

| Stage | Description | Status |
| --- | --- | --- |
| 1 | Repo skeleton, S8 port imported, builds reproducibly | in progress (builds) |
| 2 | Shim runs on the A8 and prints its banner | **reached** (screen, 2026-11-08) |
| 3 | Payload passes its check, jump to XNU | **reached** (shim prints through `JUMP TO XNU`) |
| 4 | XNU prints to the framebuffer | **reached** (screen, 2026-11-08) |
| 5 | Storage (eMMC) detected | not started |
| 6 | Root filesystem mounts | not started |
| 7 | launchd, then SpringBoard | not started |

### What the phone actually showed (stage 4)

`Darwin Kernel Version 22.6.0: Mon Aug 14 05:06:07 UTC 2023; xnu-8796.141.3`,
`pe_arm_map_interrupt_controller: found interrupt-controller`,
`gPicBase: 0xfffffff03a400000`, and then silence after our own marker
`A5 vm: vm_page_bootstrap -> pmap_startup`. The framebuffer console wraps its
rows, so the kernel really stops printing rather than running out of screen.
That window was bisected with A5XELTE-gated `pmap_startup()` /
`pmap_steal_memory_internal()` markers (former patches 0125–0127, since removed
because their per-call prints flooded the console — see the "Patch renumbering"
note in `JACKPOTLTE_PROGRESS.md`); the fix is
`0125-jackpotlte-pmap-layout-fix.patch`.

### Built, verified on the host, not yet booted

The iOS root image (16 GiB, case-sensitive journaled HFS+, label `iOSRoot`,
built from the iPhone X 16.7.16 DMGs with `tools/mkiosroot.sh`) is complete and
fsck-verified. It is a host result, not stage 6: nothing on the A8 has mounted
it yet.

## What works on the A8

| Area | State |
| --- | --- |
| Shim | Runs from the BOOT partition, finds the ramdisk in `/chosen`, validates the `SWEETXNU` payload, builds `boot_args`, jumps to XNU |
| Display hand-off | The shim writes `0x1281` to DECON_F `HW_SW_TRIG_CONTROL` (`0x14860070`); without it no frame is ever scanned and the panel keeps the Samsung logo from its own GRAM |
| Framebuffer | `0xec000000`, 1080×2220, 4320 B stride, drawn from XNU's early console |
| Interrupts | GIC-400 at GICD `0x12301000` / GICC `0x12302000`, mapped by XNU |
| Kernel | xnu-8796 DEVELOPMENT, `MACHINE_CONFIG=A5XELTE` with `-DJACKPOTLTE=1`, 4 KiB pages, built on GitHub Actions |
| RAM | Physmap at `0x880000000`, 512 MiB window (`DRAM_SIZE 0x20000000`) |
| Timer | 26 MHz arch timer, non-secure physical timer PPI 14 (INTID 30) |

Everything below stage 4 — eMMC, UFS/HFS+ root, dyld, launchd, SpringBoard — is
untouched on this device and is what the S8 tree already solved.

## How it boots

```
S-Boot (stock Samsung bootloader)
  -> BOOT partition: shim (shim/dreamlte built with the A8 constants)
     -> unmasks the DECON trigger so scanout starts
     -> verifies the SWEETXNU payload in the initrd
     -> copies XNU + ADT to 0x880000000, builds boot_args
        -> XNU (xnu-8796, A5XELTE + 0124 + 0125)
           -> ... currently stops in pmap_startup() ...
           -> [S8 only, not yet on the A8] eMMC -> HFS+ root in USERDATA
              -> dyld + shared cache -> launchd -> SpringBoard
```

## Flashing

The A8 is flashed through **fastbootd** (not Odin). Two details that cost time
to learn:

1. **Partition names are uppercase.** `fastboot flash boot ...` fails with
   `remote: 'No such file or directory'`; `fastboot flash BOOT ...` works.
   BOOT is 32 MiB.
2. **USERDATA must be a sparse image.** fastboot loads the whole raw image into
   memory first (`total-vm` equals the file size), which on a small host ends
   in an OOM kill rather than a flash. Convert with `img2simg` (or write the
   same format) and fastboot streams it in ~256 MiB chunks:

   ```sh
   img2simg iOSRoot.img iOSRoot.sparse.img
   fastboot flash USERDATA iOSRoot.sparse.img
   ```

   On this board USERDATA is `0x5122FBE00` bytes (20.3 GiB), so the 16 GiB
   image fits (the tail keeps whatever was there).

Build and flash BOOT:

```sh
# kernel: GitHub Actions, workflow "xnu"
gh workflow run xnu.yml -f machine_config=A5XELTE \
  -f kernel_config=DEVELOPMENT -f extra_cflags=-DJACKPOTLTE=1
# then: kernel + ADT + shim -> Samsung boot image
tools/jackpotlte/make-boot.sh <kernel> boot-a530f.img
fastboot flash BOOT boot-a530f.img && fastboot reboot
```

The shim itself is built locally (`shim/jackpotlte/build.sh`, or the S8 shim
with the A8 constants); XNU needs macOS/Xcode and is built only by CI.

## Layout

| Path | What it is |
| --- | --- |
| `shim/jackpotlte/` | A8 shim: `board.h` (every fact with a source), `loader.c`, `start.S` |
| `shim/dreamlte/` | S8 shim; the image that reached stage 4 is this one with the A8 constants (FB `0xec000000`, 1080×2220, DECON `0x14860070`) |
| `xnu/patches/0124-dreamlte-s8.patch` | S8 board facts and layout workarounds |
| `xnu/patches/0125-jackpotlte-pmap-layout-fix.patch` | deterministic physmap + VA reserve for the A8 (fixed the stage-4 pmap hang) |
| `tools/mkadt_jackpotlte.py` | Apple Device Tree for the A8 |
| `tools/mkpayload.py`, `tools/dreamlte_mkboot.py` | payload builder and Samsung boot-image packer |
| `tools/mkiosroot.sh` | iOS root image (case-sensitive HFS+, cryptexes, keybag) |
| `tools/jackpotlte_bootimg_info.py` | read a stock SM-A530F boot image |
| `docs/jackpotlte-bringup.md` | the readings to collect, and how |
| `JACKPOTLTE_PROGRESS.md` | port log: hardware facts, every build and boot |
| `AGENTS.md` | rules for agents working in this tree (what may be written where) |

## The Galaxy S8 port

This tree grows out of the S8 port, which is complete: XNU, the iOS 16.7.16
userland and its shared cache, launchd, backboardd and the lock screen, with
S6SY761 touch, the S2MPS17 RTC and log capture in ramoops. Its README,
installation guide and boot log stay useful as the reference for what the A8
has to reproduce: **[Floppikrob/ios-galaxy-s8](https://github.com/Floppikrob/ios-galaxy-s8)**,
and locally `DREAMLTE_PROGRESS.md`, `docs/s8-install.md`.

## Credits

- [Floppikrob](https://github.com/Floppikrob): Galaxy S8 port this tree derives from
- [MakrSas](https://github.com/MakrSas): contributor, mainline Linux on the S8 ([LinuxS8](https://github.com/MakrSas/LinuxS8))
- [orhunnx/ios-a5xelte](https://github.com/orhunnx/ios-a5xelte): the Galaxy A5 2016 port (4 KiB pages, Exynos, S-Boot) the kernel base comes from
- [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet): the original iOS 16 port to the Redmi Note 10 Pro ([docs/upstream-README.md](docs/upstream-README.md))
- Mainline Linux (Exynos 7885/8895 device trees and drivers), [uniLoader](https://github.com/ivoszbg/uniLoader), Eureka-Kernel (Samsung 4.4 sources for the A8)
- Apple's open-source releases (xnu, IOHIDFamily, hfs), [blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build), [ipsw](https://github.com/blacktop/ipsw), [apfs-fuse](https://github.com/sgan81/apfs-fuse)

iOS belongs to Apple and is not distributed here: download the IPSW yourself.

## License

The XNU patches are derived from XNU and fall under the [APSL 2.0](https://opensource.apple.com/apsl/). Code written after Linux drivers (for example `shim/dreamlte/touch_probe.c`) is GPL-2.0, see the SPDX headers. The upstream projects ios-sweet and ios-a5xelte did not state an overall license, so the remaining files have no separate license yet.