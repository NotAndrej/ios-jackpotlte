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
| 2 | Loader runs on the A8 and prints the banner | not started |
| 3 | Payload passes its check; jump to XNU | not started |
| 4 | XNU prints to the framebuffer (log or panic) | not started |
| 5 | Storage (eMMC) detected | not started |
| 6 | Root filesystem mounts | not started |
| 7 | launchd, then SpringBoard | not started |

## Hardware facts (mainline device tree; NOT yet confirmed on the phone)

Source: `arch/arm64/boot/dts/exynos/exynos7885.dtsi` and
`exynos7885-jackpotlte.dts`. See `shim/jackpotlte/board.h` for the full list.

- RAM: low `0x80000000 + 0x3da00000`, mid `0xc0000000 + 0x40000000`,
  high `0x880000000 + 0x80000000`.
- CPU: 6x Cortex-A53 (MPIDR `0x100..0x103`, `0x200..0x201`), 2x Cortex-A73
  (`0x0..0x1`). Boot core expected at MPIDR `0x100` (unconfirmed).
- GIC-400: GICD `0x12301000`, GICC `0x12302000`, GICH `0x12304000`,
  GICV `0x12306000`.
- Timer: arch timer + 26 MHz oscclk; non-secure phys timer = PPI 14 (INTID 30).
- PMU: `0x11c80000`.
- Storage: **eMMC** `samsung,exynos7885-dw-mshc-smu` @ `0x13500000`
  (the S8 uses UFS; the A5XELTE port's `bsd/dev/a5xelte_emmc.c` is the base).
- Buttons: GP1 on `pinctrl_alive` `0x11cb0000` — vol-up p5, vol-down p6,
  power p7 (active low).
- Panel: 1080x2220 class (not yet confirmed).

## Unknowns (do NOT guess; read from the named source)

| Item | Read from |
| --- | --- |
| Framebuffer base + geometry (`s3cfb.bootloaderfb` or reserved-memory node) | live DT (`/sys/firmware/fdt` in TWRP) |
| DECON base + trigger register/mask | Samsung 7885 kernel source |
| PMU secondary-core power-up sequence | Samsung 7885 kernel (`drivers/soc/samsung/exynos-pmu.c`) |
| ramoops/pstore zones (log channel) | live DT ramoops node, TWRP `/proc/iomem` |
| eMMC CIU timing / card setup | Samsung 7885 kernel, mainline `dw-mshc` timings |
| Stock boot.img header (kaddr/raddr/tags/os_version, DTBH) | the SM-A530F's own BOOT dump |
| EL at handoff, x0 register, load addresses | loader banner + a probe image on the phone |

## Build results (local, not device)

- `shim/jackpotlte/build.sh` → `out/Image` = **41,536 bytes**, 0 ABS64
  relocations, above the 16 KiB stub gate. Reproducible on this machine.
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
- `xnu` (macOS 15): builds `MACHINE_CONFIG=A5XELTE` with the A5 patch set +
  `0124-dreamlte-s8`, newest installed Xcode (patch `0083` supplies the
  `IORPC.kernelContent` field that Xcode 16+ `iig` emits), and now fails on a
  stub kernel. First runs: macos-14 was rejected by the runner; fixed to
  macos-15.

## Device confirmation

The owner confirmed the device is **SM-A530F (Exynos 7885)**, not the
Snapdragon A530W/U. The mainline `exynos7885*` values above therefore apply.

## Next concrete action

1. Read the unknowns above from the phone (TWRP) and Samsung source; record
   them in `docs/jackpotlte-bringup.md`.
2. Fill `shim/jackpotlte/board.h`: enable `JT_HAVE_FB` and `JT_HAVE_RAMOOPS`
   with the read-back values so stage 2 can print a banner.
