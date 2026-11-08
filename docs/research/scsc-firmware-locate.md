# Locating the SCSC / Maxwell (MX140) WiFi firmware blobs for SM-A530F (jackpotlte)

Research-only. Paths below are derived from Samsung's public Eureka kernel
source for `jackpotlte` (`AlexFurina/Eureka-R25U-Jackpotlte`, branch `R25U`) and
its `exynos7885-jackpotlte` defconfig; they are labelled **verified** where seen
in that source and **inferred** where they depend on CSC/build choices. No
blobs have been downloaded here.

### Verifier's note (2026-11-08, checked against `Eureka-Kernel` at `e1b8464b`)

Confirmed in `drivers/misc/samsung/scsc/mx140_file.c`:

- Base dirs `MX140_FW_BASE_DIR_SYSTEM_ETC_WIFI "/system/etc/wifi"` (`:28`) and
  `MX140_FW_BASE_DIR_VENDOR_ETC_WIFI "/vendor/etc/wifi"` (`:29`); subdirs `conf`
  (`:36`) and `debug` (`:37`); core image name `MX140_FW_BIN "mx140.bin"` (`:38`).
- Variant suffixes `_s612` (`hw_ver 0xb0`… actually `0xb1`) and `_s611` (`hw_ver 0xb0`) at
  `:58-59`, so the RF-ID auto-selection is real.
- `force_flat` split at `:123/:126` (flat for `/vendor/etc/wifi/`, nested for
  `/vendor/etc/wifi/mx140/conf/`).
- `mac.txt` (`dev.c:40`) and the MIB names (`dev.c:28-36`).
- **No signature/MDF/RSA/SHA verification anywhere in `mx140_file.c` or `mx140.c`.**
  The "loads any payload that matches the protocol" conclusion holds at the *kernel*
  level; whether the MX140 core itself validates the image is still unverified.

**Corrections — these two were marked "verified" and are not:**

1. **`CONFIG_SCSC_WLBT_CONFIG_PLATFORM="lassen_univ"` is not supported.** That config's
   Kconfig default is `""` (`drivers/misc/samsung/scsc/Kconfig:21-24`) and the jackpotlte
   defconfig does not override it (only `CONFIG_SCSC_CORE=y` appears). The platform
   directory is a runtime module parameter (`cfg_platform`, `mx140_file.c:192-224`).
   "lassen" elsewhere in the tree is an **audio codec** property (`use-lassenA` in
   `cod3035x` jack detection on 7884B/7904 devices) and has nothing to do with WiFi.
   The lookup order at `:192-224` tries `cfg_platform`, then `""`, then flat — so the
   practical instruction is: **enumerate the real `conf/` tree in `vendor.img`**, do not
   assume a platform name.
2. **MAC path.** The Kconfig default is `/efs/wifi/.mac.info`
   (`drivers/net/wireless/scsc/Kconfig:166-169`), not `/mnt/vendor/efs/wifi/.mac.info`.
   The `/mnt/vendor/efs` form is an Android-10-era convention and does not appear in
   this tree.

Everything else (AP tar → `vendor.img` → `etc/wifi/...`, CSC overlay caveat) is
reasonable but still **unverified against a real package** — nobody here has opened one.

## 1. Exact blob inventory (on-device paths)

Base directory is chosen at runtime by `mx140_basedir_file()`:
`/vendor/etc/wifi` (verified: kernel does `/system` then `/vendor` existence
checks; `CONFIG_SCSC_CORE_FW_LOCATION_AUTO` defaults to `y`, and
`SCSC_SEP_VERSION` is >= 8 for Android 9, so the post-Oreo `/vendor/etc/wifi`
branch is taken). If auto-detect is disabled, `CONFIG_SCSC_CORE_FW_LOCATION`
(default `/system/etc/wifi`) is used instead.

Because `cfg_platform` defaults to empty on this defconfig and `force_flat` is false
(`SCSC_SEP_VERSION >= 8`), the HCF lookup order is (read off `mx140_file.c:192-224`):

1. `/vendor/etc/wifi/mx140/conf/<cfg_platform>/...` (empty platform on this defconfig)
2. `/vendor/etc/wifi/mx140/conf/...`  (the branch that actually applies here)
3. `/vendor/etc/wifi/mx140_<name>.hcf`  (flat fallback)

| # | Role | Exact path(s) seen/requested | Source |
|---|------|------------------------------|--------|
| 1 | MX140 core image (R4) | `/vendor/etc/wifi/mx140.bin`, plus `/vendor/etc/wifi/mx140_s611.bin` and `/vendor/etc/wifi/mx140_s612.bin` if present (driver auto-selects by RF chip ID) | verified: `__mx140_file_download_fw()` builds `<base>/<variant><suffix>.bin` |
| 2 | Main MIB (regulatory/MAC-class config) | `/vendor/etc/wifi/mx140/conf/wlan/wlan.hcf` (platform empty), fallbacks above | verified: `slsi_mib_file="wlan.hcf"`, requested with `config_rel_path="wlan"` |
| 3 | SW MIB | same directories, `wlan_sw.hcf` | verified: `slsi_mib_file2="wlan_sw.hcf"` |
| 4 | Local/extra MIB (optional) | same directories, `localmib.hcf` | verified: `local_mib_file="localmib.hcf"` |
| 5 | MAC / device config | `/vendor/etc/wifi/<suffix>/conf/wlan/mac.txt` (via device conf), else fallback to `CONFIG_SCSC_WLAN_MAC_ADDRESS_FILENAME` (`/efs/wifi/.mac.info`) | verified: `maddr_file="mac.txt"`; `mx140_file_request_device_conf()` format |
| 6 | Debug blobs (optional) | `/vendor/etc/wifi/mx140/debug/...` (e.g. `log_strings.bin`) | verified: `mx140_file_request_debug_conf()` format |
| 7 | Runtime MAC address (per-device, not in firmware) | `/efs/wifi/.mac.info` | verified: Kconfig `SCSC_WLAN_MAC_ADDRESS_FILENAME` default |

Config deciding the path layout:
- `CONFIG_SCSC_CORE_FW_LOCATION_AUTO` (default `y`) decides base dir (runtime vs fixed).
- `CONFIG_SCSC_WLBT_CONFIG_PLATFORM` sets the conf platform subdirectory; its default is
  empty and this device's defconfig does not override it (the runtime `cfg_platform`
  module parameter does the same job).
- `SCSC_SEP_VERSION` (= `ANDROID_PLATFORM_VERSION`) decides `force_flat`; for Pie it is
  >= 8, so nested `conf/` paths are used (flat filenames are only fallback).
- `firmware_variant` and `cfg_platform` module params can override at runtime.

## 2. Firmware package / partition

- Vendor kernel/BLOBs live in the **Odin AP package** (`AP_...tar.md5` /
  `AP_...tar`), not BL/CP/CSC. They are in the **inner `vendor.img`**
  (`vendor.img.ext4`, sometimes `.lz4`-compressed) at the relative path
  `etc/wifi/...` (because the image root maps to `/vendor`). **`super.img`
  does not exist** on Android 9; system/vendor are separate images.
- Verified against the kernel defconfig/source; the region-specific CSC overlay
  (`CSC_...tar.md5` / `HOME_CSC_...`) may carry different `conf/wlan` content or
  regulatory HCF variants, so prefer the CSC matching the intended European region when
  the AP vendor image looks generic (inferred).
- Per-device data (MAC) is not in any tar; it lives in EFS.

## 3. Extraction on a Linux host

```
mkdir -p /tmp/opencode/wifi-extract && cd /tmp/opencode/wifi-extract
tar -xf AP_*.tar*                     # yields vendor.img (or vendor.img.lz4)
lz4 -t vendor.img.lz4 && lz4 -d vendor.img.lz4 vendor.img   # if compressed
file vendor.img                        # should report ext4 filesystem data
debugfs -R 'ls -l /etc/wifi' vendor.img
mkdir -p vendor_root
sudo mount -o loop,ro vendor.img vendor_root
cp -a vendor_root/etc/wifi ./wifi
sudo umount vendor_root
```

If the image is an ext4 sparse image instead, run `simg2img vendor.img vendor.img.raw`
first.

Verification:
- `find wifi -type f -printf '%p %s bytes\n'`
- Confirm all three MX140 core variants if present (`mx140.bin`, `mx140_s611.bin`,
  `mx140_s612.bin`) and the three HCF names exist and are non-empty.
- Record `sha256sum` of each file; compare against an independent extraction from
  a second firmware build/CSC. Identical hashes are the only integrity check we
  can perform without published checksums (none found).

## 4. Signing / authentication

- The driver reads these as ordinary files via `filp_open`/`request_firmware`
  and copies them into the WLBT shared memory. **No signature, MDF, or secure-boot
  check is present in `mx140_file.c`** in the Eureka source (verified).
- Conclusion: as far as the driver is concerned they are **plain payloads loadable by
  any driver speaking the protocol**. Device integrity is instead provided by
  Android Verified Boot / dm-verity over the vendor partition at OS boot, not by the
  Maxwell chip loading path (this latter point is inferred, not documented in source).
- The chip-level authentication behavior is not documented in the sources reviewed;
  if WLBT secure boot rejects unsigned R4 images on this SoC, that will need
  separate verification on hardware.

## 5. Alternative sources (ranked by cost)

1. **Same Odin firmware package** (SamMobile, SamFrew, OdinRom, samfirms) for
   `SM-A530F` with an EU CSC — authoritative and complete; full ~4 GB package is
   the cost (do not let any step materialize >50 MB without care).
2. **Lineage/community `jackpotlte` vendor trees** (e.g.
   `exynos7885-dev/proprietary_vendor_samsung_jackpotlte`) — useful for other
   blobs, but tree inspection showed **no SCSC WiFi files**, so insufficient
   alone.
3. **XDA Galaxy A8 2018 ROM/kernel forums** and device-specific AOSP/Lineage
   repos — may host extracted `/vendor/etc/wifi` subsets or mirrors; completeness
   and modification status are unverifiable remotely.
4. **Samsung open-source kernel (Eureka)** provides the driver and defconfig only;
   Samsung does not ship these proprietary firmware blobs in any open-source
   release reviewed (inferred).
5. Cross-device reuse from other Exynos 7885-era devices running the same SCSC
   stack is possible **only if** the RF chip ID variant matches; treat as last
   resort.

## 6. Red flags

- `mx140*.bin` has RF-chip-ID suffix variants (`_s611`, `_s612`, plain); keep
  all variants in the directory so the driver auto-selects. Dropping the wrong
  one can silently leave no matching firmware.
- Regional variants: regulatory HCF content (transmit power/domains) is region/CSC
  specific. For European use, take the firmware with an EU CSC and prefer
  `localmib.hcf`/`wlan.hcf` from that same package; do not mix HCF files across
  regions.
- `mac.txt`/`.mac.info` is per-device; a firmware package cannot provide your MAC.
  Omission usually falls back to default/zero behavior rather than failing the load.
- Filenames are also module parameters (`mib_file`, `mib_file2`, `mib_file_name`,
  `maddr_file`, `firmware_variant`, `cfg_platform`, `base_dir`), so a path that
  looks wrong on one build may be intentionally overridden there.
- Verified (driver logic/base dir/defconfig) vs inferred (exact nested directory
  existence in a given vendor image, regional HCF difference, chip-level secure
  boot). If a path does not appear under `/etc/wifi` of your extracted image,
  that package uses a different layout — the log line `try ...` from
  `scsc_mx`/`mx140_file` will show the actual attempted paths.
