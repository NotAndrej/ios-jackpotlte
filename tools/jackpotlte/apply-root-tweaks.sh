#!/bin/bash
# SPDX-License-Identifier: MIT
# Apply the SWEET iOS-root tweaks *and* the jackpotlte app fix to the iOS
# root image.
#
#   pkexec tools/jackpotlte/apply-root-tweaks.sh [IMAGE]
#
# IMAGE defaults to /var/tmp/IOSRoot.img (the raw master; the sparse copy
# flashed to USERDATA is derived from it). The script is idempotent: the
# tweaks are set-style and an app already in /Applications is left alone.
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
# What gets written:
#   (via tools/sweet-root-tweaks.py)
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
#   (then, by this script, after the Python step)
#   /Applications              the 32 bundles from
#                              /private/var/staged_system_apps moved in
#   /usr/local/bin/maliprobe   the userspace GPU probe, and
#   /System/Library/LaunchDaemons/com.jackpotlte.maliprobe.plist
#                              to run it once at boot (userspace/README.md).
#                              Skipped when the built copy is absent: it is
#                              arm64 iOS, so it comes from the userspace CI
#                              job, not from this host.
#   /usr/local/lib/libjackpotlte-gl.dylib + libOSMesa.8.dylib
#                              the EAGL shim and the Mesa it sits on, for the
#                              kernel's exec-time injection into backboardd
#                              (0156; docs/jackpotlte-mali.md sec. 9).  Same
#                              CI-origin note as the probe.
#
# Why the app move: SpringBoard showed only ~7 icons because iOS 16 ships the
# 32 user apps (Maps, Music, Weather, Clock, Calculator, Files, Books, Notes,
# Mail, ...) in /private/var/staged_system_apps and installs them onto the App
# cryptex on the first boot (SystemAppMigrator). This root is one plain HFS+
# volume and its App cryptex is a 10 MB skeleton (the IPSW's `Cryptex1,AppOS`)
# that holds no apps, so the migration installs nothing and LaunchServices
# never registers them. /Applications is the directory it already scans for the
# icons that work. Same volume, so the move is a rename and costs no space.
# tools/mkiosroot.sh does the same during a from-scratch build.
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
img=${1:-/var/tmp/IOSRoot.img}
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

# Present the apps iOS 16 leaves staged for the App cryptex (see the header).
if [ -d "$mnt/private/var/staged_system_apps" ]; then
	promoted=0
	for app in "$mnt/private/var/staged_system_apps"/*.app; do
		[ -e "$app" ] || continue
		[ -e "$mnt/Applications/${app##*/}" ] && continue
		mv "$app" "$mnt/Applications/"
		promoted=$((promoted + 1))
	done
	echo "promoted $promoted staged system apps into /Applications"
else
	echo "warning: no /private/var/staged_system_apps in $imgabs" >&2
fi

# The phone's userspace tool: the probe and the LaunchDaemon that runs it once
# at boot.  It reports to /dev/console only on a GPU boot, so it is quiet on an
# ordinary one (see userspace/README.md).
probe=${MALIPROBE_OUT:-$repo/userspace/out}
if [ -f "$probe/maliprobe" ]; then
	install -d -m 0755 "$mnt/usr/local/bin"
	install -m 0755 "$probe/maliprobe" "$mnt/usr/local/bin/maliprobe"
	install -m 0644 "$repo/userspace/jackpotlte-maliprobe/com.jackpotlte.maliprobe.plist" \
	    "$mnt/System/Library/LaunchDaemons/com.jackpotlte.maliprobe.plist"
	echo "installed maliprobe + its LaunchDaemon"
else
	echo "note: no $probe/maliprobe (the userspace CI job builds it); skipping the probe"
fi

# The GLES shim and the OSMesa it sits on, for the kernel's exec-time injection
# (0156; docs/jackpotlte-mali.md sec. 9).  They share one directory: the shim
# opens Mesa by @loader_path first, so /usr/local/lib holding both is enough and
# the injection names the shim by absolute path (boot-arg jtexecinject).  Both
# are arm64 iOS dylibs, so like the probe they come from CI, not this host.
shim=${GL_OUT:-$repo/userspace/out}
if [ -f "$shim/libjackpotlte-gl.dylib" ] && [ -f "$shim/libOSMesa.8.dylib" ]; then
	install -d -m 0755 "$mnt/usr/local/lib"
	install -m 0755 "$shim/libjackpotlte-gl.dylib" "$shim/libOSMesa.8.dylib" \
	    "$mnt/usr/local/lib/"
	echo "installed libjackpotlte-gl.dylib + libOSMesa.8.dylib into /usr/local/lib"
else
	echo "note: no GLES shim/Mesa in $shim (both from CI); skipping the loader payload"
fi

umount "$mnt"
echo "--- fsck after ---"
fsck.hfsplus -fy "$img" || true

echo
echo "OK: tweaks + app fix applied to $imgabs"
echo
sparse=${imgabs%.img}.sparse.img
echo "Next (host): free the old sparse copy first, there is not room for two."
echo "  rm -f $sparse"
echo "  img2simg $imgabs $sparse"
echo "Then, with the phone in fastboot:"
echo "  fastboot flash USERDATA $sparse"
