#!/bin/bash
# SPDX-License-Identifier: MIT
# Install the built jackpotlte userspace into the iOS root image.
#
#   sudo tools/jackpotlte/install-userspace.sh [IMAGE] [OUT]
#   (pkexec works the same way; the script itself needs root only to mount)
#
# IMAGE defaults to /var/tmp/IOSRoot.img, the raw master the sparse image
# flashed to USERDATA is derived from; OUT defaults to userspace/out, the
# directory CI's `userspace` artifact unpacks to (or userspace/build.sh's
# output).  Nothing is installed that is not there, and every file is read
# back and hashed after the copy, so a half-written image is not left to be
# flashed.
#
# Why a mount: the phone's iOS root lives inside USERDATA as HFS+, and the two
# libraries that make the GL path are files in it (/usr/local/lib).  The
# kernel's hfsplus driver is the only writer available here (7z reads HFS+ and
# hfsutils writes only classic HFS), and mounting needs root.  fsck.hfsplus
# then re-reads the whole volume, which is what catches a metadata write the
# driver got wrong before a 12 GB sparse upload spends it on the phone.
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
image=${1:-/var/tmp/IOSRoot.img}
out=${2:-$repo/userspace/out}

[ "$(id -u)" = 0 ] || { echo "error: needs root to mount $image (sudo or pkexec)" >&2; exit 1; }

files=(
	libjackpotlte-gl.dylib
	libOSMesa.8.dylib
)

for name in "${files[@]}"; do
	[ -f "$out/$name" ] || { echo "error: no $out/$name" >&2; exit 1; }
done

before=$(sha256sum "$image" | cut -d' ' -f1)
echo "image  $image"
echo "before $before"

mountpoint=$(mktemp -d /tmp/jackpotlte-root.XXXXXX)
cleanup() {
	umount "$mountpoint" 2>/dev/null || true
	rmdir "$mountpoint" 2>/dev/null || true
}
trap cleanup EXIT

mount -t hfsplus -o loop,rw "$image" "$mountpoint"
echo "mounted $mountpoint"

install -d -m 0755 "$mountpoint/usr/local/lib"
for name in "${files[@]}"; do
	echo "install $name ($(stat -c %s "$out/$name") bytes)"
	install -m 0755 "$out/$name" "$mountpoint/usr/local/lib/$name"
done
sync

# Read every one back from the mounted volume, not from the copy: this is the
# bytes the phone's kernel will read.
status=0
for name in "${files[@]}"; do
	want=$(sha256sum "$out/$name" | cut -d' ' -f1)
	got=$(sha256sum "$mountpoint/usr/local/lib/$name" | cut -d' ' -f1)
	if [ "$want" = "$got" ]; then
		echo "ok    $name $got"
	else
		echo "BROKEN $name: wrote $want, read $got" >&2
		status=1
	fi
done

umount "$mountpoint"
rmdir "$mountpoint"
trap - EXIT

after=$(sha256sum "$image" | cut -d' ' -f1)
echo "after  $after"
[ "$before" != "$after" ] || { echo "error: the image did not change" >&2; exit 1; }

echo "--- fsck.hfsplus (read-only) ---"
if fsck.hfsplus -fn "$image"; then
	echo "fsck: clean"
else
	echo "error: fsck does not like the image; do not flash it" >&2
	status=1
fi

echo
echo "next: img2simg $image /var/tmp/IOSRoot.sparse.img"
echo "      tools/jackpotlte/verify-sparse.py $image /var/tmp/IOSRoot.sparse.img"
echo "      fastboot flash USERDATA /var/tmp/IOSRoot.sparse.img"
exit $status