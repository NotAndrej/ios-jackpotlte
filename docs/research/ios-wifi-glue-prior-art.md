# iOS/XNU WiFi glue prior art: how public ports attached (or didn't) a non-Apple WiFi chip to Apple's wireless stack

Research-only. Read-only web research. No device, git, CI, commits or pushes. Every external claim below carries a label:

- **verified** — I read the named source and can quote it.
- **claim** — someone asserts it; I did not independently verify.
- **inference** — my reasoning.

Evidence rule followed: project names/URLs appear only after they were found in fetched sources. Where a requested project could not be verified to exist, it is **dropped** (see §7). Quotes are short and verbatim.

Companion documents already cover the chip side and are not redone here: `docs/research/wifi-chip-re-prior-art.md` (SCSC/MX140 transport), `docs/research/scsc-firmware-locate.md` (firmware paths), `docs/drafts/jackpotlte-wifi/PORT-ATTEMPT.md` (our Apple-side placeholder table).

---

## 1. Executive answer (one screen)

- **Did any public port get a working network interface at all?** Yes, several, on **wired** USB Ethernet (ios-sweet commit `0075` "USB Ethernet (CDC-ECM) function and ue0 network interface", phone answers ping) and on **WiFi** (HakiOS WCN6750, MakrSas topaz/Qualcomm WCN, joranv1 sailfish QCA6174 PCIe, danstronics S9+ Broadcom in dev sessions). No port runs **Samsung SCSC/MX140** WiFi.
- **Did anyone drive a non-Apple WiFi chip through IO80211/wifid on iOS?** **Yes.** HakiOS, sailfish and topaz each expose a Qualcomm radio to iOS as an Apple80211 interface (`en0`), and `wifid`/Settings/Safari work. They do **not** use Apple's `IO80211Controller` class; they answer the `SIOCGA80211`/`SIOCSA80211` ioctls and post `KEV_IEEE80211` kernel events themselves (verified in HakiOS `drivers/wifi/hakios.apple80211.c`). The hard part for us is therefore **not** the Apple glue; it is the chip transport + firmware.
- **Which half dominates for us?** The **chip/transport** half, by a wide margin. The Apple-side IO80211 glue now has a public reference implementation (HakiOS). The Exynos 7885 SCSC/MX140 transport has no portable reference beyond TizenRT (per `wifi-chip-re-prior-art.md`).
- **Can USB Ethernet substitute?** For development and general IP traffic, **yes** (demonstrated). For iOS **activation/setup**, **no** — these ports bypass activation through Hactivation; Ethernet does not make `wifid` or AirDrop/Continuity work, and `wifid` still expects an 802.11 interface.
- **Output file:** `/home/not_andrej/ios-jackpotlte/docs/research/ios-wifi-glue-prior-art.md`
- **Could not verify:** `georgev-6974/s9plus-ios` could not be confirmed to exist (GitHub search returned `danstronics/ios-s9plus-exynos` instead). Floppikrob ios-galaxy-s8 network state not stated anywhere fetched. UniXNU WiFi claims are thin.

---

## 2. Project-by-project: is any network interface up?

### 2.1 Table (verified where quoted)

| Project (owner/repo) | Device / SoC | Chip | Network state | Evidence |
|---|---|---|---|---|
| `dipa-studio/ios-sweet` | Xiaomi Redmi Note 10 Pro / SM7150 (Snapdragon 732G) | Qualcomm (modem-internal) | **USB Ethernet YES; WiFi NO** | README "What does not work (yet): … No Wi-Fi, Bluetooth, cellular, audio, camera, sensors…" (verified via fetched README). Commits `0075` "USB Ethernet (CDC-ECM) function and ue0 network interface", `0076` "receive USB Ethernet frames into one cluster mbuf", `0077` "live console log over USB Ethernet (UDP to 192.168.7.1:5140)", `0081` "ue0 reports an active Ethernet link (SIOCGIFMEDIA, KEV_DL_LINK_ON)". Search-snippet notes: "USB Ethernet works, the phone answers ping"; "notes: USB device mode enumerates on the host" |
| `MakrSas/ios-topaz` | Xiaomi Redmi Note 12 4G / SM6225 (Snapdragon 685) | Qualcomm (modem WLFW) | **WiFi YES (WPA2-PSK), fully usable incl. Safari HTTPS** | README: "Wi-Fi: `TopazModem` (patches 0113–0130): modem PAS boot through TrustZone from `modem_a`, GLINK/QRTR, pd-mapper/tftp/rmtfs, WLFW, copy engines, HTC/WMI/HTT, scan, association, WPA2-PSK 4-way handshake in the kernel, Ethernet data path. iOS sees it through `wifid`"; "iOS Settings lists networks, joins WPA2 networks, remembers the password in the keychain and reconnects after a reboot; Safari loads pages over HTTPS" |
| `joranv1/ios-sailfish` | Google Pixel 1 / MSM8996 (Snapdragon 821) | Qualcomm QCA6174 over PCIe | **WiFi YES (WPA2-PSK); wifid/configd/Settings drive it** | README: "Wi-Fi: QCA6174 over PCIe: own driver for firmware download (BMI), copy engines, HTC/WMI/HTT, scan, WPA2-PSK (CCMP) with our own 4-way and group-key handshake, rejoin after the AP drops us, interrupt-driven data path. To iOS it is an Apple80211 interface (`en0`), so wifid, configd and the Settings page drive it as on an iPhone" |
| `oosexgang/hakios-xnu-kernel` (+ `hakios-sm7325`, docs) | POCO X5 Pro 5G / SM7325 (Snapdragon 778G) | Qualcomm WCN6750 | **WiFi YES** (Settings Wi-Fi page, join networks, Safari) | Drivers doc: "`HAKIOS_WIFI` Wi-Fi of the WCN6750 (`wifi=1`, `wifi_step=N`): the DSP that runs the radio's firmware, Linux's ath11k (its object, made by tools/mk-ath11k.sh, is linked in only with this), a station, and the Apple80211 surface Darwin's wifid talks to. Needs fw-wifi/ from the phone". Network review doc: "`drivers/wifi/hakios.apple80211.c`: the Apple80211 ioctl/event surface for `wifid`" |
| `danstronics/ios-s9plus-exynos` | Samsung S9+ / Exynos 9810 (Mali-G72) | Broadcom (on-SoC/shared) | **WiFi PARTIAL** — association, DHCP, gateway ping, DNS, SSH verified in dev sessions; reliable reconnect across boots not finished | README: "Broadcom Wi-Fi bring-up: association, DHCP, gateway traffic, DNS and SSH have been verified in development sessions. Reliable reconnection across boots remains unfinished." Device table: "Wi-Fi: Association, DHCP, gateway ping, DNS and SSH verified in development sessions" |
| `ry-diffusion/Bergamota` | POCO F1 / SDM845 | Qualcomm Adreno (CPU only) | **USB CDC-ECM YES; WiFi NO** | README: "USB CDC-ECM Ethernet: SSH as root, live kernel log, Mac internet sharing"; "Not yet: Wi-Fi, cellular, Bluetooth, audio, camera, sensors, SEP/Face ID, GPU acceleration" |
| `Floppikrob/ios-galaxy-s8` (dreamlte) | Galaxy S8 / Exynos 8895 | Samsung SCSC (Linux driver) | **No WiFi or network claim found** (lock screen only) | README states "iOS 16.7 on S8 … reaches the iOS lock screen"; no network/Wi-Fi line in fetched README; no README mention of wifid/en0 |
| `UniXNU/UniXNU-sheng` (Pad 6S Pro) | Xiaomi Pad 6S Pro / SM8550 | Qualcomm Adreno 740 | **Claimed WiFi via `Ath12kWifi`** ("supplies the Wi-Fi stack through board PCIe, MSI, DMA, and network interfaces") — no verified device photo/log found in fetched doc | UniXNU-sheng README (claim/mixture) |
| `UniXNU-marble`, `UniXNU-mondrian`, `ios_sagit`, `xnu-xzs` | various | various | **No network claim found** | READMEs list "Wi-Fi … not implemented" (marble) / no WiFi line. Dropped from detail |
| `georgev-6974/s9plus-ios` | — | — | **Could not verify this repo exists** | GitHub search by name returned `danstronics/ios-s9plus-exynos`. Dropped — see §7 |

### 2.2 What "working" means here (explicit distinctions)

- **ios-sweet**: `wifid` and other hardware daemons start and fail/relaunch (no interface). The *only* network path is **USB Ethernet** (`ue0`). Evidence quote: "No Wi-Fi, Bluetooth, cellular, audio, camera, sensors, GPS or SEP / Face ID. Daemons for them start and fail or keep relaunching." Verdict: **starts `wifid` with no interface → not traffic**.
- **HakiOS/topaz/sailfish**: an `en0` interface exists, `wifid` is happy, Settings joins WPA2, Safari loads HTTPS. Verdict: **interface + association + traffic**. Limitations stated: WPA2-PSK only, no WPA3/enterprise/open (topaz/sailfish), no power save (topaz), no MAC randomisation.
- **danstronics**: association + DHCP + gateway + DNS + SSH observed, but "Reliable reconnection across boots remains unfinished." This is the closest **non-qualcomm** (Exynos/Broadcom) datapoint and it is still dev-session, not day-to-day.

---

## 3. What exactly is the Apple-side surface? (iOS 16)

Sources: BlackHat USA 2020 "Dive into Apple IO80211FamilyV2" (fetched PDF text), NewOSXBook "11208elppA" Parts I/II, OpenIntelWireless `apple80211_ioctl.h`, HakiOS `drivers/wifi/hakios.apple80211.c`, acidanthera `MacKernelSDK` PR #13.

### 3.1 IO80211Family V1 vs V2 (and who ships them)

- **V1** "mainly designed to support Apple Airport and related equipment": "Family drivers: IO80211Family, IONetworkingFamily. Plugin drivers: AirPortBrcmNIC, AirPortBrcm4360 / 4331, AirPortAtheros40 … Low-level drivers: IOPCIFamily." (verified, BlackHat slide text).
- **V2** "Starting from iOS 13 and macOS 10.15 Catalina, Apple refactored the architecture of the 80211 Wi-Fi client drivers and renamed the new generation design to IO80211FamilyV2." "IO80211FamilyV2 is mainly designed for communication and data sharing between new generation mobile-based Apple devices." "Family drivers: IO80211FamilyV2, IONetworkingFamily. Plugin drivers: AppleBCMWLANCore replaces AirPort Brcm series drivers. Low-level drivers: AppleBCMWLANBusInterfacePCIe. Firmware: BCMWLANFirmware4355 / 4364 / 4377 / 4378." "New subsystems are supported, such as Skywalk." (verified).
- Conclusion (inference): on iOS 16 our device is in the V2 era. `IO80211Family`/`IO80211FamilyV2` and `IOSkywalkFamily` ship **in the boot KC / System volume** (prebuilt Apple kexts), as do `AppleBCMWLANCore/PCIe/BusInterface`. Our *own* port never uses these plugin drivers because they only know Apple/Broadcom silicon.

### 3.2 The real user↔kernel boundary on iOS: SIOCG/SIOCSA80211 + KEV (not IO80211Controller vtables)

This is the single most important finding. Ports that "work" do not subclass `IO80211Controller`. HakiOS's `drivers/wifi/hakios.apple80211.c` (verified, fetched full file) shows:

- `#define SIOCSA80211 0x802869c8u /* _IOW('i', 200, struct apple80211req) */`
- `#define SIOCGA80211 0xc02869c9u /* _IOWR('i', 201, struct apple80211req) */`
- `struct apple80211req { char req_if_name[16]; int32_t req_type; int32_t req_val; uint32_t req_len; user_addr_t req_data; };`
- `ioctls` arrive via the socket layer; XNU "hands an interface ioctl it does not know to the interface", so they arrive in `hakios.net.c`, which forwards to `hakios.apple80211.c` "when its link is a radio."
- Events to userspace: "kernel events of class 6 (IEEE 802.11), subclass 1, whose code is Apple80211's own event number and whose data begins with the interface's name in 16 bytes (IO80211.framework reads them from a `PF_SYSTEM` socket and compares the name)."
- `struct kev_msg message = { .vendor_code = KEV_VENDOR_APPLE, .kev_class = KEV_IEEE80211_CLASS, .kev_subclass = 1, .event_code = code };` with `dv[0]` carrying the 16-byte interface name and `dv[1]` the event payload.
- Apple80211 event constants used: `EVENT_POWER_CHANGED = 1, EVENT_SSID_CHANGED = 2, EVENT_BSSID_CHANGED = 3, EVENT_LINK_CHANGED = 4, EVENT_ASSOC_DONE = 9, EVENT_SCAN_DONE = 10`. Join result codes returned via `ASSOCIATE_RESULT (21)`: "1 success, 21 'invalid PMK' (a wrong passphrase), 9 a timeout, 2 anything else." (verified quotes).
- Request type vocabulary published there (`SSID=1 … ASSOCIATE=20 … SCAN_REQ=10 … SUPPORTED_CHANNELS=27 … VIRTUAL_IF_CREATE=94 …`) and struct layouts asserted with `_Static_assert` against sizes read out of iOS 16.5's `IO80211.framework` (`apple80211_assoc` is 592 bytes, `apple80211_scan_result` 1164 bytes, `apple80211_channels` 4824 bytes). This file is the single best public reference for our apple80211 glue.

Supporting historical evidence (NewOSXBook, verified): "The underlying functionality of 802.11 in iOS has been moved into `/usr/sbin/wifid` (which likely just compiles it in statically), with new wrappers in the (also private) `MobileWiFi.framework`." OS X uses `CoreWiFi.framework`. "No matter where the functionality for Wifi is packaged … the true functionality rests in the kernel. Apple80211.framework is merely a user mode front end for the Apple80211Family.kext." Entitlement required (verified quote): `com.apple.wlan.authentication` (embedded in code signature, checked by AMFI).

OpenIntelWireless `itlwm` headers (verified) give the same ioctl numbers for 10.15+: `SIOCSA80211 = 2150656456`, `SIOCGA80211 = 3224398281`, and `apple80211req` with `req_if_name[IFNAMSIZ=16]`. These are macOS V2 numbers; iOS bytes differ in places (HakiOS reverse-engineered iOS 16.5 layouts). — **inference**: do not copy macOS struct layouts blindly; verify against iOS's `IO80211`/`wifid` at link/run time, exactly as our `PORT-ATTEMPT.md` §2.4 says.

### 3.3 Kernel pieces present on our device (IPSW/rootfs) vs Apple-silicon-only

Present and usable without writing them (iOS root image side; from the iOS 16 root content we already flash, `IOSRoot.sparse.img`):
- `wifid`, `airportd`, `sharingd`/`networkd`/`configd`, `MobileWiFi.framework`/`CoreWiFi`, `IO80211(Private)Framework` (NewOSXBook: `IO80211` symbols live on macOS; on iOS the parser is in `wifid`/`WiFiManager`). `configd`/IPConfiguration can lease addresses over any en interface (HakiOS device doc: "configd names the interface en0 and IPConfiguration leases an address over DHCP" for virtio-net). So a wired or shimmed interface does satisfy generic IP config.
- `SpringBoard`/Settings Wi-Fi page expects the Apple80211 ioctl vocabulary; without a driver answering it, Settings shows no networks (ios-sweet behavior).

Apple silicon-specific, **cannot** drive Samsung SCSC (do not try):
- `AppleBCMWLANCore`, `AppleBCMWLANPCIe`, `AppleBCMWLANBusInterfacePCIe`, firmware `BCMWLANFirmware4355/4364/4377/4378` (Broadcom). None of these are our chip. `AirPortBrcmNIC`/`AirPortAtheros40`/`AirPortItlwm`/`ATH9KFixup` are macOS hackintosh kexts and require AMFI/code-signing bypasses (`launchd_unsecure_cache=1`, `cs_enforcement_disable=1`) that these ports already require for everything, plus `IO80211Family` ABI matching. **Label: not portable to our permissive-but-in-kernel build without recompiling; mac hacks are not iOS precedent.**

### 3.4 IOKit/IO80211Controller class — actually optional

`IO80211Controller`/`IO80211VirtualInterface`/`IO80211SkywalkInterface` exist (community headers: itlwm `IO80211Controller.h`, MacKernelSDK PR #13 partial; BlackHat V2 is IOKit-based on the Apple silicon path). But the HakiOS working WiFi port **does not use them at all** (verified: no IO80211 kext references in `hakios.apple80211.c`; it is a plain socket-ioctl + kernel-event implementation over an `ifnet`). This is a strong precedent: on a port where everything is compiled into the kernel, **emulating the Apple80211 boundary at the ioctl/KEV level is enough for `wifid`.** The `PORT-ATTEMPT.md` §2 skeleton (subclassing `IO80211Controller`) is therefore one option, not the only one, and it is the riskier of the two against vtable drift.

---

## 4. Has anyone ever driven a non-Apple WiFi chip through IO80211 / wifid?

**Yes — on iOS, on non-Apple silicon, on Qualcomm radios.** Three independent ports demonstrate interface + association + traffic:

1. **HakiOS (WCN6750, Snapdragon 778G)** — driver files `hakios.wifi.c`, `hakios.wlfw.c`, `hakios.lx.c` (Linux ath11k object converted), `hakios.sta.c`, `hakios.apple80211.c`, `hakios.wlan.cpp`; "the Apple80211 surface Darwin's wifid talks to." Verified worked: Settings Wi-Fi page, join networks, Safari. Source evidence `hakios.apple80211.c` (fetched).
2. **joranv1/ios-sailfish (QCA6174 over PCIe)** — "To iOS it is an Apple80211 interface (`en0`), so wifid, configd and the Settings page drive it as on an iPhone" (verified via repo README snippet). WPA2-PSK CCMP with in-kernel 4-way handshake.
3. **MakrSas/ios-topaz (Qualcomm WCN via modem FWL)** — "iOS sees it through `wifid`"; Settings lists/joins/reconnects, Safari HTTPS (verified).

Non-qualcomm (Exynos): **danstronics/ios-s9plus-exynos** — Broadcom association + DHCP + gateway + DNS + SSH verified in dev sessions; chip shares family with Apple's, but his runtime is HakiOS-based and the Apple-side boundary is the same Apple80211 ioctl one (inference from HakiOS lineage; danstronics README does not name the glue, so label claim).

**Hackintosh comparison (macOS, not iOS — different ABI, label clearly):**

| Project | Chip | Target family | Reusability on iOS 16 |
|---|---|---|---|
| `OpenIntelWireless/itlwm` | Intel | `IOEthernet` (default, stable); `AirportItlwm.kext` optionally uses Apple's `IO80211` (less stable, enables Continuity) | Demonstrates a non-Apple radio exposing Apple80211 *requests*. But it is a macOS kext against the macOS `IO80211Family`; on iOS the boundary is the lighter SIOCG/SIOCSA80211+KEV shape (HakiOS proves sufficiency). Blockers on iOS: (a) code signing/kext-load policy — ports compile in-tree instead of kexts; (b) version-private ABI — request type numbers/layout changed between versions; (c) userspace entitlements — irrelevant in our permissive model. itlwm docs (verified): "itlwm.kext uses Apple's IOEthernet … is more stable than AirportItlwm.kext … which uses Apple's IO80211 … but supports certain Continuity features." |
| `ATH9KFixup` + `AirPortAtheros40` | Atheros (V1 family) | Legacy `IO80211Family` (V1) on macOS | V1 family is not iOS 13+ path; mostly historical. |
| `AirPortBrcmNIC` + `AirportBrcmFixup`, OCLP root patches | Broadcom | `IO80211FamilyLegacy`/skywalk on macOS | Only works because the silicon is Apple's Broadcom family. Not relevant to SCSC. |
| `0xFireWolf/AppleBCMWLANCompanion` (BCMC) | Legacy Broadcom FullMAC (BCM4350/43602) on macOS Tahoe/Sequoia | `AppleBCMWLANCore`-compatible | Modern non-Apple-Broadcom revived via Apple family — again only the Broadcom family. Does not help Samsung. |

**Key distinction for our chip (inference):** SCSC is neither Apple/Broadcom nor Qualcomm. The Intel/Qualcomm ports show that the *Apple-side* boundary (Apple80211 requests + KEV events) is implementable for any radio; the *chip-side* transport cannot be reused across families. HakiOS and friends exist because ath11k/WCN bring-up knowledge and firmware tooling exist; for SCSC the equivalent does not.

---

## 5. Substitute paths (what ports used instead of WiFi)

### 5.1 USB Ethernet (CDC-ECM) — demonstrated on iOS/XNU

- **ios-sweet** commits (verified via fetched activity/commit listing): `0075` "USB Ethernet (CDC-ECM) function and ue0 network interface" (patch `59e7aec`), `0076` "receive USB Ethernet frames into one cluster mbuf" (`992cadb`), `0077` "live console log over USB Ethernet (UDP to 192.168.7.1:5140)" (`5029e91`), `0081` "ue0 reports an active Ethernet link (SIOCGIFMEDIA, KEV_DL_LINK_ON)" (`fdad0de`). Search-snippet quote: "notes: USB Ethernet works, the phone answers ping" and "live kernel log over USB Ethernet". **Cost:** patches written in-house (ifnet attach, `mbuf` RX into one cluster, ether_input, link-state KEV), ~1-2 weeks; modem/USB side. Limitation: development link only; not subject to activation, and `wifid` still not satisfied.
- **Bergamota** (verified README): "USB CDC-ECM Ethernet: SSH as root, live kernel log, Mac internet sharing." Same pattern inherited from ios-sweet.
- **USB as log channel (our repo too):** our ground-truth `JACKPOTLTE_PROGRESS.md` shows the same legacy (serial over fb). Generic USB Ethernet is the cheapest useful network on these ports.

### 5.2 Host-side tethering / bridge tricks

- Ports share the Mac host's network via Internet Sharing over the USB Ethernet `en0`/`ue0` (Bergamota: "Mac internet sharing"). Standard OS, no kernel work. Topic-manager note (inference): this gives IP/HTTP/SSH, not cellular/WWAN identity.
- QEMU route (`alephsecurity/xnu-qemu-arm64`, HakiOS QEMU): virtio-net backed `en0` with DHCP. Not a phone result but same concept; label not applicable to physical radios.

### 5.3 Does iOS accept a non-WiFi interface for setup/activation?

- **configd/IPConfiguration** leases an address over DHCP on any Ethernet-ish interface (verified by UniXNU-droidvm quote: "configd names the interface en0 and IPConfiguration leases an address over DHCP"). For general daemons, an Ethernet `en0` is sufficient.
- **Setup/activation** in these ports: every port uses the **Hactivation** device-tree path (ios-sweet: "Native 'Hactivation' path of mobileactivationd, chosen through device-tree security status. No Apple servers involved."). Real activation would need Secure Enclave/SEP (absent) — `mobileactivationd` cannot generate the UIK (HakiOS note: "mobileactivationd inability to create the UIK (`0xe00002c7` unsupported)"). Thus: **no iOS port has ever activated a phone with a real network anywhere**; activation is always bypassed. So there is **no evidence** that Ethernet alone achieves real activation; and conversely no evidence that WiFi is strictly required, because activation never completes either way. The honest answer: **activation outcome is orthogonal to which interface exists.**
- **`wifid` itself**: runs and expects an 802.11 interface; without one it relaunch-loops on ios-sweet, which is acceptable but noisy. `sharingd`, `airportd` similarly loop. No port has gotten rid of these loops except by providing WiFi (HakiOS/topaz/sailfish reduce them because the radio answers).
- **`networkd`**: accepts wired interfaces.
- **iOS Simulator networking** is **not applicable to hardware**: the simulator reuses the macOS host network stack; it is not a model for device-side drivers. (Label: inference/common knowledge; not in fetched sources.)

---

## 6. Our situation (jackpotlte, Exynos 7885, Samsung SCSC/MX140)

- Chip side: scoped; only TizenRT portable reference; vendor firmware required. See `docs/research/wifi-chip-re-prior-art.md` and `scsc-firmware-locate.md`.
- Apple side: the working ports' **Apple80211 ioctl/KEV boundary code is public** (HakiOS) and the struct layouts for iOS 16.5 are documented with `_Static_assert`s. This removes most of the 6-12 week "reverse-engineer Apple's codec" risk for a new port, leaving adaptation (2-4 weeks) instead of invention.
- Critical decision: because we compile in-tree (permissive model), we can either (a) emulate the Apple80211 boundary the HakiOS way (recommended — matches how every working port does it), or (b) subclass `IO80211Controller` (higher vtable-drift risk on the first link). The skeleton in `docs/drafts/jackpotlte-wifi/PORT-ATTEMPT.md` §5 chose (b); consider (a).
- The Exynos S9+ port shows even a Broadcom-ish Exynos radio reaches "association + DHCP + DNS + SSH" with a HakiOS-derived port, so **interoperating with `wifid` is clearly feasible on Exynos** if the transport runs. The blocking piece for us is Exynos-7885 SCSC transport, not Apple glue.

---

## 7. Blunt verdict and effort

For a port with no Apple/Broadcom/Qualcomm radio, chip is Samsung SCSC:

- **(a) Any interface up:** ~2-4 engineer-weeks if we take the USB Ethernet path (we even have the commit trail). Already demonstrated on this board family's cousins; trivially adaptable.
- **(b) `wifid` happy:** ~4-8 weeks for the Apple-side emulation (adapt HakiOS `hakios.apple80211.c` + struct layouts) **plus** the transport. Realistic only after MX core is scoped.
- **(c) Actual association + traffic on SCSC:** only after transport + firmware land. Transport 4-8 weeks (per `wifi-chip-re-prior-art.md` TizenRT opportunity), MX scoping 2-6 weeks, firmware procurement 1-2 weeks. **Total 12-20 engineer-weeks from a standing start**, same order as `PORT-ATTEMPT.md` §7.1, but with the Apple-glue worst case removed by prior art.
- **Which half dominates:** **chip/transport dominates.** Apple-side IO80211 glue is now "known how" (HakiOS/sailfish/topaz public code). The Exynos 7885 MIF aperture, AxDMA, recovery, and signed/unsigned firmware questions are the true unknowns.
- **Can USB Ethernet substitute for setup/activation?** It cannot make activation "real" (no port achieves real activation; all use Hactivation). It **can** give functional IP for development and many apps, and is the rational default if SCSC firmware/transport stalls (see kill criteria in `PORT-ATTEMPT.md` §6.1). Keep `redwoodwifi=1` gated either way.

---

## 8. Items not verified (drop rather than guess)

- `georgev-6974/s9plus-ios` — I could not confirm this repo exists. GitHub search by that name returned `danstronics/ios-s9plus-exynos`. Not cited.
- Floppikrob `ios-galaxy-s8` (dreamlte): no network/Wi-Fi/USB-Ethernet state is stated in the fetched README; I found no commit titles or issues claiming any interface. Treated as "no network demonstrated."
- UniXNU `mondrian`/`marble`/`MC-XiaoXiao/ios_sagit`/`lechaukha12/xnu-xzs` WiFi state: only `marble` README explicitly lists "Wi-Fi … not implemented". UniXNU-sheng/S (Pad 6S Pro) *claims* `Ath12kWifi` works but I found no device photos/logs verifying association or traffic; label claim/mixture, not verified.
- iOS Simulator as a model: not in any fetched source; described from general knowledge as an analogy only (§5.3 above).
- danstronics `ios-s9plus-exynos` Apple-side glue: README does not name the boundary (IO80211 vs Apple80211-ioctl). Labelled inference from HakiOS lineage.
- No phone was touched; no log/screenshot from our device exists for any network claim.

---

## 9. Source list (auditable URLs)

Ports / primary sources:
- `dipa-studio/ios-sweet` README and commit activity: https://github.com/dipa-studio/ios-sweet (incl. commits `59e7aec`, `992cadb`, `5029e91`, `fdad0de`; fetched via search).
- `MakrSas/ios-topaz` README: https://github.com/MakrSas/ios-topaz (fetched via search; full README snippet).
- `joranv1/ios-sailfish` README: https://github.com/joranv1/ios-sailfish (fetched via search).
- `oosexgang/hakios-xnu-kernel` + drivers doc: https://github.com/oosexgang/hakios-xnu-kernel ; https://hackios-web.vercel.app/en/docs/reference/drivers ; `drivers/wifi/hakios.apple80211.c` fetched in full.
- `danstronics/ios-s9plus-exynos` README: https://github.com/danstronics/ios-s9plus-exynos (fetched).
- `ry-diffusion/Bergamota` README: https://github.com/ry-diffusion/Bergamota (fetched via search snippet).
- `Floppikrob/ios-galaxy-s8` README: https://github.com/Floppikrob/ios-galaxy-s8 (fetched, no network claim).
- `UniXNU/UniXNU-sheng`, `UniXNU-marble`: https://github.com/UniXNU/UniXNU-sheng , https://github.com/MiLacy/UniXNU-marble (fetched snippets).
- `UniXNU-droidvm` network description: https://github.com/Droid-VM/UniXNU-droidvm (fetched).

Apple-side surface / Hackintosh:
- BlackHat USA 2020, "Dive into Apple IO80211FamilyV2" (Wang Yu): https://i.blackhat.com/USA-20/Thursday/us-20-Wang-Dive-into-Apple-IO80211FamilyV2.pdf (fetched via search, verbatim slides quoted).
- NewOSXBook, "11208elppA" / "11208elppA, Part II" (Jonathan Levin): http://newosxbook.com/articles/11208ellpA.html and http://newosxbook.com/articles/11208ellpA-II.html (fetched excerpts).
- OpenIntelWireless/itlwm `include/Airport/apple80211_ioctl.h`: https://github.com/OpenIntelWireless/itlwm/blob/master/include/Airport/apple80211_ioctl.h (fetched via search).
- AppleBCMWLANCompanion (0xFireWolf): https://github.com/0xFireWolf/AppleBCMWLANCompanion (fetched).
- IO80211Family headers / MacKernelSDK PR #13: https://github.com/acidanthera/MacKernelSDK/pull/13 (fetched).
- Hackintosh OCLP/wireless buyers guide (itlwm/Realtek/Atheros): https://dortania.github.io/Wireless-Buyers-Guide/Kext.html (fetched).

Local (read-only, for our context only, NOT treated as outside evidence):
- `/home/not_andrej/ios-jackpotlte/docs/research/wifi-chip-re-prior-art.md`
- `/home/not_andrej/ios-jackpotlte/docs/research/scsc-firmware-locate.md`
- `/home/not_andrej/ios-jackpotlte/docs/drafts/jackpotlte-wifi/PORT-ATTEMPT.md`
- `/home/not_andrej/ios-jackpotlte/JACKPOTLTE_PROGRESS.md`