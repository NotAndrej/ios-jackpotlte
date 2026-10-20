#!/bin/bash
# Kernel Mach-O -> Galaxy S8 BOOT image.
#   make-boot.sh KERNEL STOCK_BOOT.img OUT.img [boot-args...]
# Default boot-args: one CPU, verbose console, no watchdog; no root device
# yet, so the first trials end in a root-mount panic by design.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
kernel=$(realpath "$1"); stock=$(realpath "$2"); out=$(realpath -m "$3")
shift 3
args=${*:-debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress wdt=-1 wdt_test=no_user_checkin}
work=$repo/shim/dreamlte/out
mkdir -p "$work"
python3 "$repo/tools/mkadt_dreamlte.py" "$work/dreamlte.adt"
python3 "$repo/tools/mkpayload.py" "$kernel" "$work/dreamlte.adt" "$work/payload.bin" --cmdline "$args"
bash "$repo/shim/dreamlte/build.sh" "$stock" "$work/payload.bin" "$out"
python3 "$repo/tools/dreamlte_bootimg_info.py" "$out" | head -4
