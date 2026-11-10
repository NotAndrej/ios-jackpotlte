#!/bin/bash
# SPDX-License-Identifier: MIT
# Apply the donor (SWEET) iOS-root tweaks to the jackpotlte iOS root image.
#
#   pkexec tools/jackpotlte/apply-root-tweaks.sh [IMAGE]
#
# IMAGE defaults to /var/tmp/iosroot/IOSRoot.img (the raw master; the sparse
# copy flashed to USERDATA is derived from it).
#
# Why this exists: tools/mkiosroot.sh builds the plain root, and the donor's
# tools/sweet-root-tweaks.py is what turned "plain mkiosroot output" into the
# images that reach the SpringBoard UI. The image flashed on 2026-11-08 was
# checked with `7z l`: it has NO /private/var/mobile/Library/Preferences/
# com.apple.migration.plist, no purplebuddy Setup flags, no mobileactivationd
# data ark, no containers/ dirs, and VirtualAudio still installed. That is the
# pre-tweak state, which is exactly the endless-DataMigration boot: the bar
# runs, and SpringBoard is held in -[BKSSystemShellService _checkIn] until
# backboardd's DMPerformMigrationIfNeeded returns.
#
# What gets written (all via tools/sweet-root-tweaks.py):
#   com.apple.migration.plist  LastSystemVersion=20H392 so DataMigration stops
#                              running on every boot (the proven 100% gate),
#                              + DMUserDataDisposition + DMLastMigrationResults
#   containers/                /private/var/containers/{Bundle,Data/System,
#                              Shared/SystemGroup,Temp}
#   purplebuddy                SetupDone / SetupFinishedAllSteps / SetupVersion,
#                              mini-buddy flags off
#   springboard                BKNoWatchdogs, no cloud-configuration UI
#   mobileactivationd          activation data ark (-ActivationState Activated)
#   launchd.plist              _PanicOnCrash -> _PanicOnCrasX, CommCenter jobs off
#   mediaserverd               VirtualAudio HAL plug-in disabled,
#                              StartRecorderServer=false
#
# The volume is journaled. Linux refuses it read-write until fsck.hfsplus has
# repaired the volume header, hence `-o force`; and it must be unmounted
# cleanly or XNU has to replay the journal on first boot.
#
# Host quirk (2026-11-08): the ext4 directory index for this image is damaged,
# so a fresh lookup of the *absolute* path intermittently returns ENOENT while
# readdir still lists the entry (the inode is fine: debugfs shows Links 1 and
# the full block list). Running from inside the directory with a relative name,
# and priming with a readdir of that directory before each attempt, makes it
# resolve. Hence the cd + retries below.
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
img=${1:-/var/tmp/iosroot/IOSRoot.img}
imgabs=$img

[ "$(id -u)" -eq 0 ] || { echo "error: run as root (sudo/pkexec $0 $img)" >&2; exit 1; }
[ -d "$(dirname "$img")" ] || { echo "error: no directory for $img" >&2; exit 1; }

# Work from inside the image's directory with a relative name (see the host
# quirk note above), and prime the dentry with a readdir each time.
imgdir=$(cd "$(dirname "$img")" && pwd)
imgbase=$(basename "$img")
cd "$imgdir"
img=./$imgbase

prime() { ls -la . >/dev/null 2>&1 || true; }

n=0
until [ -f "$img" ]; do
	prime
	n=$((n + 1))
	if [ "$n" -ge 300 ]; then
		echo "error: no image at $imgabs (after $n tries)" >&2
		exit 1
	fi
	sleep 1
done
echo "image: $imgabs ($(stat -c %s "$img" 2>/dev/null || echo '?') bytes)"

echo "--- fsck before (repairs the header Linux objects to) ---"
for n in $(seq 1 60); do
	prime
	if fsck.hfsplus -fy "$img"; then break; fi
	echo "  ... fsck attempt $n failed (transient lookup?), retrying"
	sleep 1
done

mnt=$(mktemp -d /tmp/iosroot-XXXXXX)
cleanup() {
	mountpoint -q "$mnt" && umount "$mnt" || true
	rmdir "$mnt" 2>/dev/null || true
}
trap cleanup EXIT

echo "--- mount read-write ---"
mounted=0
for n in $(seq 1 300); do
	prime
	if mount -t hfsplus -o loop,rw,force "$img" "$mnt" 2>/tmp/iosroot-mount.err; then
		mounted=1
		break
	fi
	[ $((n % 30)) -eq 0 ] && echo "  ... mount attempt $n still failing"
	sleep 1
done
if [ "$mounted" != 1 ]; then
	echo "error: cannot mount $imgabs after $n tries:" >&2
	cat /tmp/iosroot-mount.err >&2
	exit 1
fi

if ! touch "$mnt/.rwcheck" 2>/dev/null; then
	echo "error: $mnt is not writable (journal not repaired?)" >&2
	exit 1
fi
rm -f "$mnt/.rwcheck"

python3 "$repo/tools/sweet-root-tweaks.py" "$mnt"

umount "$mnt"
echo "--- fsck after ---"
fsck.hfsplus -fy "$img" || true

echo
echo "OK: tweaks applied to $imgabs"
echo
echo "Next (host): free the old sparse copy first, there is not room for two."
echo "  rm /var/tmp/iosroot/IOSRoot.sparse.img"
echo "  img2simg $imgabs /var/tmp/iosroot/IOSRoot.sparse.img"
echo "Then, with the phone in fastboot:"
echo "  fastboot flash USERDATA /var/tmp/iosroot/IOSRoot.sparse.img"
