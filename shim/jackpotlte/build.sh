#!/bin/bash
# Build the Galaxy A8 2018 (SM-A530F, jackpotlte) XNU loader (Samsung S-Boot
# "kernel" Image). Optionally pack a BOOT image:
#   build.sh STOCK_BOOT.img PAYLOAD.bin OUT.img   full payload
#   build.sh --probe STOCK_BOOT.img OUT.img       shim only (banner/probe)
# (PAYLOAD from tools/mkpayload.py; STOCK_BOOT supplies header and DTBH.)
#
# Hard gates (no continue-on-error): the loader must build, carry no absolute
# relocations, and be at least MIN_IMAGE bytes. A stub is a build failure.
set -euo pipefail
cd "$(dirname "$0")"

CC=aarch64-linux-gnu-gcc
OBJCOPY=aarch64-linux-gnu-objcopy
READELF=aarch64-linux-gnu-readelf
NM=aarch64-linux-gnu-nm
CFLAGS="-O2 -ffreestanding -nostdlib -fno-pic -mgeneral-regs-only -mstrict-align -fno-stack-protector -fno-tree-loop-distribute-patterns -march=armv8-a -Wall -Wextra -Werror"

# The payload-copy/boot_args machinery plus the vector table and stack is the
# irreducible core. dreamlte's loader (more probes, plus mngs) is ~59 KiB; the
# jackpotlte one is smaller. Require at least 16 KiB so a stub cannot pass.
MIN_IMAGE=$((16 * 1024))
MIN_PAYLOAD=$((16 * 1024))

# Parse an optional leading --probe (shim only, empty ramdisk).
probe=0
if [ "${1:-}" = "--probe" ]; then
	probe=1
	shift
fi

mkdir -p out
$CC $CFLAGS -c start.S -o out/start.o
$CC $CFLAGS -c loader.c -o out/loader.o
$CC $CFLAGS -c ../fdt.c -o out/fdt.o

objs="out/loader.o out/fdt.o"

# Runs wherever S-Boot puts it, with no relocation pass: only the header's
# image_size (linked at 0) may be an absolute address.
abs=$($READELF -r $objs | grep -c ABS64 || true)
if [ "$abs" != 0 ]; then
	echo "error: $abs absolute relocations" >&2
	exit 1
fi

$CC $CFLAGS -static -T loader.ld -Wl,--build-id=none -Wl,--no-warn-rwx-segments -Wl,-N \
	out/start.o $objs -o out/loader.elf
$OBJCOPY -O binary out/loader.elf out/Image
size=$($NM out/loader.elf | awk '$3=="_end"{print strtonum("0x"$1)}')
truncate -s "$size" out/Image

img_size=$(stat -c %s out/Image)
echo "out/Image: $img_size bytes"
if [ "$img_size" -lt "$MIN_IMAGE" ]; then
	echo "error: out/Image is $img_size bytes, below the $MIN_IMAGE-byte minimum (stub?)" >&2
	exit 1
fi

if [ "$probe" = 1 ]; then
	# Shim-only image: an empty ramdisk makes S-Boot hand the loader a
	# ramdisk with no SWEETXNU payload; the loader prints its banner and the
	# live-DT probe, then stops. This is the stage-2 bring-up image.
	[ $# = 2 ] || { echo "usage: build.sh --probe STOCK_BOOT.img OUT.img" >&2; exit 1; }
	# A minimal valid newc cpio.gz (canonical empty archive), so S-Boot never
	# sees a zero-size ramdisk.
	python3 - out/empty.rd <<'PY'
import gzip, sys
empty = b'070701' + b'0' * 104 + b'TRAILER!!!\x00'
empty += b'\x00' * (-len(empty) % 4)
open(sys.argv[1], 'wb').write(gzip.compress(empty, 9))
PY
	python3 ../../tools/jackpotlte_mkboot.py "$1" out/Image out/empty.rd "$2"
	sha256sum "$2"
	exit 0
fi

if [ $# = 3 ]; then
	if [ ! -s "$2" ]; then
		echo "error: payload $2 missing or empty" >&2
		exit 1
	fi
	payload_size=$(stat -c %s "$2")
	if [ "$payload_size" -lt "$MIN_PAYLOAD" ]; then
		echo "error: payload $2 is $payload_size bytes, below the $MIN_PAYLOAD-byte minimum (stub?)" >&2
		exit 1
	fi
	python3 ../../tools/jackpotlte_mkboot.py "$1" out/Image "$2" "$3"
	sha256sum "$3"
fi
