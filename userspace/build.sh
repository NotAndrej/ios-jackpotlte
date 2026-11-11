#!/bin/bash
# SPDX-License-Identifier: MIT
# Build the jackpotlte userspace.  Needs macOS: the phone's payloads are arm64
# iOS, which come from the iPhoneOS SDK, and the ABI self-test is a Mach-O
# binary too.  CI runs this on the macos-15 runner (.github/workflows/userspace.yml).
#
#   userspace/build.sh [DEST_DIR]         (default userspace/out)
#
# Produces:
#   out/maliprobe.host       host copy, for `--abi` (the check CI greps)
#   out/maliprobe            arm64 iOS, for the phone
#   out/test_gpu_host        host copy of the M4 request layer's test, for CI to run
#   out/libjackpotlte.a      arm64 iOS: the M4 shim (request layer + iOS backend)
#   out/jackpotlte_gpu.h     the header that goes with the archive
#
# The phone copies are not signed.  They do not have to be: the root they run in
# boots with cs_enforcement_disable=1 and launchd_unsecure_cache=1, so an
# unsigned arm64 Mach-O runs (the kernel carries built-in entitlements for
# this).
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$repo/userspace/out}
probe_src=$repo/userspace/jackpotlte-maliprobe/maliprobe.c
lib=$repo/userspace/libjackpotlte

[ "$(uname)" = Darwin ] || { echo "error: $0 needs macOS (the iPhoneOS SDK and Mach-O)" >&2; exit 1; }

mkdir -p "$out"

# -Wall -Wextra -Werror on everything: the request layer's structures are an
# ABI, and a warning there is a bug.  No -Wl flags here: the -c steps below
# cannot take them, and clang counts an unused one as an error.
common=(-O2 -Wall -Wextra -Werror -Wpedantic)

# -Wl,-undefined,dynamic_lookup: IOKit's user entry points are declared in the
# sources, not linked (neither the iOS SDK nor the macOS one ships IOKit
# headers for these), so the platform's own IOKit resolves them at load time.
# Link time only - an archive is not linked.
lookup=(-Wl,-undefined,dynamic_lookup)

echo "--- host copy of the probe (the ABI self-test) ---"
clang "${common[@]}" "${lookup[@]}" -o "$out/maliprobe.host" "$probe_src"

echo "--- host copy of the M4 request layer's test ---"
# The sanitizers are the reason this is a host build and not an arm64 one: they
# catch a wrong offset or a structure written past its size, which is exactly
# what a mistake in this layer is.
clang "${common[@]}" -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
	-o "$out/test_gpu_host" "$lib/jackpotlte_gpu.c" "$lib/test_gpu_host.c"

echo "--- the iOS backend parses without a Darwin header ---"
# It declares its own IOKit externs on purpose (the SDK ships no headers for
# them); this is what keeps that true, and it costs one compile.
clang "${common[@]}" -c "$lib/jackpotlte_iokit.c" -o "$out/jackpotlte_iokit.host.o"

echo "--- arm64 iOS probe ---"
sdk=$(xcrun --sdk iphoneos --show-sdk-path)
xcrun --sdk iphoneos clang "${common[@]}" "${lookup[@]}" -target arm64-apple-ios16.0 \
	-isysroot "$sdk" -o "$out/maliprobe" "$probe_src"

echo "--- arm64 iOS libjackpotlte.a ---"
obj=$out/.obj
rm -rf "$obj"
mkdir -p "$obj"
for source in jackpotlte_gpu.c jackpotlte_iokit.c; do
	xcrun --sdk iphoneos clang "${common[@]}" -target arm64-apple-ios16.0 \
		-isysroot "$sdk" -c "$lib/$source" -o "$obj/${source%.c}.o"
done
rm -f "$out/libjackpotlte.a"
libtool -static -o "$out/libjackpotlte.a" "$obj"/*.o
rm -rf "$obj"
cp "$lib/jackpotlte_gpu.h" "$out/jackpotlte_gpu.h"

echo
file "$out/maliprobe"
lipo -info "$out/libjackpotlte.a"
echo
echo "built:"
ls -l "$out/maliprobe" "$out/maliprobe.host" "$out/test_gpu_host" \
	"$out/libjackpotlte.a" "$out/jackpotlte_gpu.h"
