#!/bin/bash
# Apply the donor (SWEET) iOS-root tweaks to the jackpotlte iOS root image.
#
#   sudo tools/jackpotlte/apply-root-tweaks.sh [IMAGE]
#
# IMAGE defaults to /var/tmp/iosroot/iOSRoot.img (the raw master; the sparse
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
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
img=${1:-/var/tmp/iosroot/IOSRoot.img}

[ -f "$img" ] || { echo "error: no image at $img" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || { echo "error: run as root (sudo $0 $img)" >&2; exit 1; }

echo "--- fsck before (repairs the header Linux objects to) ---"
fsck.hfsplus -fy "$img" || true

mnt=$(mktemp -d /tmp/iosroot-XXXXXX)
cleanup() {
	mountpoint -q "$mnt" && umount "$mnt" || true
	rmdir "$mnt" 2>/dev/null || true
}
trap cleanup EXIT

echo "--- mount read-write ---"
mount -t hfsplus -o loop,rw,force "$img" "$mnt"

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
echo "OK: tweaks applied to $img"
echo
echo "Next (host): free the old sparse copy first, there is not room for two."
echo "  rm /var/tmp/iosroot/IOSRoot.sparse.img"
echo "  img2simg $img /var/tmp/iosroot/IOSRoot.sparse.img"
echo "Then, with the phone in fastboot:"
echo "  fastboot flash USERDATA /var/tmp/iosroot/IOSRoot.sparse.img"
