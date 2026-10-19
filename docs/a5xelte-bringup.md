# a5xelte iOS 16 bring-up

Target: SM-A510F, Exynos 7580. Donor: this repository's sweet XNU 8796 and iOS 16 userland. Baseline commit: 11da58bd050672960c0a83b5bef2176047eea479. The donor README reports a SpringBoard boot on Redmi Note 10 Pro. The a5xelte XNU build and first device entry are verified separately below.

## First device milestone

Keep the working postmarketOS BOOT checkpoint and private backups. Use a separate a5xelte configuration. First prove that one CPU enters a Samsung-specific shim and XNU and produces recoverable text diagnostics. Do not prepare the iOS root or change USERDATA for this milestone.

Run `python3 tools/a5xelte-preflight.py` on the existing Linux installation. It reads the live device tree. The CPU feature value is useful only when supplied from a trusted raw EL1 capture: Linux masks the TGran16 field in its EL0 CPU register emulation, so a userspace `MRS` result of zero does not prove the hardware value. A raw EL1 measurement on this device has now confirmed the 16 KiB incompatibility; see below.

The Linux port already captured a Samsung handoff at EL1 (CurrentEL 0x4), wrapper load 0x40205000 and FDT 0x48000000 in its attempt3 recovery log. Recheck these against the selected working BOOT before building the XNU shim; do not treat them as universal firmware constants. Samsung uses a 2048-byte Android BOOT header with DTBH; the donor's Xiaomi boot.img v2 and `fastboot boot` path do not apply. Use the existing port's guarded BOOT packaging and recovery route only after a candidate passes offline checks.

## Live CPU granule result (2026-10-19)

The running 7.2.8 a5xelte Linux kernel reported MIDR_EL1 `0x410fd033` (Cortex-A53 r0p3). A temporary GPL module built against that exact kernel read `ID_AA64MMFR0_EL1=0x00000000001122` at EL1; `TGran16[23:20]=0`. The module was unloaded and its tmpfs copy removed. This is a device-side register observation, not an XNU boot. Arm's Cortex-A53 TRM defines zero as no 16 KiB translation granule.

The donor's `shim/mmu.c` uses `PG_SHIFT=14`, `TG0=16K`, `TG1=16K` and 2048-entry page tables; `SWEET.h` defines `__ARM_16K_PG__`. These cannot be reused on this CPU. The a5xelte path must use 4 KiB tables and a distinct XNU board configuration compiled for ARMv8-A. Apple's XNU 8796 BCM2837 board config demonstrates an ARM64 target without `__ARM_16K_PG__`, but the donor's GICv3, timer and storage paths still require Exynos-specific replacements. iOS 16 userland operation on a 4 KiB XNU kernel remains unverified.

The reproducible register probe is in `tools/a5xelte-mmfr0/`. Build it with `make -C "$KDIR" M="$PWD/tools/a5xelte-mmfr0" modules` against the exact running kernel. Loading a module is a live kernel operation; run it only on the lab device and remove it immediately afterward.

## Separate 4 KiB MMU diagnostic

`shim/a5xelte/build-probe4k.sh` builds a Samsung Image for a single-core EL1 test. It verifies 4 KiB granule support, constructs 4 KiB L1/L2/L3 identity tables, enables the MMU, records TCR/SCTLR and a result code in the reserved ramoops page, then requests TWRP recovery through the Exynos7580 PMU reset-mode registers. The probe itself does not access block devices. The host packer and flash operation are separate.

On 2026-10-19 the packaged probe was written to BOOT p10 after its ramdisk, DTBH and header digest were checked against the working image. Readback matched the candidate. The ordinary `reboot` command did not restart the shell-based Linux system; `reboot -f` did. The phone entered TWRP automatically. The TWRP `/proc/last_kmsg` capture contained `A5XNU001` with CurrentEL `0x4`, SCTLR before `0x30c5183e`, FDT `0x48000000`, Image `0x40205000`, `ID_AA64MMFR0_EL1=0x1122`, result `4`, TCR after `0x80a03520`, and SCTLR after `0x30c5183f`. This is a **live 4 KiB MMU result**, not XNU execution. The captured `CNTFRQ_EL0` was zero; the Exynos timer path needs separate attention. At the user's request the Linux BOOT image was not restored, and the phone remains in TWRP for XNU work. No USERDATA, EFS or recovery partition was written.

`xnu/patches/0082-a5xelte-4k-entry.patch` is a first XNU entry variant: it removes the donor's 16 KiB page selection, limits initial SMP to one CPU, skips the donor framebuffer writes and places an XNU-owned physical entry marker in reserved log RAM. `0083` accommodates Xcode 27's generated `IORPC.kernelContent` references, and `0084` selects the 36-bit device user VA limit so the 4 KiB commpage nesting assertion holds. The donor GICv3, timer, UFS and framebuffer paths are still incompatible with Exynos7580; the marker is only an entry checkpoint.

`shim/a5xelte/build-xnu-boot.sh` builds a separate 6 KiB Samsung Image loader. It reads the bootloader's `/chosen/linux,initrd-*` payload range, checks the `SWEETXNU` container and an isolated 512 MiB RAM window at `0x80000000`, copies Mach-O segments and the ADT, creates 4 KiB-aligned boot arguments, and jumps to the XNU physical entry. Failures request TWRP; the successful jump leaves a reserved-RAM marker for later recovery. The loader compiled with the VM's ARMv8-A cross toolchain and has no ELF relocations. Its first device run reached the XNU entry marker; the evidence is recorded below.

`tools/mkadt_a5xelte.py` generates an entry-only ADT with the Exynos7580 GICv2 node and no storage node. `tools/a5xelte-pack-probe.py --ramdisk` packages an XNU payload in a Samsung BOOT image while keeping the reference DTBH. The donor 16 KiB XNU cannot execute its MMU setup on this CPU and **must not be flashed**.

## Local XNU build and first BOOT attempt (2026-10-19)

Xcode 27.1 on macOS 27.2 built `kernel.development.sweet` with the a5xelte 4 KiB patch set. The 13.5 KDK package payload was expanded into a temporary directory and passed as `KDKROOT`, avoiding a system-wide installation. A local `blacktop/darwin-xnu-build` checkout at `663906528d1cd5ea8fde7de6f16de73db9511549` performed the build. The resulting arm64 Mach-O was 19 MB, SHA-256 `f093a6bc0d8f9c060f6dc4429810f768e00ec3f093ca624f1cd2694bd9a9b444`. The 4 KiB path compiled past `pmap_insert_commpage_internal`'s page-shift assertion. This is build evidence, not device execution evidence.

The entry-only ADT was 3,356 bytes. `mkpayload.py` produced a 19,940,288-byte `SWEETXNU` container; the Samsung BOOT candidate with the 6 KiB loader was 19,990,544 bytes, SHA-256 `8dff262ad0934b71396900eefef2ce34b681f0574e5e37ba065fc7482cff5921`. The candidate was written to BOOT p10 from TWRP, and p10 readback matched the candidate hash. TWRP `adb reboot` started the test. The user observed a persistent Samsung logo, then entered TWRP with the keys. Its `/proc/last_kmsg` preserved both `A5XNU002` at offset `0x7cde2` and the XNU-owned `XNUELTE1` at `0x7cee2`, exactly `0x100` bytes apart as the loader and XNU marker addresses specify. Loader slots showed EL1 (`0x4`), FDT `0x48000000`, Image `0x40205000`, state `5` (ready to branch), payload `0x42000000..0x433043c0`, isolated RAM `0x80000000..0xa0000000`, entry PA `0x81423488`, ten Mach-O segments, and boot args PA `0x827f5000`. **This proves execution reached XNU's `start_first_cpu` entry marker on the phone.** It does not prove the MMU transition, interrupt controller, timer, root filesystem or iOS userland. USERDATA, recovery and EFS were not written.

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

1. A5xelte shim: the raw EL1 granule result and live 4 KiB MMU transition are complete. Build a Samsung payload loader for the 4 KiB XNU variant. Do not attempt the donor's 16 KiB MMU setup. Compile for baseline ARMv8-A.
2. A5xelte XNU machine config: GICv2 interrupt acknowledgement, timer PPI and one CPU. Then test PSCI secondaries.
3. Recoverable panic log, eMMC read-only and root-device identification.
4. Exynos display, touch and keys using downstream Linux and Lineage source as hardware references.
5. iOS 16 root/userland only after memory and storage sizing: this board has 2 GiB RAM and about 11.5 GiB USERDATA, so the donor's 16 GiB root-image command is unsuitable as written.

Build output, kernel observations and user-verified screen/input behavior must be recorded separately. The only partition operation in this branch's live test was the explicitly authorized diagnostic BOOT p10 write.

## Console boot argument for the next C-stage test

The current early assembly renderer prints fixed XNU-owned strings before C initialization. The donor's DEVELOPMENT `kprintf` path is disabled by default, even with `debug=0xc`; the donor notes require `-enable_kprintf_spam` once `serial_init` can run. For a future A5XELTE payload, pass an explicit command line to `tools/mkpayload.py`:

```sh
--cmdline 'debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress'
```

This flag does not fix a failure before `arm_init` or make the fixed assembly strings into dynamic `kprintf` output. The A5XELTE framebuffer serial node is `a5xelte-fbcon` at physical `0x67000000`; it must be verified on the phone after C startup. Do not use the donor's `rd=ufsroot0`, `sweetfb_*`, UFS or USERDATA arguments on this device.
