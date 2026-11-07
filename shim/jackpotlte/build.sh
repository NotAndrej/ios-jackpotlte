#!/bin/bash
# Build the Galaxy A8 2018 (SM-A530F, jackpotlte) XNU loader (Samsung S-Boot
# "kernel" Image). Optionally pack a BOOT image:
#   build.sh STOCK_BOOT.img PAYLOAD.bin OUT.img
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

if [ $# = 3 ]; then
	if [ ! -s "$2" ]; then
		echo "error: payload $2 missing or empty" >&2
		exit 1
	fi
	payload_size=$(stat -c %s "$2")
	min_payload=$((16 * 1024))
	if [ "$payload_size" -lt "$min_payload" ]; then
		echo "error: payload $2 is $payload_size bytes, below the $min_payload-byte minimum (stub?)" >&2
		exit 1
	fi
	python3 ../../tools/jackpotlte_mkboot.py "$1" out/Image "$2" "$3"
	sha256sum "$3"
fi
