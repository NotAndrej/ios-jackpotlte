#!/bin/bash
# SPDX-License-Identifier: MIT
# Put the system apps iOS 16 keeps in /private/var/staged_system_apps on the
# SpringBoard home screen, by moving them into /Applications.
#
#   pkexec tools/jackpotlte/promote-staged-apps.sh [IMAGE]
#
# IMAGE defaults to /var/tmp/IOSRoot.img (the raw master; the sparse copy
# flashed to USERDATA is derived from it).
#
# Why only a handful of icons show: the iPhone10,3 16.7.16 (20H392) root made
# by tools/mkiosroot.sh holds two kinds of bundle.  /Applications has 142 of
# them, but almost all are XPC/UI services with no home-screen icon; the real
# ones - MobileSafari, MobileSMS, MobilePhone, MobileSlideShow, Preferences,
# Camera, AppStore - are the ~7 icons that appear.  The 32 user apps that
# belong on the home screen (Maps, Music, Weather, Clock, Calculator, Files,
# Books, Notes, Mail, Reminders, ...) sit in /private/var/staged_system_apps.
#
# They do not stay there on a device: the first-boot migration
# (SystemAppMigrator / cryptexd) installs each staged bundle onto the App
# cryptex, mounted at /System/Cryptexes/App, and LaunchServices registers them
# from there.  Our root is one plain HFS+ volume.  Its App cryptex is only a
# 10 MB skeleton (the IPSW's `Cryptex1,AppOS`, 087-86715-047.dmg - it never
# contains the apps), the migration installs nothing, and so those 32 bundles
# are never registered and never get an icon.  /System/Library/AppPlaceholders
# even ships a placeholder icon + Info.plist per app, which is what the system
# shows while an app is still installing.
#
# Fix: move each staged bundle to /Applications, the directory LaunchServices
# scans for the icons that already work.  It is a same-volume rename, so it
# costs no space.  The bundles are complete (each carries its Mach-O
# executable: Maps 18 MB, Books 21 MB, Weather 10 MB, ...) and their shared
# frameworks live in the shared cache this image already hard-links into
# /System/Library/Caches/com.apple.dyld, so no other path has to change.
#
# The volume is journaled.  Linux refuses it read-write until fsck.hfsplus has
# repaired the volume header, hence `-o force`; and it must be unmounted
# cleanly or XNU replays the journal on first boot.
#
# Host quirk (2026-11-08): the ext4 directory index for this image
# intermittently returns ENOENT for a fresh absolute lookup, so work from
# inside the image's directory with a relative name and prime the dentry with
# a readdir.  See tools/jackpotlte/apply-root-tweaks.sh.
set -euo pipefail

img=${1:-/var/tmp/IOSRoot.img}
imgabs=$img

[ "$(id -u)" -eq 0 ] || { echo "error: run as root (sudo/pkexec $0 $img)" >&2; exit 1; }
[ -d "$(dirname "$img")" ] || { echo "error: no directory for $img" >&2; exit 1; }

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
echo "image: $imgabs ($(stat -c %s "$img") bytes)"

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

staged=$mnt/private/var/staged_system_apps
apps=$mnt/Applications
[ -d "$staged" ] || { echo "error: no $staged in the image" >&2; exit 1; }
[ -d "$apps" ] || { echo "error: no $apps in the image" >&2; exit 1; }

before=$(find "$apps" -maxdepth 1 -name '*.app' -type d | wc -l)
moved=0
skipped=0
shopt -s nullglob
for app in "$staged"/*.app; do
	name=$(basename "$app")
	if [ -e "$apps/$name" ]; then
		echo "  keep   $name (already in /Applications)"
		skipped=$((skipped + 1))
		continue
	fi
	if ! mv "$app" "$apps/$name"; then
		echo "error: cannot move $name to /Applications" >&2
		exit 1
	fi
	echo "  moved  $name"
	moved=$((moved + 1))
done
shopt -u nullglob

sync
after=$(find "$apps" -maxdepth 1 -name '*.app' -type d | wc -l)
left=$(find "$staged" -maxdepth 1 -name '*.app' -type d | wc -l)
echo "--- /Applications: $before -> $after .app ($moved moved, $skipped kept); staged now $left ---"

umount "$mnt"
echo "--- fsck after ---"
fsck.hfsplus -fy "$img" || true

echo
echo "OK: staged apps promoted in $imgabs"
echo
echo "Next (host): free the old sparse copy first, there is not room for two."
echo "  rm /var/tmp/IOSRoot.sparse.img"
echo "  img2simg $imgabs /var/tmp/IOSRoot.sparse.img"
echo "Then, with the phone in fastboot (USERDATA is the iOS root - destructive):"
echo "  fastboot flash USERDATA /var/tmp/IOSRoot.sparse.img"
