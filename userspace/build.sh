#!/bin/bash
# SPDX-License-Identifier: MIT
# Build the jackpotlte userspace.  Needs macOS: the phone's payloads are arm64
# iOS, which come from the iPhoneOS SDK, and the ABI self-test is a Mach-O
# binary too.  CI runs this on the macos-15 runner (.github/workflows/userspace.yml).
#
#   userspace/build.sh [DEST_DIR]         (default userspace/out)
#
# Produces:
#   out/maliprobe.host   host copy, for `--abi` (the check CI greps)
#   out/maliprobe        arm64 iOS, for the phone
#
# The phone copy is not signed.  It does not have to be: the root it runs in
# boots with cs_enforcement_disable=1 and launchd_unsecure_cache=1, so an
# unsigned arm64 Mach-O runs (the kernel carries built-in entitlements for
# this).
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$repo/userspace/out}
src=$repo/userspace/jackpotlte-maliprobe/maliprobe.c

[ "$(uname)" = Darwin ] || { echo "error: $0 needs macOS (the iPhoneOS SDK and Mach-O)" >&2; exit 1; }

mkdir -p "$out"

# -Wl,-undefined,dynamic_lookup: IOKit's user entry points are declared in the
# source, not linked (the iOS SDK ships no IOKit headers), so the device's own
# IOKit resolves them when the tool loads.
common=(-O2 -Wall -Wextra -Werror -Wl,-undefined,dynamic_lookup)

echo "--- host copy (the ABI self-test) ---"
clang "${common[@]}" -o "$out/maliprobe.host" "$src"

echo "--- arm64 iOS copy (the phone) ---"
sdk=$(xcrun --sdk iphoneos --show-sdk-path)
xcrun --sdk iphoneos clang "${common[@]}" -target arm64-apple-ios16.0 \
	-isysroot "$sdk" -o "$out/maliprobe" "$src"

echo
file "$out/maliprobe"
echo
echo "built:"
ls -l "$out/maliprobe" "$out/maliprobe.host"
