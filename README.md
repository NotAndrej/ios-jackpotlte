# iOS on the Samsung Galaxy A8 2018

**iOS 16 on the Samsung Galaxy A8 2018 (jackpotlte, Exynos 7885).**

This repository carries the Galaxy A8 2018 bring-up. It is derived from the
finished Galaxy S8 (`dreamlte`, Exynos 8895) port,
[Floppikrob/ios-galaxy-s8](https://github.com/Floppikrob/ios-galaxy-s8), which
reaches the iOS lock screen. The A8 reuses that kernel patch set, shim and
tooling, and replaces the SoC-specific parts (display hand-off, GIC, timer,
storage, ADT).

> ⚠️ Early bring-up, but iOS now boots. **On the A8, XNU detects the eMMC,
> mounts the HFS+ iOS root from `USERDATA`, runs launchd and starts SpringBoard;
> the panel shows the iOS Apple logo and boot progress bar** (2026-11-08, on one
> core). Reaching the lock screen is not yet confirmed. Installing the root image
> **erases USERDATA** (all Android data). SM-A530F only, other models of the A8
> might not work properly.

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
| 5 | Storage (eMMC) detected | **reached** (screen, 2026-11-08) — `dwmmc0@13500000` maps and the GPT reads |
| 6 | Root filesystem mounts | **reached** (screen, 2026-11-08) — the HFS+ root mounts from `USERDATA` and `/sbin/launchd` executes |
| 7 | launchd, then SpringBoard | **reached (screen output, 2026-11-08)** — launchd clears AMFI with `launchd_unsecure_cache=1`, SpringBoard execs and the panel shows the iOS Apple logo + boot progress bar; reaching the lock screen is not yet confirmed |

### What the phone actually showed (stages 4–7)

**Stage 4.** `Darwin Kernel Version 22.6.0: Mon Aug 14 05:06:07 UTC 2023; xnu-8796.141.3`,
`pe_arm_map_interrupt_controller: found interrupt-controller`,
`gPicBase: 0xfffffff03a400000`. That boot then stopped inside `pmap_startup()`;
the fix is `0125-jackpotlte-pmap-layout-fix.patch` (a deterministic physmap and
VA reserve for the A8).

**Stage 5.** With `0125` the kernel boots past pmap and page-table setup. Mapping
`dwmmc0@13500000` (`0127-jackpotlte-emmc-dwmmc0-base.patch`) then reads the GPT:
`SYSTEM` at LBA 409600, 11,078,400 sectors.

**Stage 6.** Booting with `rd=emmcroot0 emmcroot=USERDATA emmcwrite=1` mounts the
HFS+ root and executes `/sbin/launchd`. The S8-derived kernel killed launchd as
`EXEC_EXIT_REASON_WRONG_PLATFORM` (`namespace 9 code 0xe`) because an
`XNU_TARGET_OS_OSX` kernel requires `unentitled_ios_sim_launch=1` before it will
exec an iOS userland process; adding it (plus `cs_enforcement_disable=1`) fixed
that kill.

**Stage 7 (reached).** launchd runs past the platform check; `launchd_cache_loader`
aborts with `Unable to find AMFI` (`namespace 7`, `OS_REASON_LIBXPC`) until
`launchd_unsecure_cache=1` (the documented S8/A5 fix) is added. With it, on
`jt10`, SpringBoard execs and the panel switches to the iOS Apple logo with the
boot progress bar — iOS userland running on the A8, on one core. Whether it
finishes to the lock screen is not yet confirmed.

### The `-DJACKPOTLTE=1` build flag is mandatory

`pexpert/pexpert/arm64/A5XELTE.h` (patch `0124`) selects the board from the
`JACKPOTLTE` macro and, **when it is absent, defines `DREAMLTE`**. Without
`-DJACKPOTLTE=1` an `A5XELTE` build silently compiles the Galaxy S8 board paths
and every `#if defined(JACKPOTLTE)` block — the A8 battery stub and all five
component ports — becomes dead code. That is exactly what panicked one boot:
`dreamlte_battery_read()` maps the S8 MAX77865 fuel gauge at physical
`0x10970000` and reads it, which external-aborts on the A8. The `xnu` workflow
now sets `EXTRA_MACHINE_FLAGS: -DJACKPOTLTE=1` for push builds.

### Built, verified on the host, not yet booted

The iOS root image (16 GiB, case-sensitive journaled HFS+, label `iOSRoot`,
built from the iPhone X 16.7.16 DMGs with `tools/mkiosroot.sh`) is complete and
fsck-verified, and the merged port set (`0001`–`0133`) applies cleanly on a
pristine `xnu-8796.141.3` tree. Those are host results, not boot claims.

## What works on the A8

| Area | State |
| --- | --- |
| Shim | Runs from the BOOT partition, finds the ramdisk in `/chosen`, validates the `SWEETXNU` payload, builds `boot_args`, jumps to XNU |
| Display hand-off | The shim writes `0x1281` to DECON_F `HW_SW_TRIG_CONTROL` (`0x14860070`); without it no frame is ever scanned and the panel keeps the Samsung logo from its own GRAM |
| Framebuffer | `0xec000000`, 1080×2220, 4320 B stride, drawn from XNU's early console |
| Interrupts | GIC-400 at GICD `0x12301000` / GICC `0x12302000`, mapped by XNU |
| Kernel | xnu-8796 DEVELOPMENT, `MACHINE_CONFIG=A5XELTE` with `-DJACKPOTLTE=1` (mandatory), 4 KiB pages, built on GitHub Actions |
| Storage | eMMC `dwmmc0@13500000` maps and the GPT reads (`0127`) |
| Root filesystem | The HFS+ iOS root mounts from `USERDATA` (`rd=emmcroot0 emmcroot=USERDATA`) |
| Userland | `/sbin/launchd` executes (`unentitled_ios_sim_launch=1`) and starts SpringBoard (`launchd_unsecure_cache=1`); the panel shows the iOS Apple logo + progress bar |
| Ported drivers | Read-only probes for dwc3 USB (`0128`), the PMIC fuel gauge + charger (`0129`), sec_ts touch (`0130`/`0131`), the gpa1 buttons (`0132`) and the DSIM panel (`0133`, log-only by default) |
| RAM | Physmap at `0x880000000`, 512 MiB window (`DRAM_SIZE 0x20000000`) |
| Timer | 26 MHz arch timer, non-secure physical timer PPI 14 (INTID 30) |

Past SpringBoard — the lock screen and the stock apps — the S8 tree already
solved it; on the A8 those are what remain to confirm.

## How it boots

```
S-Boot (stock Samsung bootloader)
  -> BOOT partition: shim (shim/jackpotlte, A8 constants)
     -> unmasks the DECON trigger so scanout starts
     -> verifies the SWEETXNU payload in the initrd
     -> copies XNU + ADT to 0x880000000, builds boot_args
        -> XNU (xnu-8796, A5XELTE + -DJACKPOTLTE=1)
           -> GIC + framebuffer console
           -> eMMC (dwmmc0) -> HFS+ root in USERDATA
              -> /sbin/launchd -> SpringBoard (iOS Apple logo + progress bar)
                 -> [not yet confirmed] lock screen
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
# kernel: GitHub Actions, workflow "xnu". A push build now defaults
# EXTRA_MACHINE_FLAGS to -DJACKPOTLTE=1; a manual dispatch passes it too:
gh workflow run xnu.yml -f machine_config=A5XELTE \
  -f kernel_config=DEVELOPMENT -f extra_cflags=-DJACKPOTLTE=1
# then: kernel + ADT + shim -> Samsung boot image
tools/jackpotlte/make-boot.sh <kernel> boot-a530f.img
fastboot flash BOOT boot-a530f.img && fastboot reboot
```

The shim itself is built locally (`shim/jackpotlte/build.sh`); XNU needs
macOS/Xcode and is built only by CI.

## Layout

| Path | What it is |
| --- | --- |
| `shim/jackpotlte/` | A8 shim: `board.h` (every fact with a source), `loader.c`, `start.S` |
| `shim/dreamlte/` | S8 shim, kept as the reference loader |
| `xnu/patches/0124-dreamlte-s8.patch` | S8 board facts and layout workarounds (also the A8 `A5XELTE.h` board selection) |
| `xnu/patches/0125-jackpotlte-pmap-layout-fix.patch` | deterministic physmap + VA reserve for the A8 (fixed the stage-4 pmap hang) |
| `xnu/patches/0127-jackpotlte-emmc-dwmmc0-base.patch` | eMMC `dwmmc0@13500000` for the A8 (stage 5) |
| `xnu/patches/0128`–`0133` | the A8 component ports: USB, PMIC, sec_ts touch, gpa1 buttons, DSIM panel |
| `tools/mkadt_jackpotlte.py` | Apple Device Tree for the A8 (incl. the `disp0` display service) |
| `tools/mkpayload.py`, `tools/jackpotlte_mkboot.py` | payload builder and Samsung boot-image packer |
| `tools/mkiosroot.sh` | iOS root image (case-sensitive HFS+, cryptexes, keybag) |
| `tools/jackpotlte_bootimg_info.py` | read a stock SM-A530F boot image |
| `docs/jackpotlte-bringup.md` | the readings to collect, and how |
| `JACKPOTLTE_PROGRESS.md` | port log: hardware facts, every build and boot |
| `AGENTS.md` | rules for agents working in this tree (what may be written where) |

## The Galaxy S8 port

This tree grows out of the S8 port, which is complete: XNU, the iOS 16.7.16
userland and its shared cache, launchd, backboardd and the lock screen, with
S6SY761 touch, the S2MPS17 RTC and log capture in ramoops. Its README,
installation guide and boot log are the reference for what the A8 has to
reproduce: **[Floppikrob/ios-galaxy-s8](https://github.com/Floppikrob/ios-galaxy-s8)**.

## Credits

- [Floppikrob](https://github.com/Floppikrob): Galaxy S8 port this tree derives from
- [MakrSas](https://github.com/MakrSas): contributor, mainline Linux on the S8 ([LinuxS8](https://github.com/MakrSas/LinuxS8))
- [orhunnx/ios-a5xelte](https://github.com/orhunnx/ios-a5xelte): the Galaxy A5 2016 port (4 KiB pages, Exynos, S-Boot) the kernel base comes from
- [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet): the original iOS 16 port to the Redmi Note 10 Pro ([docs/upstream-README.md](docs/upstream-README.md))
- Mainline Linux (Exynos 7885/8895 device trees and drivers), [uniLoader](https://github.com/ivoszbg/uniLoader), Eureka-Kernel (Samsung 4.4 sources for the A8)
- Apple's open-source releases (xnu, IOHIDFamily, hfs), [blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build), [ipsw](https://github.com/blacktop/ipsw), [apfs-fuse](https://github.com/sgan81/apfs-fuse)

iOS belongs to Apple and is not distributed here: download the IPSW yourself.

## License

The upstream ios-sweet code, and the code of the two ports derived from it - the Galaxy A8 2018 (`jackpotlte`) port and the Galaxy A5 2016 (`a5xelte`) port - is MIT, see [LICENSE](LICENSE). Code files carry an `SPDX-License-Identifier: MIT` header.

The XNU patches (`xnu/patches/`) are derived from XNU and fall under the [APSL 2.0](https://opensource.apple.com/apsl/). Code written after Linux drivers (for example `shim/dreamlte/touch_probe.c`, `shim/a5xelte/display_console.c`) is GPL-2.0, see the SPDX headers.

The Galaxy S8 (`dreamlte`) material (`shim/dreamlte/`, `tools/dreamlte/`, `tools/dreamlte_bootimg_info.py`, `tools/dreamlte_mkboot.py`, `tools/mkadt_dreamlte.py`) carries no license yet, except the files whose SPDX header already names GPL-2.0.
