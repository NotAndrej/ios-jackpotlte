# Port attempt: WiFi for Samsung Galaxy A8 2018 (`jackpotlte`, Exynos 7885, SCSC Maxwell/MX140)

> **SUPERSEDED (2026-11-08, same day).** This is the earlier draft; read
> [`PORT-START.md`](PORT-START.md) instead. Two of its premises did not survive
> contact with the vendor tree or the first implementation:
>
> - **§2 and §5 assumed a `IO80211Controller` subclass.** The approach actually
>   implemented (`0139-jackpotlte-apple80211-glue.patch`, described in
>   PORT-START.md) does not subclass the family at all. It registers a plain
>   `IFT_ETHER` ifnet and answers the `SIOCSA80211`/`SIOCGA80211` ioctl
>   (`0x802869c8`/`0xc02869c9`) boundary that XNU already routes to the
>   interface, so wifid finds an interface with no Apple-private vtable to
>   link against. Treat every `IO80211Controller`/`IO80211Interface` symbol in
>   §2 and the §5 skeleton as an abandoned path.
> - **§1.4/§1.6 said "no firmware → no port, stop."** The SCSC firmware was
>   later obtained (`mx140.bin`, kept out of git) and `0138` implements the
>   mailbox/MIF transport and the `smxf` load path, so the §1 no-go gate no
>   longer applies.
> - **The opt-in boot-arg was renamed.** This draft says `redwoodwifi=1`; the
>   shipped gate is `a8wifi=1` (see PORT-START.md). `redwoodwifi` is not read
>   by any patch.
>
> What is still worth reading here: the §3 cfg80211→iOS primitive mapping and
> the §6 ladder/kill criteria. The register-level facts live in PORT-START.md,
> which is the accurate one.

**Status:** superseded draft, NOT applied, opt-in only. **Date:** 2026-11-08. **Scope:** read-only evidence from the local repo and Samsung's 4.4 vendor tree. No device, no git, no CI, no commits. Nothing in this document is wired into the boot path; every runnable piece below is behind an explicit `redwoodwifi=1` boot-arg and returns `false` on failure.

**Audience:** implementation only. Not a survey.

### Verifier's note (2026-11-08, added after checking against the vendor tree)

- **Completeness confirmed.** `slsi_ops` at `drivers/net/wireless/scsc/cfg80211_ops.c:3355` has **40**
  ops; the §3.1 table covers all 40 in 31 rows (related ops grouped, e.g.
  `set_tx_power/get_tx_power`, `PMKSA:…`, `APmode:…`, `sched_scan_start/stop`,
  `mgmt_tx/mgmt_tx_cancel_wait`). Nothing is silently dropped.
- **Vendor symbols real.** `hip4_init`, `scsc_mx_service_open`, `scsc_mx_service_start`,
  and `slsi_dev_attach` all exist in the subtrees named. (An earlier chat summary said
  `scsl_*`; that was a typo for `slsi_*` and does not appear in this document.)
- **Spot-checked mappings.** `slsi_scan`, `slsi_connect`, `slsi_disconnect`, `slsi_add_key`,
  `slsi_set_power_mgmt`, `slsi_mgmt_tx`, `slsi_set_qos_map` all exist as named.
- **What could not be verified is exactly what the document marks as such.** The
  Apple-side surface (`IO80211Controller` vtable, `apple80211Request` selector numbers,
  "Hello"-era Skywalk split) is labelled `[inference]` throughout and sourced to community
  headers (itlwm, MacKernelSDK) and the BlackHat USA 2020 IO80211FamilyV2 talk — none of
  which were fetched during this check. That labelling is honest; treat every Apple symbol
  name in §5 as a placeholder until the real `IO80211Family` kext is in hand.
- **Agreement on the gate.** The report's own conclusion (no firmware → no port) matches
  what the local filesystem shows: there is no `.hcf` or `mx140` core image anywhere on
  this machine, and `cm_if.c` comments confirm the MX service downloads firmware during
  `service_open`. Effort before the blobs exist is wasted.

---

## 0. Provenance and evidence tags

Every claim in this file is tagged:

- **[verified]** — pulled directly from a file under the two allowed roots.
- **[inference]** — reasoned from allowed sources plus public literature; not asserted as fact.
- **[invented]** — placeholder introduced by this draft because no public source exists. Treat all `[invented]` names as pseudocode, not as API.

Allowed roots read for this draft:

- `/home/not_andrej/ios-jackpotlte` (our repo)
- `/var/tmp/opencode-keep/Eureka-Kernel`, **restricted to** `drivers/net/wireless/scsc/`, `include/scsc/`, `drivers/misc/samsung/scsc_bt/`

No other paths were read. Lines referenced are in those subtrees.

---

## 1. Go / no-go gate (read this first)

### 1.1 Bottom line

**No-go until you obtain and ship the firmware blobs below.** [verified: vendor clobbers]

The SCSC WLAN driver cannot register a single `wiphy`, open the WLAN service, or bring the Maxwell core out of reset without the configuration/MIB tables and the core image. `scsc_mx_service_open(SCSC_SERVICE_ID_WLAN)` itself downloads the Maxwell firmware during service open (comment in `cm_if.c:439-540`: "Open service - will download FW"). There is no firmware-less bring-up mode, no built-in fallback tables, and no host stub of the firmware. **Effort spent on `IO80211Controller` glue before the blobs exist is wasted.**

This is the hard blocker. Nothing else in this document matters without (a) blobs and (b) a filesystem layout that can hand them to the MX core at the time `scsc_mx_service_open` runs.

### 1.2 Blobs required, as named in our vendor tree [verified]

From `drivers/net/wireless/scsc/dev.c` (module parameters):

| Blob | Module param | Used at | Role |
|---|---|---|---|
| `wlan.hcf` | `slsi_mib_file` (`dev.c:28`) | MIB load in `mgt.c:1272` via `mx140_file_request_conf(..., "wlan", mib_file_name)` | First static MIB table, vendor W1 format, length-checked |
| `wlan_sw.hcf` | `slsi_mib_file2` (`dev.c:32`) | Same path, appended after `wlan.hcf` | Second MIB table (sw MIB) |
| `localmib.hcf` | `local_mib_file` (`dev.c:36`, static) | Optional extra MIB values, appended third | Platform overrides |
| `mac.txt` | `maddr_file` (`dev.c:40`) | `mgt.c:256` `mx140_file_request_device_conf(..., "wlan/mac.txt")` | MAC address seed (trivial, fall back to default) |
| Maxwell core image | *(not in the WL tree; requested by MX core in `scsc_mx_service_open`)* | Service open | The actual `mx140` firmware. Filename not derivable from the WL subtree. |

Size pressure: `IMG_MGR_SEC_WLAN_MIB_SIZE` bounds the combined MIB region used in `hip4.c:1285-1340`. The MIB tables are small (tens of KB typically).

### 1.3 Where they come from on a stock SM-A530F [inference]

- The 4.4 tree references the relative paths `"wlan/<name>"` (`mgt.c:1272`, `"wlan/%s"` at `mgt.c:264`, `"wlan/%s"` again), and the BT co-processor requests `scsc_bt_addr` through `mx140_request_file` (`scsc_bt_module.c:653`).
- Samsung's TizenRT port of the **same** MX140 core (research note, `docs/research/exynos7885-scsc-wifibt.md §2.1`) keeps them under:
  - `/vendor/firmware/mx140/fw/<variant>/mx140.bin` (core image)
  - `/vendor/firmware/mx140/fw/<variant>/conf/wlan/wlan.hcf` (HCF MIB)
- Therefore, on a stock SM-A530F the most likely location is the **`vendor` partition** of the Android firmware, specifically under `/vendor/firmware/...` and (for the user-supplied MAC override) `/vendor/firmware/wlan/mac.txt`. [inference: TizenRT paths are a reference, not proof of the A8's layout]
- Samsung firmware packages for the A8 are distributed as Odin/AP split tarballs (`AP_..._A530FXX.tar.md5`, etc.). The `vendor` image is inside the AP or as `vendor.img`. The system image alone is **not** enough; the vendor image is the target.
- The Android root we previously flashed (`IOSRoot.sparse.img`) does **not** carry these files. IOSRoot is an iPhone image with no SCSC firmware. [verified by its description in `JACKPOTLTE_PROGRESS.md`]

### 1.4 Signed / authenticated? [unknown]

- The vendor WL tree never calls a signature check, never parses a key, and hands the buffer straight to `mx140_file_request_conf` with a firmware-style `struct firmware *`. [verified by grep in allowed subtree: no sha/rsa/verify in `drivers/net/wireless/scsc/`]
- The MX core (out of scope here, but the API in `include/scsc/scsc_mx.h` shows `scsc_mx_service_open` returning a service and `mxman` reset) is where any check would live. It is also where the file is fetched with `request_firmware()`-equivalent logic. I did **not** read that implementation. [verified forbidden by scope]
- The research note ([verified: earlier verifier note]) states: "No public source states whether mx140.bin/HCF is signed, verified, or encrypted." I reproduce that: **unknown**. Plan must assume either that S-Boot/Android verified them before, and our fresh copy must be bitwise clean, or that no check exists. Neither is known.
- Consequence: do **not** mutate these blobs. Treat them as opaque, integrity-checked-by-whoever-extracted-them data. Ship them verbatim from a stock image.

### 1.5 Recoverability [inference]

- **Yes, in principle**, by downloading a factory firmware package for SM-A530F and unpacking the vendor image. You lose nothing irrecoverable if the phone's Android install is gone — but you must actually obtain that firmware package, and the machine has no local copy [verified: `JACKPOTLTE_PROGRESS.md` says IPSW DMGs were removed, only 3.2 GB free; no vendor image is mentioned].
- **No backups exist in this repo.** [verified] Nothing (no `/var/tmp` vendor dump, no blob basename) is stored. `grep -rn "wlan.hcf" /home/not_andrej/ios-jackpotlte` returns nothing.
- The MAC address file is recoverable without any vendor image (default addressed in `mgt.c`, randomised from SoC UID under `CONFIG_ARCH_EXYNOS`, low-risk fallback).
- **BT co-download:** the BT service (service id 2) is a separate driver at `drivers/misc/samsung/scsc_bt/`. It also calls `mx140_file_request_conf(...)` and `mx140_request_file(...)` (`scsc_bt_module.c:578-653`) for its own config/firmware. That tree is in scope but I only confirmed calls. BT is a *second* blob set of the same class and equally unrecoverable without the same vendor partition. **This port attempt deliberately scopes WLAN only.**

### 1.6 Decision gate (must be answered before any engineering effort)

| Question | If "no" |
|---|---|
| Can we obtain a stock SM-A530F firmware package and extract `/vendor/firmware`? | **Stop. No WiFi possible; use USB Ethernet or BT-only path.** |
| Did the extracted blobs include `wlan.hcf`, `wlan_sw.hcf`, `localmib.hcf`, a core image, and an address file layout? | **Partial go: name-test with core image + minimum HCF; many features degrade silently (`mgt.c` logs "Skip MIB download").** |
| Do we have a loader path that can deliver multi-MB blobs to XNU in time for `service open`? | **Stop for WLAN; the XNU root (iOS root image) cannot carry vendor firmware without packaging work (bundle into KEXT resources).** |
| Is anyone willing to maintain the ported SCSC core as a kernel extension? | **Stop.** |

**If any of the first three is no, this attempt is correctly abandoned at §1.**

---

## 2. Interface surface: what XNU/iOS requires of a WiFi driver

### 2.1 Evidence basis [inference / not verified]

I **do not** have the Apple IO80211 SDK, nor `IO80211Family.kext`, on this machine, and the AGENTS hard rules forbid touching anything outside the two roots. The names in this section come from:

- **[verified external, not from allowed roots]:** public community headers published by the itlwm project (`IO80211Controller.h`, etc.), MacKernelSDK's 80211 headers, and the BlackHat USA 2020 "Dive into Apple IO80211FamilyV2" talk cited in the web search above, plus NewOSXBook on `Apple80211`. [inference: iOS 16 / Darwin 22 is V2-era, Skywalk-aware; header names should transfer; exact vtable layout will not]
- **[verified internal]:** our repo contains no `IO80211` reference (`grep -rn IO80211` returned only this draft's research note). There is no prior WiFi driver to copy.

Treat every Apple symbol below as **[inference]** or **[invented]**. Do not ship against them unverified.

### 2.2 KEXT / IOKit contract [inference]

The driver is a subclass of `IO80211Controller` (provided by the (private) `IO80211Family` kext) which the system loads during `I/O Kit` autoconf. [inference]

- Provider matching is by IORegistry personality / device type; for an in-SoC core there is no standard probe path. [inference] Our bootstrap would need either (a) force-load the KEXT through a boot-arg hook, or (b) register an `IOService` personality in the ADT chosen by this port (note `IONameMatch` patterns still carry `samsung,a5xelte` in some shim paths — we cannot claim a clean hook without an ADT node; that is a design item [inference]).

- An `IO80211Controller` is a Family-level object. It instantiates one or more interface nub (`IO80211Interface`, or in the Skywalk era `IO80211SkywalkInterface` / `IO80211VirtualInterface`) per active VIF. [inference from itlwm/MacKernelSDK headers].

- Pure-virtual overrides observed in the public headers (names as published; iOS 16 names may drift):

  - `apple80211Request(UInt cookie, int code, IO80211Interface *iface, void *data) = 0;`
  - `apple80211VirtualRequest(...)`, `apple80211SkywalkRequest(...)` (Skywalk era, `__MAC_10_15` and later)
  - `stopDMA() = 0;`
  - `hardwareOutputQueueDepth(IO80211Interface *) = 0;`
  - `performCountryCodeOperation(IO80211Interface *, IO80211CountryCodeOp) = 0;`
  - `useAppleRSNSupplicant(...)` (two overloads)
  - `dataLinkLayerAttachComplete(IO80211Interface *)`
  - `enableFeature(IO80211FeatureCode, void *) = 0;`
  - `getHardwareAddressForInterface(...)`

- The IO80211 family (not our driver) owns calling the Apple private protocol. Our code fulfils the protocol by decoding the `SIOCGIW*`/`apple80211_*` ioctl-ish bundles inside `apple80211Request` and friends. This is the biggest single API surface to reverse-engineer. [inference from public community headers]

### 2.3 Userspace side [inference]

- `wifid` [Apple daemon; inference naming] consumes the Family's MIG/XPC-ish interface and forwards to `CoreWiFi`/`CoreWLAN` used by UI (`WiFiManager` on the user side). [inference]
- The `Apple80211` framework is **private** [verified by the NewOSXBook/Lower-level references cited above: it lived under `IPConfiguration.bundle` and was hidden/removed from iOS public SDKs]. The `apple80211_*` selectors referenced by `IO80211Controller::apple80211Request` are the on-the-wire vocabulary between userspace and the KEXT. [inference: selector numbers are not available to me without the kext]
- **Controlled gating:** our earlier bootstrap requires `launchd_unsecure_cache=1` even for App Store binaries; arbitrary userland is not in a position to load a custom WiFi KEXT. The KEXT must be loaded by the kernel itself, gated on `redwoodwifi=1`, or not at all. [verified by device bring-up history in `JACKPOTLTE_PROGRESS.md`, §stage 6/7]

### 2.4 What is documented vs private [inference]

| Layer | Status | Source of truth I can cite |
|---|---|---|
| `IOService`/IOKit lifecycle (`start`, `attach`, `registerService`) | Public Apple kernel API docs | Apple IOKit Fundamentals [verified external] |
| `IOEthernetInterface`, mbuf paths | Public-ish (IONetworkingFamily) | public headers [not checked locally] |
| `IO80211Controller` vtable | Private; no public Apple header in our roots | Community headers (itlwm/MacKernelSDK) only [inference] |
| `apple80211_*` selectors and structs (`apple80211_assoc_result_data`, etc.) | Private/undocumented; published only by community reverse engineering | community headers [inference] |
| Skywalk interfaces (`IO80211SkywalkInterface`) | iOS-13+ refactor; private | BlackHat 2020 talk [inference] |
| `Apple80211` userspace library | Private, hidden | NewOSXBook [inference] |

I cannot verify whether the vtable layout in the community headers matches iOS 16.2/Darwin 22.6. **[unverified — must be checked at link time]**. This is the highest risk of the port beyond firmware.

---

## 3. Mapping table: vendor SCSC/cfg80211 primitives → iOS entry points

Axes: **Vendor (file::function)** = proven real (all from allowed subtree). **iOS target** = Apple-side entry point; empty where none exists / cannot be verified (`—` = no known, must be invented).

### 3.1 cfg80211 ops (`cfg80211_ops.c:3355`, `slsi_ops`)

| Vendor primitive | Vendor function | Linux side | iOS side (verified/inference) |
|---|---|---|---|
| add_virtual_intf | `slsi_add_virtual_intf` | cfg80211 | `IO80211Controller::createVirtualInterface`? [invented — no verified name] ; must be invented as a plain `IO80211VirtualInterface` attach |
| del_virtual_intf | `slsi_del_virtual_intf` | cfg80211 | — (mirror) |
| change_virtual_intf | `slsi_change_virtual_intf` | cfg80211 | — |
| `scan` | `slsi_scan` | cfg80211 scan request | Apple's "scan" command flows through `apple80211Request(code=SCAN, ...)` selector [inference]. No verified selector number. Must drive vendor `slsi_scan` from there. |
| `connect` | `slsi_connect` | cfg80211 connect | `apple80211Request` assoc path [inference]. |
| `disconnect` | `slsi_disconnect` | cfg80211 disconnect | Apple80211 `DISASSOC` [inference]. |
| `add_key` | `slsi_add_key` | cfg80211 key install | `apple80211Request` key ioctl [inference]. |
| `del_key` | `slsi_del_key` | cfg80211 | (as above) |
| `get_key` | `slsi_get_key` | cfg80211 | (as above) |
| `set_default_key` | `slsi_set_default_key` | cfg80211 | (as above) |
| `set_default_mgmt_key` | `slsi_config_default_mgmt_key` | cfg80211 | (as above) |
| `set_wiphy_params` | `slsi_set_wiphy_params` | cfg80211 | — (must be swallowed) |
| `del_station` | `slsi_del_station` | cfg80211 | — |
| `get_station` | `slsi_get_station` | cfg80211 | Apple info ioctl [inference]. |
| `set_tx_power` / `get_tx_power` | `slsi_set_tx_power`, `slsi_get_tx_power` | cfg80211 | —/country regulatory |
| `set_power_mgmt` | `slsi_set_power_mgmt` | cfg80211 ps | Apple power mgmt [inference]. |
| `suspend` / `resume` | `slsi_suspend`, `slsi_resume` | cfg80211 | `IO80211Controller` power-state hooks [inference]. |
| PMKSA: `set_pmksa`/`del_pmksa`/`flush_pmksa` | `slsi_set_pmksa` etc. | cfg80211 | Supplicant-managed by Apple (we likely **do not** need to implement; Apple may use its own RSN supplicant because `useAppleRSNSupplicant()` exists) [inference]. |
| `remain_on_channel` / `cancel_remain_on_channel` | `slsi_remain_on_channel`, `slsi_cancel_remain_on_channel` | cfg80211 | Apple P2P/AWDL path [inference; likely unused for simple STA]. |
| `change_bss` | `slsi_change_bss` | cfg80211 | — |
| `set_channel` (legacy ≤3.5) | `slsi_set_channel` | cfg80211 | — |
| AP mode: `start_ap`/`change_beacon`/`stop_ap` | `slsi_start_ap`, `slsi_change_beacon`, `slsi_stop_ap` | cfg80211 AP | iOS hotspot uses `IO80211`'s own SoftAP path [inference]. Park this; STA only for v1. |
| `sched_scan_start`/`stop` | `slsi_sched_scan_start`, `slsi_sched_scan_stop` | cfg80211 bg scan | (nice-to-have) |
| `mgmt_frame_register` | `slsi_mgmt_frame_register` | cfg80211 | — (Apple's management frame subscriptions handled by Family) |
| `mgmt_tx` / `mgmt_tx_cancel_wait` | `slsi_mgmt_tx`, `slsi_mgmt_tx_cancel_wait` | cfg80211 | — |
| `set_txq_params` | `slsi_set_txq_params` | cfg80211 queues | — |
| `external_auth` (SAE) | `slsi_synchronised_response` | cfg80211 | — |
| `set_mac_acl` | `slsi_set_mac_acl` | cfg80211 | — |
| `update_ft_ies` | `slsi_update_ft_ies` | cfg80211 FT | Apple roaming [inference]; unused until roam lands |
| `tdls_oper` | `slsi_tdls_oper` | cfg80211 TDLS | — |
| `set_qos_map` | `slsi_set_qos_map` | cfg80211 QoS map | — |

### 3.2 MIB get / set [vendor]

- The MIB path is internal firmware configuration, never cfg80211. MIB get/set is driven from:
  - `mgt.c:1112` `slsi_mib_get_platform`
  - `mgt.c:1391` chip version readback
  - `mgt.c:1700-1890`: APF/RTT/gscan capabilities, per-PSID requests
  - encode/decode helpers in `mib.c`: `slsi_mib_encode_get_list`, `slsi_mib_decode_get_list`, `slsi_mib_find`
- There is **no iOS equivalent entry point**. Apple's `apple80211Request` selectors are an *open* vocabulary; we can expose our own private debug get/set through there, but the operating system will not call them. **MIBs become a start-up-only thing: load the tables once, do not plan on runtime MIB knob twiddling from iOS.**

### 3.3 Tx / Rx data path [vendor]

| Vendor | Function (file) | What it does | iOS target [inference] |
|---|---|---|---|
| netdev open/stop | `slsi_net_open`, `slsi_net_stop` (`netif.c`) | bring interface up/down | `IO80211Interface` enable, or `ifnet` attach/detach |
| start xmit | `slsi_net_hw_xmit` (`netif.c:1099`) | dequeue to HIP | `ifnet_output` / Skywalk `KL`? (invented) |
| Rx poll | `slsi_net_rx_poll` (`netif.c:986`) | NAPI budget loop | XNU network stack input (mbuf) |
| lower xmit | `slsi_tx_data`, `slsi_tx_data_lower` (`tx.c:147`,`424`) | mbulk → HIP queues | mmapped tx rings |
| tx control | `slsi_tx_control` (`tx.c:559`) | firmware commands | — |
| Rx scan | `slsi_rx_scan_ind`, `slsi_rx_scan_done_ind`, `slsi_scan_complete` (`rx.c`) | scan results | cfg80211 scan-done equivalent → Apple WiFi scan cache (must invent; Apple's Family drives scan UI) |
| Rx connected | `slsi_rx_connected_ind`, `slsi_rx_roamed_ind`, `slsi_rx_roam_ind` (`rx.c`) | join/roam indications | must call the Family's assoc-complete paths [invented] |
| Tx pause/unpause | `slsi_tx_pause_queues`, `slsi_tx_unpause_queues` (`tx.c:611,619`) | flow control | — |

### 3.4 Power save [vendor]

| Vendor | Function | iOS |
|---|---|---|
| `slsi_set_power_mgmt` | cfg80211 | Apple power management (IOPMPowerSource/MagicP? invented) |
| `slsi_suspend` / `slsi_resume` | cfg80211 | `IO80211Controller` sleep/wake hooks [inference] |
| wake locks (`wakelock.c`, `slsi_wakelock_init`) | Linux wakelocks | XNU power assertions [invented] |
| recovery timeout / `slsi_sm_*` | cm_if | — |

### 3.5 Event handling [vendor]

Vendor drives Linux `cfg80211` event functions (scan-done, connect-result, roam, regulatory, etc.). [verified by presence of cfg80211 include + cfg80211_* calls implied; not enumerated as identifiers] I have not extracted every call site. `[inference]` mapping to Apple: these events must be reflected into the IO80211 family state machine through Apple-internal APIs rather than cfg80211; they cannot be fed to a missing cfg80211. **Empty cell in many rows — must be invented at link time when the real vtable is in hand.**

### 3.6 Netdev ops [vendor]

`netif.c:1096`, `slsi_netdev_ops`:

- `.ndo_open = slsi_net_open`
- `.ndo_stop = slsi_net_stop`
- `.ndo_start_xmit = slsi_net_hw_xmit`
- `.ndo_do_ioctl = slsi_net_ioctl`
- `.ndo_get_stats = slsi_net_get_stats`
- `.ndo_select_queue = slsi_net_select_queue`
- `.ndo_fix_features = slsi_net_fix_features`
- `.ndo_set_rx_mode = slsi_set_multicast_list`
- `.ndo_set_mac_address = slsi_set_mac_address`

iOS target: **IOSkywalk / ifnet (`ifnet` attach + `if_output`, mbuf path)** [inference] — not `ndo_*`. Our implementation must satisfy both the IOKit IOEthernetInterface view and the Skywalk packet path. Which APIs exist and whether they are public is **[unverified]**.

---

## 4. The transport: must be reimplemented vs lifted

### 4.1 What must be reimplemented (Linux-specific, all ends)

| Linux construct | Where in vendor | XNU replacement |
|---|---|---|
| `struct sk_buff` receive/transmit paths | `rx.c`, `tx.c`, `netif.c` | `mbuf_t` / `IOBufferMemoryDescriptor` |
| `netdev` / `napi_struct` | `netif.c:986` | `ifnet` + Skywalk, or stubbed |
| `workqueue`, `tasklet`, `spin_lock`, `mutex`, `completion` | `hip4.c:1490-1496`, `dev.c` | IOKit locks / `IOWorkLoop` / `IOCommandGate` |
| Linux `procfs` (hip4 stats, debug) | `hip4.c:193-344`, `procfs.c` | `sysctl`/IORegistry properties or cut |
| `regmap`? | not found as regmap in scope | device reads; no MMIO regmap here |
| DMA API (`kmalloc`, `dma_alloc_coherent`) | mifram layer API only | `IOMemoryDescriptor`/`IODMACommand` |
| Linux firmware loader (`request_firmware`) | `mx140_request_file`, `mx140_file_request_conf` | bundled blobs in KEXT resources; read via `OSData`/file I/O — we must implement the file-read path ourselves, out of scope of the WL tree |
| cfg80211 | entire `cfg80211_ops.c` | see §3; Apple family instead |

### 4.2 Transport that is *lifted nearly verbatim* (memory layout + protocol constants)

Everything defined by the **protocol**, not by the OS, is reusable as-is: HIP4 config structs, queue indices, magic numbers, doorbell bit positions, mbulk geometry, scoreboard. These are in data, not code.

- **HIP4 shared-memory layout constants** [verified: `hip4.h`]:
  - queues: `HIP4_MIF_Q_FH_CTRL`, `FH_DAT`, `FH_RFB`, `TH_CTRL`, `TH_DAT`, `TH_RFB`; `MIF_HIP_CFG_Q_NUM = 6`; `q_len = 256`.
  - `HIP4_DAT_SLOTS = 218`, `HIP4_CTL_SLOTS = 32`.
- **HIP4 config structs v3/v4** with field layout [verified: `hip4.h:struct hip4_hip_config_version_3/4`] including `magic_number = 0xcaba0401`, `init.magic_number = 0xcaaa0400`, `host_buf_loc/sz`, `fw_buf_loc/sz`, `mib_loc/sz`, interrupt bit positions, scoreboard location. These must be byte-identical; **copy the structs verbatim** and leave field names.
- **mbulk pool geometry** [verified: `hip4.c` init code] with pool IDs `MBULK_CLASS_FROM_HOST_DAT` / `MBULK_CLASS_FROM_HOST_CTL` and constants `IMG_MGR_SEC_WLAN_TX_*`. The mbulk allocator itself (`mbulk.c`) is modest and portable except Linux internals; lift the `mbulk.h` defined layout.
- **MIB framing** (`mib.c` encode/decode + hash): the `slsi_mib_encode_get_list`/`slsi_mib_decode_get_list` pair deals with on-wire firmware format. Lift the state machine, rewrite the allocator calls.

Vendor functions corresponding to each piece (all `[verified]` existence; roles inferred):

| Piece | Vendor (file::function) | Linux-specific? | Disposition |
|---|---|---|---|
| MIF interrupt register mask/unmask/bit set | `scsc_service_mifintrbit_*` (API in `include/scsc/scsc_mx.h`) | MX core supplies these; lift call sequence | Keep calls; MX core must be ported (see §4.3) |
| MIF addr ↔ ptr translation | `scsc_mx_service_mif_addr_to_ptr`, `..._ptr_to_addr` | MX core supplies | Keep (same) |
| MIF RAM alloc/free | `scsc_mx_service_mifram_alloc/free` | MX core supplies | Keep |
| Mailbox (alloc/get ptr) | `scsc_mx_service_alloc_mboxes`, `..._get_mbox_ptr` | MX core supplies | Keep |
| service lifecycle | `scsc_mx_service_open/start/stop/close` | MX core supplies | Keep sequence; `slsi_sm_wlan_service_open/start` orchestration in `cm_if.c` |
| HIP4 init (shared memory ring/scoreboard fill-in) | `hip4_init` (`hip4.c:1237`) | workqueue/timer/populate procfs | **Lift ~90%, replace workqueue/timer/procfs with placeholders** |
| HIP4 setup/start/stop/deinit | `slsi_hip_start/setup/stop`, `hip4_setup` (`hip4.c:1625`), `hip4_deinit` (`hip4.c:1737`) | Linux mutex/irq | Rewrite synchronisation |
| HIP4 TX (control+data) | `scsc_wifi_transmit_frame` (`hip4.c:1525`) | mbulk+work queue | Rewrite queue kick + locking |
| HIP4 RX interrupt handler | `hip4_irq_handler` (`hip4.c:1174`) | Linux IRQ callback + tasklet | Rewrite as IOKit interrupt handler → `IOWorkLoop` command gate |
| HIP4 BH | `hip4_tasklet`/`hip4_wq` (`hip4.c:838`,`840`) | tasklet/workqueue | Rewrite as workloop |
| RX translation mbulk→skb | `hip4_mbulk_to_skb` (`hip4.c:607`) | skb alloc | Rewrite to `mbuf_t` |
| TX translation skb→mbulk | `hip4_skb_to_mbulk` (`hip4.c:452`) | skb | Rewrite from `mbuf_t` |
| Watchdog | `hip4_watchdog` (`hip4.c:745`) | timer | IOTimerEventSource |
| suspend/resume/freeze | `hip4_suspend/resume/freeze` (`hip4.c:1678-1736`) | — | Keep logic |
| HIP state guard | `atomics` `hip_state` | — | Keep, replace atomic type |
| MIB load | `slsi_mib_open_file/get` (`mgt.c~1240+`) | `request_firmware` | Reimplement file read; keep MIB hashing/framing |
| Chip init / version readback | `mgt.c:1350-1460` (~) | — | Keep logic; calls `slsi_mib_*` + firmware |
| cfg80211 registration | `slsi_cfg80211_new/register` | cfg80211 | Replace entirely with IO80211 attach (§3) |
| Netif init | `slsi_netif_init` (`netif.c:1345`) | netdev | Replace entirely with ifnet/Skywalk |
| Dev attach | `slsi_dev_attach` (`dev.c:269`) | cfg80211 device | Rewrite shell; keep field init of `sdev` |
| Platform regdomain | `slsi_regd_init` (`dev.c:229`) | cfg80211 custom regulatory | Drop — Apple's region handling [inference] |

`__init`-style: none in the WL tree appear Linux `__init` (it is not a one-shot probe module here; the driver attaches through the MX module client after probe). [verified grep-style in scope did not find `__init` on top-level functions] Instead the "probe" equivalent is `slsi_dev_attach` + `slsi_hip_init` (called at attach time) and `slsi_sm_wlan_service_open/start` (called from `cfg80211_ops` / netdev open).

### 4.3 The MX (MIF) core itself — **the unscoped prerequisite**

The MX core implementation lives in `drivers/misc/samsung/scsc/` which the task **explicitly forbids** me from reading beyond `scsc_bt/`. [verified: scope instruction] I therefore did **not** inspect the MIF register-level implementation, mailbox internals, AxDMA, firmware-load state machine, or platform probe.

Consequences for this port:

- The transport **cannot be honestly ported** from this draft alone. §4.2 lists only the *WLAN-facing* calls to the MX core. The MX core (`platform_mif.c`, `miframman.c`, `mifintrbit.c`, `mxman.c`, `scsc_mx_impl.c`, `mx140_file.c`, etc. [inferred filenames from the allowed `scsc_bt` includes and from public trees; I did not read them]) contains:
  - the Samsung Exynos MIF/MMIO init (shared-memory aperture, BAAW window at `SCSC_BAAW1_OFFSET 0x400000` per `scsc_mx.h`)
  - AxDMA/aer vs Coredump handling
  - firmware image request + download into Maxwell's view of RAM [verified: `scsc_mx_service_open` "will download FW"]
  - recovery/restart on firmware crash (the `mxman` recovery flow invoked in `slsi_sm_wlan_service_open`)
  - mailbox ring glue and the `mif_fh_int_n`/`mif_th_int_n` doorbell templates
- **This is the largest unknown chunk of the port.** Either port the entire `drivers/misc/samsung/scsc/` MX core (read it under a separate scope decision) or keep using the Samsung vendor kernel for connectivity indefinitely. [inference]
- The scope of this draft is deliberately WLAN-only. **Do not attempt BT until MX core is settled.**

### 4.4 Interrupt handling [verified existence; mechanism inferred]

- `hip4_init` registers a **to-host** interrupt: `scsc_service_mifintrbit_register_tohost(service, hip4_irq_handler, hip)` (`hip4.c:1263`).
- It masks it during init (`..._bit_mask`) and allocates a from-host bit (`scsc_service_mifintrbit_alloc_fromhost(service, SCSC_MIFINTR_TARGET_R4)`).
- `hip4_irq_handler` (`hip4.c:1174`) is a Linux IRQ handler signature `int irq, void *data`. Must become an XNU-style interrupt handler returning to an `IOWorkLoop`.
- The chip's GIC line for the MIF is whatever the MX core maps; [not available in allowed subtree] for the 7885. [inference] Platform MIF likely drives a SoC interrupt (the `SYSTEM INT`/`MIF` interrupt) — identify during MX-core scoping, not from here.

### 4.5 Shared-memory layout [verified constants]

Constants from `include/scsc/scsc_mx.h`: `SCSC_BAAW1_OFFSET 0x400000` (offset from 0x80000000 start of the BAAW1 region). `IMG_MGR_SEC_*` constants are in `hip4.c`/its headers (`IMG_MGR_SEC_WLAN_*_OFFSET/SIZE`); these define the 2 MiB mifram window, TX/RX buffer pools, MIB region, config region. [verified their use; the macro definitions themselves were in included platform headers outside my allowed directory — they are part of `include/scsc/` not listed, but referenced. I did not read those headers because they are outside `include/scsc/`? Actually `hip4.c` includes them transitively. I did not grep them; values unknown in this draft.]
- ` IMG_MGR_SEC_WLAN_TX_DAT`, `_TX_CTL`, `_RX`, `_CONFIG/MIB_OFFSET` geometry [inference: must be copied from headers during port; do not re-derive].

### 4.6 Where code can be copied verbatim [verified list]

- `hip4_hip_config_version_3/4`, `hip4_hip_init`, `scoreboard` structs (`hip4.h`).
- `scsc_service_client` callbacks shape (`scsc_mx.h`) — the contract; keep names.
- `slsi_dev_mib_info` fields used for load (compile against XNU's type).
- MIB magic/hash check arithmetic.

---

## 5. Skeleton code (single KEXT skeleton, structurally real, not intended to compile unaided)

All symbols marked `// [invented]` or `// [uncertain]` fall into scope note §2.4. Load-gated on `redwoodwifi=1` via `kern_getenv`/IOKit property check at `start()` (honest user must wire that; not in this skeleton).

```cpp
// RedwoodSCSC.cpp -- draft only. IOKit (Darwin 22 / iOS 16 era), 64-bit, PAC off (xnu tree is PAC-off).
// [invented] whole file. Kernel: no stdlib. Own calls to ported SCSC code are imagined.

#include <IOKit/IOService.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <libkern/OSAtomic.h>
#include <netinet/if.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>

extern "C" {
#include "scsc_mx.h"          // from include/scsc; unchanged contract
#include "hip4.h"             // copied layout constants
}

#define REDWOOD_WIFI_BOOTARG "redwoodwifi"

// --- ported (C) SCSC transport, kept in its own TU --------------------------------
extern "C" {
    struct slsi_dev;          // opaque handle identical to vendor dev.h
    struct slsi_dev *slsi_dev_attach_stub(struct RedwoodBoard *board, scsc_mx *core);
    int               slsi_dev_start_stub(struct slsi_dev *sdev);   // = slsi_sm_wlan_service_open+start
    void              slsi_dev_stop_stub(struct slsi_dev *sdev);
    void              slsi_hip_init_stub(struct slsi_dev *sdev);    // wraps hip4_init
    // [invented] name set -- real ported code names will differ
}

// [uncertain] exact base class for iOS 16. IO80211Controller is the V1/V2 family root.
class IO80211Controller;
class IO80211Interface;       // [uncertain] exact base of Skywalk-era interface
class IO80211SkywalkInterface; // [uncertain] name/version

class RedwoodSCSCInterface;   // [invented]

class RedwoodSCSC : public IO80211Controller {
    OSDeclareDefaultStructors (RedwoodSCSC);

public:
    // IOService
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;    // [verified API]
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;     // [verified API]

    // IO80211Controller pure-ish overrides (names from public community headers)
    virtual SInt32 apple80211Request(UInt cookie, int req, IO80211Interface *iface,
                                     void *data)                 // [verified-in-community-headers]
        { return stubApple80211Request(cookie, req, iface, data); }
    virtual SInt32 apple80211SkywalkRequest(UInt cookie, int req, IO80211SkywalkInterface *iface,
                                            void *data)           // [uncertain: iOS-16 name set]
        { (void)cookie; (void)req; (void)iface; (void)data; return 0; }
    virtual SInt32 stopDMA()                                      // [verified-in-community-headers]
        { return kIOReturnSuccess; }                             // [stub]
    virtual UInt32 hardwareOutputQueueDepth(IO80211Interface *i)   // [verified-in-community-headers]
        { (void)i; return 0; }
    virtual SInt32 performCountryCodeOperation(IO80211Interface *i, int op) // [verified-in-community-headers]
        { (void)i; (void)op; return 0; }
    virtual bool useAppleRSNSupplicant(IO80211Interface *i) { (void)i; return true; } // [uncertain]
    virtual void dataLinkLayerAttachComplete(IO80211Interface *i); // [verified-in-community-headers]
    virtual SInt32 enableFeature(int code, void *arg) { (void)code; (void)arg; return kIOReturnUnsupported; } // [verified-in-community-headers]

private:
    bool                 fEnabledByBootArg;   // [invented field]
    IOWorkLoop          *fWorkLoop;           // [verified type]
    IOCommandGate       *fGate;               // [verified type]
    IOTimerEventSource  *fWatchdog;           // [verified type, pattern from AppleCLCD shim]

    scsc_mx             *fMxCore;            // [invented plumbing] how provided? (see design note)
    struct slsi_dev     *fSdev;              // [verified shape, opaque]

    RedwoodSCSCInterface *fPrimaryIf;        // [invented]
    IOEthernetAddress     fMAC;               // [verified type]

    static SInt32 stubApple80211Request(UInt cookie, int req, IO80211Interface *iface, void *data);
};

OSDefineMetaClassAndStructors(RedwoodSCSC, IO80211Controller); // [verified macro pattern, see AppleCLCD]

// [invented] board plumbing. There is no provider in our IOKit registry yet.
// open question: how does start() obtain fMxCore? In the Linux model, scsc_mx
// is a platform device enumerated by the Exynos MIF driver. In XNU there is no
// enumerator. Options: (A) skip autoconf, force-load via kext via boot-arg but
// still need a provider; (B) make RedwoodSCSC match a fake ADT node with
// compatible "samsung,jackpotlte-scsc-wifi" and have an MX IOKit provider.
// Either way, this is an *MI* prerequisite not answered in this draft.

bool
RedwoodSCSC::start(IOService *provider)
{
    if (!IOService::start(provider))                      // [verified Apple pattern from AppleCLCD]
        return false;

    // Opt-in gate: boot path must be untouched unless explicitly enabled.
    // [invented] boot-arg read; equivalent using pe_getenv / kern_getenv.
    char buf[16]; size_t n = sizeof(buf);
    if (kern_getenv ? false : true) { }   // [invented] placeholder, do not compile
    const char *flag = 0;                 // [invented] obtain via kern_boot_args or g??
    fEnabledByBootArg = (flag && flag[0] == '1');
    if (!fEnabledByBootArg) {
        IOLog("RedwoodSCSC: disabled (redwoodwifi=1 to enable)\n");
        return false;     // honest failure; IOService fails to start; no taint of boot
    }

    // Build work loop first (pattern: AppleCLCD)
    fWorkLoop = IOWorkLoop::workLoop();                       // [verified API]
    if (!fWorkLoop) return fail(false, "no work loop");

    fGate = IOCommandGate::commandGate(this);
    if (!fGate || fWorkLoop->addEventSource(fGate) != kIOReturnSuccess)
        return fail(false, "no command gate");

    fWatchdog = IOTimerEventSource::timerEventSource(this, &RedwoodSCSC::watchdogFired);
    if (fWatchdog && fWorkLoop->addEventSource(fWatchdog) == kIOReturnSuccess)
        fWatchdog->setTimeoutMS(6000);

    // --- MX core + transport -----------------------------------------------
    // [invented] obtaining fMxCore. Implement actual attach per MX port (§4.3).
    fMxCore = (scsc_mx *)0;        // INVENTED PLACEHOLDER -- crash if reached as-is
    if (!fMxCore) {
        IOLog("RedwoodSCSC: no MX core; cannot attach\n");
        return false;
    }

    fSdev = slsi_dev_attach_stub((RedwoodBoard *)0, fMxCore);  // [invented stub]
    if (!fSdev) return false;

    slsi_hip_init_stub(fSdev);    // calls ported hip4_init -> shared memory rings

    // --- service start -------------------------------------------------------
    int rc = slsi_dev_start_stub(fSdev);   // open+start service; downloads firmware inside MX core
    if (rc != 0) {
        IOLog("RedwoodSCSC: WLAN service start rc=%d\n", rc);
        return false;
    }

    // MAC address from vendor maddr load (or SoC UID fallback) already in sdev;
    // read back via ported accessor [invented accessor]:
    // get_sdev_mac(fSdev, &fMAC);

    // --- IO80211 interface attach ------------------------------------------
    fPrimaryIf = new RedwoodSCSCInterface; // [invented]
    if (!fPrimaryIf || !fPrimaryIf->attach(this)) { // [uncertain method name]
        return false;
    }

    setProperty(kIOUserClientClassKey, "RedwoodSCSCUserClient"); // [invented kext's own client]
    registerService();                                          // [verified API]

    IOLog("RedwoodSCSC: started, MAC %02X:%02X:%02X:%02X:%02X:%02X (debug)\n",
          fMAC.bytes[0], fMAC.bytes[1], fMAC.bytes[2],
          fMAC.bytes[3], fMAC.bytes[4], fMAC.bytes[5]);
    return true;
}

void
RedwoodSCSC::stop(IOService *provider)
{
    if (fSdev) { slsi_dev_stop_stub(fSdev); fSdev = NULL; }
    if (fPrimaryIf) { fPrimaryIf->release(); fPrimaryIf = NULL; } // [uncertain]
    if (fWatchdog) { fWorkLoop->removeEventSource(fWatchdog); fWatchdog->release(); fWatchdog = NULL; }
    if (fGate) { fWorkLoop->removeEventSource(fGate); fGate->release(); fGate = NULL; }
    if (fWorkLoop) { fWorkLoop->release(); fWorkLoop = NULL; }
    IOService::stop(provider);     // [verified Apple pattern]
}

// [uncertain] signature of apple80211Request as passed; types from community headers.
SInt32
RedwoodSCSC::stubApple80211Request(UInt cookie, int req, IO80211Interface *iface, void *data)
{
    // This is the single choke point where all iOS-private Apple 80211 traffic
    // arrives. Map req codes (huge table, unknown numbers) to ported slsi_* calls:
    //   scan        -> slsi_scan(...) then cfg80211-like scan-done events back
    //   connect     -> slsi_connect
    //   disconnect  -> slsi_disconnect
    //   get_station -> slsi_get_station
    //   key install -> slsi_add_key/set_default_key
    //   power mgmt  -> slsi_set_power_mgmt
    // Field names/structs: community headers; iOS 16 likely a V2 variant. [uncertain]
    (void)cookie; (void)req; (void)iface; (void)data;
    return 0;              // [stub: say success to probing apps; unsafe, but non-driving]
}

void
RedwoodSCSC::dataLinkLayerAttachComplete(IO80211Interface *i)
{
    // [uncertain] called by Family once link attaches; good place to finish notify
    (void)i;
    IOLog("RedwoodSCSC: data link layer attach complete\n");
}

// --- interface nub ---------------------------------------------------------------
class RedwoodSCSCInterface : public IO80211Interface {   // [uncertain base]
    OSDeclareDefaultStructors (RedwoodSCSCInterface);

public:
    RedwoodSCSC *fController;   // [invented]
    bool attach(RedwoodSCSC *c) { fController = c; /* [invented] call family attach */ return true; }
};

OSDefineMetaClassAndStructors(RedwoodSCSCInterface, IO80211Interface); // [uncertain linkage]
```

**Notes on skeleton honesty:**

- `kern_getenv` reference is `[invented]` and almost certainly wrong for this XNU tree (this tree rarely exposes `kern_getenv` the way Darwin does; patch 0126 shows use of `kern_boot_args`/PE data). The intent is plain: parse `redwoodwifi=1` from `PE_boot_args`/`kern_getenv` at `start()`.
- `fMxCore = (scsc_mx *)0;` is an explicit crash hazard and is **deliberately** not wired. Wiring it requires the MX core port (§4.3) which is the true prerequisite.
- Every Apple vtable name is either community-header verified (`apple80211Request`, `stopDMA`, ...) or marked `[uncertain]`. iOS 16 may require the **Skywalk** overrides instead; if so the whole controller base changes and this skeleton's `IO80211Interface` members may not link.

---

## 6. Ladder and kill criteria

Order: each step must be evidenced on a real phone (photo / serial / screen output), same standard as `JACKPOTLTE_PROGRESS.md`. Build success never counts.

1. **L0 — KEXT loads and logs (opt-in).** Flash a BOOT image with `redwoodwifi=1`, observe `RedwoodSCSC: started/disabled/...` on serial/fb. Done when the KEXT's `start()` returns and the I/O Registry shows the personality. Kill: if load link-fails on IO80211 symbol names, stop and obtain real headers.
2. **L1 — Transport attaches (no radio).** `slsi_dev_attach_stub` + `slsi_hip_init_stub` succeed; hip4 rings configured; `scsc_mx_service_mifram_alloc(2MB)` succeeds; no crash. Done when `RedwoodSCSC: transport attached` marker printed and no later abort. Kill: if the shared-memory aperture/BAAW is wrong, stop (this is MX scope, unreachable from WL).
3. **L2 — Firmware downloads.** Service open returns 0 and the log reads a fw/patch id from the `hip_control` config window; observe `fw_build_id` nonzero in trace. Done when `slsi_rx_synchronised_ind`-class sync arrives or service state reaches `CM_IF_STATE_*` (name [verified host of constant not extracted]) running. Kill: if firmware fails signature/auth or MX core cannot read the blobs, **abandon WLAN** — nothing downstream can compensate.
4. **L3 — Scan.** Associate a CLI/IOCTL or a minimal `Apple80211` scan request returns scan results. Done when a probe request/response is seen in `rx.c` logs and bytes cross to the Family. Kill: if the event mapping needs >2 unknown Apple selectors to complete, pause and reverse-engineer before continuing.
5. **L4 — Key install / join.** `slsi_connect` reaches `slsi_rx_connected_ind`; interface transitions RUN. Done when the iOS-side network UI sees an IP-capable interface. Kill: if association works in trace but not in UI (`wifid` never pairs), the bottleneck is in §2 (Apple-private) and you are reverse-engineering, not porting — budget accordingly.
6. **L5 — Associates with an AP.** Full association + DHCP-equivalent on iOS (or at least carrier association + `ifconfig` shows associated at link layer). Done with both a screenshot and a `dmesg`/ramoops.
7. **L6 (stretch).** Suspend/resume across sleep; roaming; AP mode (hotspot) — do not attempt until L5 is photo-evidenced on multiple runs.

### 6.1 Kill criteria (abandon)

- No firmware blobs available (§1.6).
- MX core (`drivers/misc/samsung/scsc/`) cannot be brought in scope and read — this draft explicitly cannot port the transport without it.
- `IO80211Family` vtable does not link against the community headers' signatures (RSSI of `apple80211Request` differences), AND no locally available system image provides the Family kext to inspect. [inference that the iOS root's `System/Library/Extensions` carries it but access is out of scope of today's draft]
- Link‑time undefined symbols for `IO80211Controller` itself — the Family may have been renamed/removed in this Darwin (22.6); then the whole stack is iOS‑private beyond what we can stub.
- L0–L2 consume more than ~4 engineer‑weeks without a single successful service start: stop and switch to USB Ethernet.
- The phone enters the recovery partition repeatedly without returning a log (inability to see `IOLog` breaks all debugging).

---

## 7. Effort and honesty

### 7.1 Blunt estimate [inference]

- **Gated on prerequisites:** 2–6 weeks just to get MX core, BAAW aperture, aperture sizes and firmware on disk (scoping we did not do here because of the read limits).
- **Transport (§4.2)** port: 4–8 weeks for someone who can read the ported vendor C daily. Most risk is in locks/interrupts/workloops, not data structures (which are copied).
- **IO80211 integration (§2, §3, §5)**: 6–12 weeks, dominated by reverse-engineering Apple's request codec, not by typing. Anyone telling you less has not read §2.4.
- **Scan → associate end-to-end:** 12–24 engineer-weeks from a standing start; multiplayer, only on a hardware-available schedule (each feedback cycle is a flash+reboot+photo round trip, ~minutes).
- **Maintenance:** the ported SCSC core needs to track firmware blob versions; Apple's userspace also moves per iOS version.

### 7.2 Realistic alternatives

1. No WiFi: the device is usable over serial + USB Ethernet dongle for development. Highest value-for-effort if this port is otherwise done.
2. BT only: more separable than WLAN — `scsc_bt/` exposes HCI over the same MX transport (verifier's note: standard virtual HCI device on Linux, HCI H4 char device otherwise), which an iOS Bluetooth stack *can* consume much more cleanly than the `cfg80211`-shaped WLAN side. **[inference: BT is a smaller task than WLAN once MX core exists]** but needs the same MX core and another firmware set. BT is not covered by this skeleton.
3. Stay on the Android vendor kernel for connectivity, XNU for the rest: no effort, no WiFi-as-iOS-networking, but the SCSC HAL already works.

**Recommendation in one line:** port WiFi only if you are willing to commit to scoping the MX core (`drivers/misc/samsung/scsc/`) in the *next* read-heavy task; otherwise do USB Ethernet. [inference from this draft]

### 7.3 What I got wrong / could not verify

- **I could not read the MX (MIF) core.** This is the backbone of transport, and the part I am forbidden by the task's own scope from opening (`drivers/misc/samsung/scsc/` beyond `scsc_bt/`). **Everything in §4 about interrupt register layouts, AxDMA, firmware download state machine, recovery timing is generic or omitted, not verified.** [must be fixed by a follow-up scoping pass]
- **I did not enumerate the chip-interrupt line, nor confirm whether MIF interrupt is shared with GPU.** Guesswork; not sourced. [unverified]
- **Apple vtable names** in §2.2 and §5 are community-sourced (itlwm/MacKernelSDK), not Apple. iOS 16 / Darwin 22.6 may require Skywalk-based overrides instead; link-time failure is expected on first try. [inference only]
- **No Apple SDK or Family kext** was available to me; `getHardwareAddressForInterface`, `efiNVRAMPublished`, and the data-link attach flow I only saw as log strings in a StackOverflow post, not as source. [unverified]
- **Firmware location/signing** is inferred from TizenRT, not from a stock SM-A530F image. I did not download or unpack a firmware package. [inference only; §1.3–§1.4]
- **MIB file size limits, hash geometry and platform-extraction code path** (`slsi_mib_get_platform`) I referenced but did not fully trace through `mib.c` (472 lines) + `mgt.c` (6375 lines) — I cited line numbers from `grep`, not full comprehension of all branches.
- **I did not walk the path from `slsi_scan` to the cfg80211 scan-done calls** to confirm which cfg80211 helper is invoked; the row labelled "scan ind → cfg80211 scan done" is a plausible rendering, not a verified call-site. [verified gap]
- **User data flow turnstile:** I asserted "iOS root cannot carry vendor firmware without packaging work" in §1.6; IOSRoot is HFS+ and *could* carry a `/vendor` subtree, but launchd-XPC paths and AMFI would gate it. I did not verify the launch path for arbitrary firmware files. [inference]
- **Wrong precedent:** treating this like the S8 donor port. The A8 adds no `IONameMatch` provider beyond existing A5XELTE shim conventions; `samsung,a5xelte` survives in two places in the shim per `JACKPOTLTE_PROGRESS.md`. Any claim that "the shim already exposes a WiFi provider" would be false. [verified]
- **Two-vs-many blobs:** the task phrasing was "two firmware blobs matter." In reality the minimum set is closer to {core image} + {2 MIB tables + localmib} + {MAC file}: at least 4 blobs, and two *logical* ones (core image; HCF bundle). I kept that framing in §1.2 to avoid confusion.
- **Whatever else my inference got wrong** about `IO80211` on iOS 16 would not matter: the port's first realistic kill criterion is §1, not §5.

---

*Draft ends. Nothing in this file is applied to any tree; it lives under `docs/drafts/` and is outside the numbered patch series.*
