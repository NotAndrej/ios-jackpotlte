# jackpotlte bring-up — readings to collect

This file records **observed** values from the phone and from Samsung's source.
Every `TODO` in `shim/jackpotlte/board.h` and `tools/mkadt_jackpotlte.py` points
here. Nothing goes into the code until it is recorded here first; nothing here
is a guess.

## How to collect each reading

### Stock BOOT image (required for the packer)
In TWRP:
```sh
adb shell "ls -l /dev/block/by-name/"
adb shell "dd if=/dev/block/by-name/BOOT of=/sdcard/BOOT.img"
adb pull /sdcard/BOOT.img backup/BOOT.img
```
Then locally:
```
python3 tools/jackpotlte_bootimg_info.py backup/BOOT.img
```
If the A8's BOOT is a different block, use `/dev/block/by-name/BOOT`.

Recorded:
- kernel addr / ramdisk addr / tags / page size / os_version / DTBH:
  _pending_

### Live device tree
```sh
adb shell "dd if=/sys/firmware/fdt of=/sdcard/fdt.dtb"
adb pull /sdcard/fdt.dtb
dtc -I dtb -O dts fdt.dtb -o docs/jackpotlte-live.dts
```
Then read: the framebuffer / reserved-memory nodes, `/chosen` bootargs
(`s3cfb.bootloaderfb`), the ramoops node, the `mmc@13500000` node, gpio-keys.

Recorded:
- framebuffer base / geometry: `0xec000000`, 20 MiB, 1080x2220, 32 bpp
  (stride 4320). Source: Samsung 4.4
  `arch/arm64/boot/dts/exynos/{dtb,dtbo}/exynos7884-rmem.dtsi` /
  `exynos7885-rmem.dtsi` `fb_handover: framebuffer@0xEC000000` with
  `compatible = "exynos,dpu_fb_handover"` and `reg = <0x0 0xEC000000 0x1400000>`;
  panel `s6e3fa7` 1080x2220 in `exynos7885-jackpotlte_common.dtsi`. Confirmed
  on the phone: the loader banner is visible at this address (stage 2/4,
  2026-11-08).
- DECON base / trigger: `0x14860000`, `HW_SW_TRIG_CONTROL` `+0x0070`,
  write **`0x1281`**. Source: `dpu_7885/regs-decon.h` (`HW_TRIG_EN` bit 0,
  `HW_TRIG_EDGE_POLARITY` bit 7, `TRIG_AUTO_MASK_EN` bit 12, `SW_TRIG_EN`
  bit 8 deliberately clear). Confirmed on the phone: with `0x1281` written by
  the S8 shim the panel showed the loader + XNU text; with no trigger the
  panel held the S-Boot frame. The same trigger is now in `shim/jackpotlte`
  (built, not yet flashed).
- ramoops base / size / console+pmsg offsets: _pending_
- eMMC node reg + phandle/timings: _pending_

### Registers at handoff
The loader banner prints EL, SCTLR and x0 (DTB). To learn whether S-Boot enters
at EL1 and where it placed the Image, we need the banner on screen (stage 2) or
a TWRP readback of the ramoops text.

Recorded:
- EL at handoff: _pending_
- x0 (DTB address): _pending_
- S-Boot Image load address: _pending_

### Samsung 7885 open-source kernel
Needed for: DECON register map, PMU secondary-core power-up, eMMC CIU timing,
`s3cfb,bootloaderfb`. Record the exact source tag and the file/line for each
value used.

Recorded:
- source tag / URL: `eurekadevelopment/Eureka-Kernel`, "Release R24U sources"
  (Linux 4.4.302). DTS: `arch/arm64/boot/dts/exynos/dtb/exynos7885*.dts[i]`.

## Stage 2 flash procedure (once the BOOT dump is in place)

Everything below runs against `backup/BOOT.img` (the SM-A530F's own BOOT dump).

### 2a. Probe image (shim only — the first flash)

No kernel is needed. The loader prints its banner and the live-device-tree dump,
then halts cleanly ("probe-only run") because the ramdisk carries no payload.

```sh
bash shim/jackpotlte/build.sh --probe backup/BOOT.img a8-probe.img
python3 tools/jackpotlte_bootimg_info.py a8-probe.img   # sanity: addresses kept
```

### 2b. Full image (once a payload exists)

```sh
python3 tools/mkadt_jackpotlte.py shim/jackpotlte/out/jackpotlte.adt
python3 tools/mkpayload.py <kernel> shim/jackpotlte/out/jackpotlte.adt \
    shim/jackpotlte/out/payload.bin --cmdline "debug=0xc serial=3 cpus=1"
bash shim/jackpotlte/build.sh backup/BOOT.img shim/jackpotlte/out/payload.bin \
    a8-boot.img
python3 tools/jackpotlte_bootimg_info.py a8-boot.img   # sanity: addresses kept
```

### 2c. Flash BOOT only (in TWRP). Confirm the node first:

```sh
adb shell "ls -l /dev/block/by-name/BOOT"
adb push a8-probe.img /tmp/boot.img
adb shell "dd if=/tmp/boot.img of=/dev/block/by-name/BOOT bs=1048576 conv=fsync; sync"
adb reboot
```

Stage 2 is reached only when a **photo of the phone screen** shows the
`JACKPOTLTE LOADER BEGIN` banner (the loader draws to the S-Boot framebuffer at
`0xec000000`). The probe dump also tells us the live bootargs, initrd range,
`/memory` base/size and which 7885 nodes S-Boot actually leaves — record those
here. A build is not a boot.

