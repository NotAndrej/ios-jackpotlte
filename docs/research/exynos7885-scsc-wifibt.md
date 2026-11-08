# Exynos 7885 / Samsung SCSC Wi-Fi + Bluetooth (`jackpotlte`) — research note

Date: 2026-11-08. Author: research subagent. Scope: read-only web research.
No device, no git, no CI.

### Verifier's note (2026-11-08)

Checked against Samsung's 4.4 tree (`jackpotlte` defconfig):

- Confirmed: `include/scsc/scsc_mx.h:28-29` defines `SCSC_SERVICE_ID_WLAN = 1` and
  `SCSC_SERVICE_ID_BT = 2`, so that symbol is real.
- Confirmed: the Maxwell/mx140 API and the firmware names are real —
  `drivers/net/wireless/scsc/mgt.c` calls `mx140_file_request_conf()`/`mx140_request_file()`,
  and `dev.c` sets `wlan.hcf` / `wlan_sw.hcf` / `localmib.hcf`.
- **Correction — BT is more separable than this note claims.** Bluetooth is a
  separate driver, `drivers/misc/samsung/scsc_bt/`, not part of the WLAN driver.
  Two seams exist in that source:
  1. `SCSC_BT=y` (as in the jackpot defconfig) exposes a **character device**
     `SCSC_H4_DEVICE_NAME` (`scsc_bt_module.c:2079-2094`), carrying **HCI H4**
     framing — the file is full of `H4_RECV_ACL`/`H4_RECV_EVENT` packet types.
  2. `SCSC_BT_BLUEZ` (default **N**, *not* set in the jackpot defconfig) registers a
     standard virtual controller: `scsc_bluez.c:178-194` does `hci_alloc_dev()`,
     `hdev->bus = HCI_VIRTUAL`, `hdev->dev_type = HCI_BREDR`, `hci_register_dev()`.
  So the host-facing contract is standard HCI (H4 or a virtual HCI device). What is
  proprietary is the **transport to the chip** (Maxwell shared memory / MIF / HIP4),
  not the Bluetooth protocol. The report's "not separable, no generic HCI path"
  overstates the difficulty: an HCI-speaking OS (including iOS's Bluetooth stack) still
  needs the transport, but does not need a Bluetooth stack rewrite.
- WLAN is the opposite case: the host-facing contract is full `cfg80211` + Samsung
  MIB/firmware, with no thin standard seam.

## Summary

- The Exynos 7885 does **not** use a discrete, SDIO-attached Wi-Fi/BT part in the usual
  Samsung Broadcom sense. Samsung's datasheet/product page describes 802.11ac Wi-Fi,
  Bluetooth 5.0 and FM radio as integrated into the SoC.
  (https://semiconductor.samsung.com/processor/mobile-processor/exynos-7-series-7885)
- What the Samsung kernel calls `scsc_wifibt` is Samsung LSI's in-SoC **Maxwell**
  wireless subsystem (driver module description in `drivers/net/wireless/scsc/dev.c`:
  `"mx140 Wi-Fi Driver"`), reached through the **SCSC MX / MIF** shared-memory
  interface rather than a standard bus. WLAN and BT are two *services*
  (`SCSC_SERVICE_ID_WLAN`, `SCSC_SERVICE_ID_BT`) on the same Maxwell core.
  (https://raw.githubusercontent.com/samsungexynos7885/android_kernel_samsung_universal7885/android-9.0/include/scsc/scsc_mx.h)
- HIP4 is the host-interface queue/scoreboard protocol implemented in
  `drivers/net/wireless/scsc/hip4.h`: a 4–6 queue, 256-entry ring-style config shared
  in DRAM between host and firmware.
- Public/open implementation: **none** in mainline Linux, and no BSD/Darwin/iOS driver
  is known. TizenRT ships a Samsung-maintained port of the same SCSC/MX140 Wi-Fi
  driver, but that is an RTOS port, not a non-Android PC OS driver, and I did not
  find evidence of BT there.
- The BT path is **not** a separable, generic HCI controller: it is `SCSC_SERVICE_ID_BT`
  behind the same Maxwell shared-memory/MIF transport. There is no evidence of a simple
  UART/USB/SDIO HCI seam. *Corrected by verification — see the Verifier's note: the host
  side does register a standard virtual HCI device.*
- Bottom line: realistically achievable under a non-Android kernel today is "use the
  existing Android vendor kernel's SCSC driver" (as postmarketOS's `linux-samsung-jackpotlte`
  appears to), not a fresh bring-up. For iOS, both WLAN and BT would require a full
  SCSC/MX transport plus firmware + cfg80211/BlueTool-equivalent glue.

## 1. What is SCSC / "S610"? silicon, bus, HIP4

### 1.1 Verified source

- Exynos 7885 product page lists integrated Wi-Fi/Bluetooth/FM/GNSS. SoC-integrated,
  not a discrete module with its own FCC ID.
  (https://semiconductor.samsung.com/processor/mobile-processor/exynos-7-series-7885)
- Notebookcheck confirms 802.11ac Wi-Fi, Bluetooth 5.0, FM radio in-chip.
  (https://www.notebookcheck.net/Samsung-Exynos-7885-SoC-Benchmarks-and-Specs.285088.0.html)
- SCSC driver tree in the Samsung 4.4 vendor kernel
  (https://github.com/samsungexynos7885/android_kernel_samsung_universal7885/tree/android-9.0/drivers/net/wireless/scsc)
  with files matching the supplied facts: `hip4.c`, `cfg80211_ops.c`, `dev.c`, `mib.c`,
  `ba.c`, `cac.c`, `debug_frame.c`.
- Device node `scsc_wifibt@120c0000`, compatible `samsung,scsc_wifibt` — supplied fact.
- Driver is built as `CONFIG_SCSC_WLAN`, `CONFIG_SCSC_BT`, `CONFIG_SCSC_WLAN_AP_INFO_FILE`
  — supplied fact.
- `drivers/net/wireless/scsc/dev.c` ( Samsung tree) declares `MODULE_DESCRIPTION("mx140 Wi-Fi Driver")`
  and attaches through `struct scsc_mx *core`:
  `slsi_dev_attach(struct device *dev, struct scsc_mx *core, struct scsc_service_client *mx_wlan_client)`.
  (https://raw.githubusercontent.com/samsungexynos7885/android_kernel_samsung_universal7885/android-9.0/drivers/net/wireless/scsc/dev.c)
- SCSC MX core offers service IDs
  `SCSC_SERVICE_ID_WLAN = 1`, `SCSC_SERVICE_ID_BT = 2`, `SCSC_SERVICE_ID_ANT`, etc.,
  so BT rides the same core service layer as WLAN.
  (https://raw.githubusercontent.com/samsungexynos7885/android_kernel_samsung_universal7885/android-9.0/include/scsc/scsc_mx.h)
- Host/firmware transport is the Maxwell Memory Interface (`MIF`): shared DRAM
  references (`scsc_mifram_ref`), contiguous shared buffers, mailboxes, and interrupt
  bits (`scsc_service_mifintrbit_*`, `fromhost/tohost` bits), i.e. shared memory +
  doorbells, not SDIO/PCI/USB/UART to a discrete chip.
  (same `scsc_mx.h`)

### 1.2 HIP4 protocol shape (from `hip4.h`)

(`https://raw.githubusercontent.com/samsungexynos7885/android_kernel_samsung_universal7885/android-9.0/drivers/net/wireless/scsc/hip4.h`)

- Six MIF queues: `HIP4_MIF_Q_FH_CTRL`, `FH_DAT`, `FH_RFB`, `TH_CTRL`, `TH_DAT`, `TH_RFB`
  (`enum hip4_hip_q_conf`), `MIF_HIP_CFG_Q_NUM = 6`, `q_len = 256`.
- `HIP4_DAT_SLOTS = 218`, `HIP4_CTL_SLOTS = 32`.
- Host-owned vs firmware-owned init/config structures (`hip4_hip_init`, v3/v4 config) with
  `magic_number`, firmware build/patch IDs, buffer locations/sizes, MIB location/size,
  queue locations, scoreboards, doorbell bit positions (`mif_fh_int_n`, `mif_th_int_n`).
- This is a firmware-assisted FullMAC stack: `netif.c`/`rx.c`/`tx.c` move 802.11 frames;
  `mib.c` loads `.hcf` MIB tables; `cfg80211_ops.c` exposes cfg80211 interface.

### 1.3 Silicon identity

**Not established** from public sources. What is verified:

- The SoC itself contains the WLAN/BT/FM IP.
- Samsung calls it "Maxwell" and uses `mx140` as the core variant/module name.
  (module description above; TizenRT paths below)
- It is a firmware-driven FullMAC core: `dev.c` says `chip ver=Maxwell, chan supp=2.4 & 5 GHz`,
  and firmware carries its own build/patch IDs.

What I could **not** establish:

- A discrete die/part number (e.g. S610-level chip) or whether the Maxwell IP is partly
  Marvell/Broadcom-derived. No teardown/FCC document I found isolates the Wi-Fi core
  separately from the Exynos die. Treat any "S610" name as unverified.

## 2. Firmware

### 2.1 Names observed

From the 4.4 driver (parameter defaults, `drivers/net/wireless/scsc/dev.c`):

- `slsi_mib_file = "wlan.hcf"`
- `slsi_mib_file2 = "wlan_sw.hcf"`
- `local_mib_file = "localmib.hcf"`
- `maddr_file = "mac.txt"`

So a full bring-up requires at least the two HCF MIB tables plus local MIB and a MAC file.

From Samsung TizenRT's SCSC port, which uses the same MX140 core
(https://github.com/Samsung/TizenRT/blob/master/os/drivers/wireless/scsc/misc/mx140_file.c):

- Base firmware image path pattern `/vendor/firmware/mx140/fw/...` (TizenRT defaults:
  `/vendor/firmware/mx140/fw/<variant>/mx140.bin`).
- HCF config under `/vendor/firmware/mx140/fw/<variant>/conf/wlan/wlan.hcf`.

For this chip that places the firmware blobs in the Android vendor partition, loaded with
`request_firmware()` by the host kernel.

### 2.2 BT firmware

I did not find the BT firmware filename or separate BT patch blob name in any public source.
The BT half of SCSC is a service on the same MX core, so RF patches may either be inside
the same Maxwell firmware image or loaded by the BT HAL; this is not documented publicly.
Treat BT firmware format as unverified.

### 2.3 Signing / authentication

No public source I found states whether `mx140.bin`/HCF is signed, verified, or encrypted.
The host driver just requests and hands the image to the Maxwell core through shared memory.
Do not assume either "loadable from an unmodified blob" or "strictly signed" without testing.

## 3. Mainline Linux / postmarketOS

### 3.1 Mainline Linux

- Mainline `drivers/net/wireless/` contains no `scsc`, `maxwell`, or `mx140` driver.
  (https://github.com/torvalds/linux/tree/master/drivers/net/wireless)
- No staging entry either: `drivers/staging/` has no SCSC wireless directory.
- Searched `lore.kernel.org` and driver lists indirectly via web search; no discussion,
  patch series, or RFC proposing an SCSC/MX140 WLAN/BT driver. No upstream driver exists.
- Exصالibur/teleport: `drivers/net/wireless/scsc` appears only in Samsung vendor trees
  (e.g. https://github.com/samsungexynos7885/android_kernel_samsung_universal7885/tree/android-9.0/drivers/net/wireless/scsc).

### 3.2 postmarketOS

- postmarketOS has a device package `samsung-jackpotlte` / `linux-samsung-jackpotlte`
  in testing (MR #1664, "Boots, display works").
  (https://gitlab.com/postmarketOS/pmaports/-/merge_requests/1664)
- That kernel is a Samsung 4.4-derived vendor kernel; Wi-Fi/BT on pmos for this device
  would ride the same downstream SCSC driver, not a mainline one. I found no evidence of
  an upmained `exynos7885` SCSC port in `linux-postmarketos-*` trees.
- Android LineageOS *does* ship the SCSC HAL (`hardware/samsung_slsi/scsc_wifibt`) — but
  that is an Android userspace/HAL layer over the kernel SCSC driver, not a new kernel driver.
  (https://github.com/MacBookPro34-comma-7/android_device_samsung_jackpotlte)

## 4. Non-Android OS ports and BT transport

- **TizenRT** (Samsung's lightweight RTOS) ports the SCSC/MX140 Wi-Fi driver under
  `os/drivers/wireless/scsc/`.
  (https://github.com/Samsung/TizenRT/blob/master/os/drivers/wireless/scsc/misc/mx140_file.c)
  This is the closest thing to a non-Android port, but it is still a Samsung RTOS port and
  targets a firmware filesystem layout, not desktop/Linux reuse. I found no clear BT service
  implementation or HCI transport in TizenRT's scsc tree.
- **BSD, Genode, Haiku, bare metal, Darwin/iOS**: no port or driver found in searches.
- **BT transport**: the kernel-exposed interface is `SCSC_SERVICE_ID_BT`, a second service
  on the shared-memory Maxwell MIF. There is no evidence of a standard HCI UART/USB/SDIO
  controller for BT. Android's BlueDroid/Fluoride likely talks to it through Samsung's
  SCSC HAL, not via `/dev/tty` or USB.
- Therefore iO/Android-style split stacks (Apple `BlueTool`, `IOBluetooth`) cannot simply
  point at an HCI adapter; they would need the proprietary Maxwell transport.

## 5. Difficulty assessment

| Path | What is needed |
|---|---|
| Any networking on Linux (non-Android) | Port the whole `drivers/net/wireless/scsc` tree plus `scsc_mx` MIF core, cfg80211 glue, and MIF interrupt/mailbox setup, then supply HCF/`mx140.bin` firmware from a stock image. Essentially "run the vendor kernel". |
| BT on Linux | Same plus an SCSC BT service driver. Not separable as a vanilla `hci_uart`/`btusb` device. |
| iOS WLAN | Reimplement SCSC/MX in XNU or port the Linux driver, expose `IO80211` via a custom driver, and feed firmware/HCF in the iOS bundle. Large effort; only worthwhile if tied to the project's existing Darwin bring-up. |
| iOS BT | Implement the SCSC BT service path to vend HCI-like or Apple-internal packets to `BlueTool`; without a separate controller there is no "generic" path. |

**Blunt conclusion:** Do not expect to get Wi-Fi or BT by "just loading a firmware" or by
reusing a generic Debian driver. The only realistic near-term route is to keep using the
Samsung vendor kernel's SCSC/MX stack (exactly what pmos currently does), incrementally
porting pieces if the project moves to XNU/iOS.

## 6. Confidence / what I could not confirm

- **Silicon identity is not established** beyond "integrated Exynos 7885 Maxwell/MX140".
  I did not find a discrete part number or independent derivation evidence.
- **Exact stock firmware paths and BT blob names are not verified.** TizenRT paths are a
  reference, not proof of the A8's `/vendor` layout.
- **Firmware signing/authentication is unknown.**
- **No outside driver for SCSC WLAN/BT is known** from the searches run; absence of
  evidence is not proof of absence, but mainline `drivers/net/wireless`, pmaports, and
  GitHub searches turned up nothing.
- I did not download or inspect a stock Samsung firmware image, and I did not run any
  device or CI commands.

## Components table

| Component | What it is | Upstream / non-Android support | Source |
|---|---|---|---|
| SCSC Wi-Fi/BT kernel driver (`scsc`) | cfg80211 FullMAC driver for Maxwell core; HIP4 host interface | None in mainline; vendor-kernel only; TizenRT has a Samsung RTOS port | https://github.com/samsungexynos7885/android_kernel_samsung_universal7885/tree/android-9.0/drivers/net/wireless/scsc |
| SCSC MX core (`scsc_mx`) | Shared-memory/mailbox/interrupt transport between host and Maxwell core | None outside Samsung kernels | https://raw.githubusercontent.com/samsungexynos7885/android_kernel_samsung_universal7885/android-9.0/include/scsc/scsc_mx.h |
| HIP4 queue protocol | 6×256-entry MIF queues + scoreboards | Defined only in Samsung driver headers | https://raw.githubusercontent.com/samsungexynos7885/android_kernel_samsung_universal7885/android-9.0/drivers/net/wireless/scsc/hip4.h |
| Maxwell WLAN firmware (`mx140.bin`, HCF MIBs) | Firmware + MIB tables loaded at boot | Shipped in Android vendor partition; no separate public repo | https://github.com/Samsung/TizenRT/blob/master/os/drivers/wireless/scsc/misc/mx140_file.c |
| BT (service id 2) | Second SCSC service on same core | No generic HCI path known | https://raw.githubusercontent.com/samsungexynos7885/android_kernel_samsung_universal7885/android-9.0/include/scsc/scsc_mx.h (`SCSC_SERVICE_ID_BT`) |
| postmarketOS device | `samsung-jackpotlte` using vendor kernel | Uses downstream SCSC driver; no mainline port | https://gitlab.com/postmarketOS/pmaports/-/merge_requests/1664 |
| LineageOS SCSC HAL | Android userspace HAL over SCSC kernel driver | Android-only | https://github.com/MacBookPro34-comma-7/android_device_samsung_jackpotlte |

**Sources basis**: Samsung 4.4 universal7885 kernel tree (GitHub), Samsung TizenRT (GitHub),
Samsung semiconductor Exynos 7885 page, postmarketOS pmaports MR #1664, LineageOS device tree,
Notebookcheck spec page. Own repo docs were not used as independent evidence.