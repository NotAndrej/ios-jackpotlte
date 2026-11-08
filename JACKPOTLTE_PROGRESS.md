# Galaxy A8 2018 (SM-A530F, jackpotlte) port — progress

Branch: this repository ports the Galaxy S8 (`dreamlte`, Exynos 8895) iOS work
to the **Galaxy A8 2018 (SM-A530F, `jackpotlte`, Exynos 7885)**. The S8 tree is
the base; we adapt it, we do not rewrite it.

Observation is kept strictly apart from plan, and build success is never a boot
claim. **A boot stage counts as reached only with a photo, serial log or screen
output from the phone.**

## Stage status

| Stage | Description | Status |
| --- | --- | --- |
| 1 | Repo skeleton, S8 port imported, builds reproducibly | **in progress (built)** |
| 2 | Loader runs on the A8 and prints the banner | **reached (screen output, 2026-11-08)** |
| 3 | Payload passes its check; jump to XNU | **in progress (payload packed + flashed)** |
| 4 | XNU prints to the framebuffer (log or panic) | not started |
| 5 | Storage (eMMC) detected | not started |
| 6 | Root filesystem mounts | not started |
| 7 | launchd, then SpringBoard | not started |

### Stage 2 evidence

- Image `/home/not_andrej/Downloads/boot-jackpotlte-shim.img`, 933,904 B,
  sha256 `a651f02badf6cf24f68bc22077aaf396a352c26f05d43167e5fb38c8ff4ad627`,
  shim kernel 46,144 B, empty ramdisk, packed on `boot_a530f_q.img`.
- Owner saw **`A8LOADER BEGIN  (jackpotlte Exynos XNU loader)`** on the phone
  after `fastboot flash BOOT` + reboot. This is the S8 `shim/dreamlte` loader
  with only: FB base `0xec000000`, 1080x2220, `DECON_TRIG 0x14860070 = 0x1281`,
  log zone at `0xc0000000`, and the probe calls removed.

## Hardware facts (from Samsung's own 4.4 source and the mainline DT; NOT yet confirmed on the phone)

Source now includes the **Eureka-Kernel** Samsung 4.4 tree for this device
(`eurekadevelopment/Eureka-Kernel`, tag "Release R24U sources",
`arch/arm64/boot/dts/exynos/dtb/exynos7885*.dts[i]`, `drivers/`). See
`shim/jackpotlte/board.h` for the full list with a citation per value.

- RAM: low `0x80000000 + 0x3da00000`, mid `0xc0000000 + 0x40000000`, high
  `0x880000000 + 0x40000000`. S-Boot's own `/chosen` sets
  `linux,initrd = 0x84000000..0x840fffff`.
- CPU: 6x Cortex-A53 (MPIDR `0x100..0x103`, `0x200..0x201`), 2x Cortex-A73
  (`0x0..0x1`). Boot core expected at MPIDR `0x100` (unconfirmed).
- GIC-400: GICD `0x12301000`, GICC `0x12302000`, GICH `0x12304000`,
  GICV `0x12306000`.
- Timer: arch timer + 26 MHz; non-secure phys timer PPI 14 (INTID 30).
- PMU: `0x11c80000`. Secondary-core power: `PMU_CPU_CONFIG_BASE 0x2000`,
  status `0x2004`, step `0x80`, `CPU_LOCAL_PWR_CFG 0xF`; clusters 0/1/2 map to
  core slots 0/4/6 (Exynos7885-specific numbering in `exynos-pmu.c`).
- Storage: **eMMC** `samsung,exynos-dw-mshc` `dwmmc0@13500000` (+ cmdq
  `0x13508000`); the S8 uses UFS, so storage adapts from the A5XELTE eMMC
  driver, not the S8's UFS.
- Display: bootloader framebuffer **`0xec000000`**, 20 MiB
  (`fb_handover`/`exynos,dpu_fb_handover`). Panel **s6e3fa7, 1080x2220**, MIPI
  command mode. **DECON_F @ `0x14860000`**, `HW_SW_TRIG_CONTROL` at `+0x70`.
  **Confirmed on the phone (2026-11-08):** the loader must write `0x1281` to
  `0x14860070` before touching the framebuffer. Without it DECON never latches
  a shadow update, no frames are scanned, and the panel keeps showing the
  S-Boot logo out of its own GRAM (the "stuck on Samsung Galaxy" symptom).
  The loader's earlier shadow-update pokes (`+0x60`) are what broke scanout;
  the `+0x70` trigger is the one that works.
- Buttons: `gpa1` (pinctrl alive `0x11cb0000`) — vol-down p6, vol-up p5,
  power p7 (active low). Bank offset still to read from the driver.

## Unknowns (do NOT guess; read from the named source)

| Item | Read from |
| --- | --- |
| ramoops/pstore zone (exact offset of the "log_pstore" item) | Eureka `drivers/trace/exynos-ss.c` ESS layout, or live DT / TWRP `/proc/iomem` |
| gpa1 bank offset inside pinctrl_0 | Eureka pinctrl bank table, or live DT |
| eMMC CIU timing / card setup | Eureka `samsung,exynos-dw-mshc` driver |
| Stock boot.img header (kaddr/raddr/tags/os_version, DTBH) | the SM-A530F's own BOOT dump |
| EL at handoff, x0 register, load addresses | loader banner + a probe image on the phone |

## Build results (local, not device)

- `shim/jackpotlte/build.sh` → `out/Image` = **48,384 bytes**, 0 ABS64
  relocations, above the 16 KiB stub gate. Reproducible on this machine.
- `shim/jackpotlte/build.sh --probe STOCK OUT` → shim-only flash image
  (48,384-byte loader + a minimal newc cpio.gz ramdisk); 61,456 bytes with a
  synthetic stock header. This is the stage-2 bring-up image.
- `tools/mkadt_jackpotlte.py` → ADT = 8,920 bytes (without the iPhone X
  `/product` node, which is an Apple artifact kept out of git).
- `tools/jackpotlte/selftest.py` → the full pack chain (loader → ADT → payload
  → Samsung pack → bootimg_info) passes on a synthetic Mach-O; the packed
  header keeps the stock load addresses and the SEANDROIDENFORCE trailer.
  This proves the pipeline is internally consistent, not that a kernel boots.
- Packer round-trip tested with a synthetic stock image only, not a real
  SM-A530F BOOT dump. Real header values still required (see above).

## CI status

- `loader` (Linux): build + gates + ADT + pipeline self-test + repo validation.
  The ABS64 gate caught a real defect the local GCC hid: a `const char *names[]`
  string-literal table in `dt_probe` became 12 absolute relocations under CI's
  GCC (locally it folded to PC-relative). Replaced with a `node_line()` helper;
  run 42413636860 is green.
- `xnu` (macOS 15): builds `MACHINE_CONFIG=A5XELTE` with the A5 patch set +
  `0124-dreamlte-s8`, newest installed Xcode (patch `0083` supplies the
  `IORPC.kernelContent` field that Xcode 16+ `iig` emits), and now fails on a
  stub kernel. First runs: macos-14 was rejected by the runner; fixed to
  macos-15. Run 42412221459 (success) produced `out/kernel.development.a5xelte`
  = 16,553,272 bytes, retrievable as the `xnu-kernel` artifact (66 MB, not
  expired) and kept as the `xnu-buildtree` artifact.

## Device confirmation

The owner confirmed the device is **SM-A530F (Exynos 7885)**, not the
Snapdragon A530W/U. The mainline `exynos7885*` values above therefore apply.

## Next concrete action

1. Get `backup/BOOT.img` (the SM-A530F's own BOOT dump) from the phone:
   `adb shell "dd if=/dev/block/by-name/BOOT of=/sdcard/BOOT.img"` then
   `adb pull /sdcard/BOOT.img backup/BOOT.img`.
2. Build and flash the shim-only probe image and photograph the screen:
   `bash shim/jackpotlte/build.sh --probe backup/BOOT.img a8-probe.img`.
   That reaches stage 2 and returns the live bootargs, initrd range, /memory
   and the 7885 node presence — record them in `docs/jackpotlte-bringup.md`.
3. Read the remaining unknowns (ramoops `log_pstore` offset, gpa1 bank offset,
   eMMC timings) from the live DT / Samsung source and fill `board.h`; then
   enable `JT_HAVE_RAMOOPS`.
