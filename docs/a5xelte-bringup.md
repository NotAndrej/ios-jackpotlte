# a5xelte iOS 16 bring-up

Target: SM-A510F, Exynos 7580. Donor: this repository's sweet XNU 8796 and iOS 16 userland. Baseline commit: 11da58bd050672960c0a83b5bef2176047eea479. The donor README reports a SpringBoard boot on Redmi Note 10 Pro. No a5xelte XNU build or boot is claimed here.

## First device milestone

Keep the working postmarketOS BOOT checkpoint and private backups. Use a separate a5xelte configuration. First prove that one CPU enters a Samsung-specific shim and XNU and produces recoverable text diagnostics. Do not prepare the iOS root or change USERDATA for this milestone.

Run `python3 tools/a5xelte-preflight.py` on the existing Linux installation. It reads the live device tree. The CPU feature value is useful only when supplied from a trusted raw EL1 capture: Linux masks the TGran16 field in its EL0 CPU register emulation, so a userspace `MRS` result of zero does not prove the hardware value. A raw EL1 measurement on this device has now confirmed the 16 KiB incompatibility; see below.

The Linux port already captured a Samsung handoff at EL1 (CurrentEL 0x4), wrapper load 0x40205000 and FDT 0x48000000 in its attempt3 recovery log. Recheck these against the selected working BOOT before building the XNU shim; do not treat them as universal firmware constants. Samsung uses a 2048-byte Android BOOT header with DTBH; the donor's Xiaomi boot.img v2 and `fastboot boot` path do not apply. Use the existing port's guarded BOOT packaging and recovery route only after a candidate passes offline checks.

## Live CPU granule result (2026-10-19)

The running 7.2.8 a5xelte Linux kernel reported MIDR_EL1 `0x410fd033` (Cortex-A53 r0p3). A temporary GPL module built against that exact kernel read `ID_AA64MMFR0_EL1=0x00000000001122` at EL1; `TGran16[23:20]=0`. The module was unloaded and its tmpfs copy removed. This is a device-side register observation, not an XNU boot. Arm's Cortex-A53 TRM defines zero as no 16 KiB translation granule.

The donor's `shim/mmu.c` uses `PG_SHIFT=14`, `TG0=16K`, `TG1=16K` and 2048-entry page tables; `SWEET.h` defines `__ARM_16K_PG__`. These cannot be reused on this CPU. The a5xelte path must use 4 KiB tables and a distinct XNU board configuration compiled for ARMv8-A. Apple's XNU 8796 BCM2837 board config demonstrates an ARM64 target without `__ARM_16K_PG__`, but the donor's GICv3, timer and storage paths still require Exynos-specific replacements. iOS 16 userland operation on a 4 KiB XNU kernel remains unverified.

The reproducible register probe is in `tools/a5xelte-mmfr0/`. Build it with `make -C "$KDIR" M="$PWD/tools/a5xelte-mmfr0" modules` against the exact running kernel. Loading a module is a live kernel operation; run it only on the lab device and remove it immediately afterward.

## Separate 4 KiB MMU diagnostic

`shim/a5xelte/build-probe4k.sh` builds a Samsung Image for a single-core EL1 test. It verifies 4 KiB granule support, constructs 4 KiB L1/L2/L3 identity tables, enables the MMU, records TCR/SCTLR and a result code in the reserved ramoops page, then requests TWRP recovery through the Exynos7580 PMU reset-mode registers. It does not read or write a block device. The image and BOOT package have passed offline assembly, ELF relocation and header checks; this is **not** evidence of device boot or XNU execution.

The completed-capture path requests recovery automatically so that the original working BOOT can be restored through TWRP. An exception before that path may still require the hardware recovery keys. Keep the previously verified working BOOT file and recovery ADB path available before a live test. No iOS root image or USERDATA change is part of this diagnostic.

## Board map from the existing Linux port

These are Linux-source facts, not XNU runtime proof. References: `mainline/exynos7580-a5xelte-minimal.dts`, `mainline/exynos7580-a5xelte-native.dts` and `mainline/package-boot.py` in the sma510fpmos workspace.

| Item | a5xelte | sweet assumption to replace |
| --- | --- | --- |
| CPU | Eight Cortex-A53; MPIDR 0..3, 0x100..0x103; PSCI 0.1 SMC | Kryo A55/A76 topology |
| Interrupts | GICv2 distributor 0x10c01000 and CPU interface 0x10c02000 | GICv3 redistributors and ICC system registers |
| Timer | 26 MHz; virtual timer PPI 11, INTID 27 | 19.2 MHz; INTID 19 |
| RAM | Starts 0x40000000; excludes firmware and multimedia reservations | Upper bank at 0x100000000 |
| Storage | DesignWare eMMC at 0x13540000, SPI 245 | Qualcomm UFS and ufsroot0 |
| Boot | Android BOOT plus DTBH rev3 for hardware rev4; BOOT p10; observed EL1 handoff | Temporary Xiaomi fastboot boot |
| Data | Linux USERDATA is in use; EFS p3 and recovery p11 preserved | HFS+ root in USERDATA |

The diagnostic DTS explicitly calls itself a scaffold. Check addresses against the installed BOOT DTB and read-only runtime evidence before hard-coding them.

## Implementation order

1. A5xelte shim: validate the 4 KiB MMU transition and recover the result through TWRP. The raw EL1 granule result is complete. Do not attempt the donor's 16 KiB MMU setup. Compile for baseline ARMv8-A.
2. A5xelte XNU machine config: GICv2 interrupt acknowledgement, timer PPI and one CPU. Then test PSCI secondaries.
3. Recoverable panic log, eMMC read-only and root-device identification.
4. Exynos display, touch and keys using downstream Linux and Lineage source as hardware references.
5. iOS 16 root/userland only after memory and storage sizing: this board has 2 GiB RAM and about 11.5 GiB USERDATA, so the donor's 16 GiB root-image command is unsuitable as written.

Build output, kernel observations and user-verified screen/input behavior must be recorded separately. This branch performs no partition operation.
