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
- framebuffer base / geometry: _pending_
- DECON base / trigger: _pending_
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
- source tag / URL: _pending_
