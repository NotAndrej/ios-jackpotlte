# a5xelte iOS 16 bring-up

Target: SM-A510F, Exynos 7580. Donor: this repository's sweet XNU 8796 and iOS 16 userland. Baseline commit: 11da58bd050672960c0a83b5bef2176047eea479. The donor README reports a SpringBoard boot on Redmi Note 10 Pro. No a5xelte XNU build or boot is claimed here.

## First device milestone

Keep the working postmarketOS BOOT checkpoint and private backups. Use a separate a5xelte configuration. First prove that one CPU enters a Samsung-specific shim and XNU and produces recoverable text diagnostics. Do not prepare the iOS root or change USERDATA for this milestone.

Run `python3 tools/a5xelte-preflight.py` on the existing Linux installation. It reads the live device tree and the CPU feature register if Linux exposes it. Unknown 16 KiB granule support remains a gate.

Capture the installed BOOT header, DTBH hardware-revision entry, load addresses and handoff exception level before building the shim. Samsung uses a 2048-byte Android BOOT header with DTBH; the donor's Xiaomi boot.img v2 and `fastboot boot` path do not apply. Use the existing port's guarded BOOT packaging and recovery route only after a candidate passes offline checks.

## Board map from the existing Linux port

These are Linux-source facts, not XNU runtime proof. References: `mainline/exynos7580-a5xelte-minimal.dts`, `mainline/exynos7580-a5xelte-native.dts` and `mainline/package-boot.py` in the sma510fpmos workspace.

| Item | a5xelte | sweet assumption to replace |
| --- | --- | --- |
| CPU | Eight Cortex-A53; MPIDR 0..3, 0x100..0x103; PSCI 0.1 SMC | Kryo A55/A76 topology |
| Interrupts | GICv2 distributor 0x10c01000 and CPU interface 0x10c02000 | GICv3 redistributors and ICC system registers |
| Timer | 26 MHz; virtual timer PPI 11, INTID 27 | 19.2 MHz; INTID 19 |
| RAM | Starts 0x40000000; excludes firmware and multimedia reservations | Upper bank at 0x100000000 |
| Storage | DesignWare eMMC at 0x13540000, SPI 245 | Qualcomm UFS and ufsroot0 |
| Boot | Android BOOT plus DTBH rev3 for hardware rev4; BOOT p10 | Temporary Xiaomi fastboot boot |
| Data | Linux USERDATA is in use; EFS p3 and recovery p11 preserved | HFS+ root in USERDATA |

The diagnostic DTS explicitly calls itself a scaffold. Check addresses against the installed BOOT DTB and read-only runtime evidence before hard-coding them.

## Implementation order

1. A5xelte shim: validated RAM bounds, MMU-off entry, exception report and safe stop. Verify 16 KiB granule before using the donor page tables. Compile for baseline ARMv8-A rather than the donor's ARMv8.2 target until CPU features are measured.
2. A5xelte XNU machine config: GICv2 interrupt acknowledgement, timer PPI and one CPU. Then test PSCI secondaries.
3. Recoverable panic log, eMMC read-only and root-device identification.
4. Exynos display, touch and keys using downstream Linux and Lineage source as hardware references.
5. iOS 16 root/userland only after memory and storage sizing: this board has 2 GiB RAM and about 11.5 GiB USERDATA, so the donor's 16 GiB root-image command is unsuitable as written.

Build output, kernel observations and user-verified screen/input behavior must be recorded separately. This branch performs no partition operation.
