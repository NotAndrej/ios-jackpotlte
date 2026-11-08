# WiFi proprietary chip stack — reverse-engineering prior art

**Question answered:** whether a portable, already-reversed reference implementation exists for Samsung's SCSC / Maxwell (MX140) transport, or for analogous FullMAC-over-shared-memory designs, and whether anything materially shortens a 12–24 engineer-week XNU port.

**Scope / evidence rules:** every external claim below is marked **VS** (verified source), **CL** (claim), or **IN** (inference). No build, device, CI, or git operation was performed for this document. Local references are limited to the two permitted roots; external facts are limited to public URLs fetched directly.

---

## 1. Verdict (short form)

- **SCSC / MX140:** **VS** Mainline Linux contains no `scsc`/`maxwell`/`mx140` wireless driver. GitHub public *repository* search for `SCSC_SERVICE_ID_WLAN` returned zero repositories (fetched 2026-11-08). `torvalds/linux` wireless subtree has no Samsung SCSC directory. A public search for equivalent terms in the vendor-kernel world turns up Samsung vendor kernels and Android HALs, not host drivers for another OS. **IN** The strongest statement supportable from public sources is therefore "no independently written, cross-OS SCSC/MX140 transport implementation is publicly known"; absence of evidence is not proof of absence.
- **TizenRT:** **VS** Samsung's TizenRT repository is public and licensed Apache-2.0. It contains a Samsung-maintained SCSC/MX140 implementation under `os/drivers/wireless/scsc/`. Across 118 source files, the tree has **zero** `#include <linux/...>` and **CL** TizenRT is therefore a genuine non-Linux reference for the *transport*. However, its MX core is still TinyAra-specific and its `platform_mif.c` is S5JT200-oriented; it also still consumes the same proprietary firmware blobs.
- **Analogous designs:** **CL** b43/brcmfmac/mt76/wcn36xx are valuable as *Linux host drivers* for different chips, but they are Linux/GPL/OS-bound and do not provide a reusable SCSC transport. NexMon patches *firmware*, not the host transport, so it is orthogonal to this port. Shared-memory mailbox reverse engineering (Qualcomm SMD/Glink, RPMSG) teaches IPC patterns, not the MX protocol.
- **Net effect:** **IN** The best available prior art (TizenRT) can realistically remove roughly 2–4 engineer-weeks of avoidable MMIO/mailbox/ring/doorbell guesswork and provides an Apache-2.0 reference for the exact firmware paths and CRC flow. It does not provide the Exynos 7885 platform register map, it does not remove the proprietary firmware requirement, and it does not cover Apple's `IO80211`/Skywalk glue. **The bulk of the 12–24-week estimate still stands.**

---

## 2. Search record (what was actually examined)

Directly fetched remote resources:

- `https://github.com/Samsung/TizenRT` — repository page and recursive git tree (118 files under `os/drivers/wireless/scsc/`, 49,143 lines total). **VS**
- `https://github.com/search?q=SCSC_SERVICE_ID_WLAN&type=repositories` — 0 repositories. **VS**
- `https://github.com/torvalds/linux/tree/master/drivers/net/wireless` — no `scsc` entry. **VS**
- `https://github.com/Samsung/TizenRT/tree/master/os/drivers/wireless/scsc` and `/misc` — file inventory and MX core location. **VS**
- Raw files fetched from TizenRT: `platform_mif.c`, `mifintrbit.c`, `scsc_mx_impl.c`, `mx140_file.c`, `mxman.c`, `utils_misc.h`, `Kconfig`, `misc/*` file list; license header. **VS**
- GitHub search used for `hal_samsung` found Samsung Android hardware HAL repos only (`LineageOS/.../android_hardware_samsung_slsi_scsc_wifibt_wifi_hal`, a userspace Android HAL), not a host OS transport. **VS**

Local resources inspected (permitted roots only):

- `/var/tmp/opencode-keep/Eureka-Kernel/{drivers/misc/samsung/scsc, drivers/net/wireless/scsc, include/scsc}` — Samsung Exynos S8-family vendor kernel. **VS**
- `/home/not_andrej/ios-jackpotlte` — project notes/drafts used for *file/function names and build history only*, not as evidence about outside projects.

Not performed: **no web-search engine results were obtainable during this task** (the search tool returned empty for every query, including trivial ones). Web claims are therefore based on fetched source-tree/GitHub pages, not on search-engine summaries. Patent databases and conference papers were not independently queried.

---

## 3. Part 1 — SCSC / MX140 prior art

### 3.1 What a "driver" in public is

- **VS** `torvalds/linux/drivers/net/wireless/` has no `scsc`, `maxwell`, or `mx140` directory (fetched directory listing). Samsung SCSC exists only in Samsung vendor kernels such as `samsungexynos7885/android_kernel_samsung_universal7885` (`drivers/net/wireless/scsc`, `drivers/misc/samsung/scsc`).
- **VS** Direct search for the canonical symbol `SCSC_SERVICE_ID_WLAN` in public GitHub repositories returned **0 results**.
- **CL** LineageOS ships `android_hardware_samsung_slsi_scsc_wifibt_wifi_hal` (found via the `hal_samsung` search). This is the Android userspace HAL above the kernel driver; it uses the same Maxwell transport, it is not a replacement transport, and it is Android-specific.
- **CL** postmarketOS uses the Samsung vendor kernel's SCSC driver; no mainline SCSC port is known. (This is reported in existing community material, but no independent mainline source exists either way.)

### 3.2 What is documented

- **VS** Samsung's own source tree (`drivers/net/wireless/scsc/hip4.h`) documents the host-interface geometry: six 256-entry queues (`HIP4_MIF_Q_FH_CTRL`, `FH_DAT`, `FH_RFB`, `TH_CTRL`, `TH_DAT`, `TH_RFB`), `HIP4_DAT_SLOTS = 218`, `HIP4_CTL_SLOTS = 32`, and v3/v4 config structures with magic `0xcaba0401` (init `0xcaaa0400`).
- **VS** `include/scsc/scsc_mx.h` documents the mailbox/service contract (`scsc_mx_service_open/start/stop/close`, `scsc_mx_service_mifram_alloc/free`, `scsc_mx_service_mif_addr_to_ptr/ptr_to_addr`, `scsc_service_mifintrbit_*`).
- **VS** TizenRT's `mx140_file.c` documents the firmware filesystem layout: `/vendor/firmware/mx140/fw/<variant>/mx140.bin` and `/vendor/firmware/mx140/fw/<variant>/conf/wlan/wlan.hcf` etc.
- **CL** No public write-up was found reverse engineering HIP4, MIF, or AxDMA independent of Samsung's source: no FOSDEM/ELCE-talk reference fetched, no Coreboot/NuttX/FreeBSD port, no Zephyr use of the transport. **IN** This absence may simply mean the pool of public work is small; it is not a proof of impossibility.

### 3.3 Verdict on Part 1

- **No independent clean-room reimplementation of the SCSC/MX140 transport is publicly known.**
- TizenRT is the only public code that reimplements the transport in a non-Android (indeed non-Linux) OS. It is Samsung-authored and licensed Apache-2.0, so it is a *reference port*, not a reverse-engineering project. See Part 2.
- **IN** Any "weeks saved" therefore comes from *having a second, OS-neutral implementation to cross-check against*, plus mailboxes/wire formats that can be copied, not from a drop-in driver.

---

## 4. Part 2 — TizenRT as a portable reference

### 4.1 Which files implement the MX core there

TizenRT does **not** keep the MX core in a separate `drivers/misc/samsung/scsc/` subtree; it is fused directly into the WLAN driver:

- **VS** Key transport/MX-core files under `os/drivers/wireless/scsc/misc/`:
  - `scsc_mx_impl.c` / `.h` — `struct scsc_mx`, `scsc_mx_create()`, `scsc_mx_destroy()`; binds MIF, interrupt manager, mailbox manager, RAM manager, `mxman`, service manager, transport, loggers, panic monitor, suspend monitor.
  - `platform_mif.c` / `.h` — MMIO platform/mif layer for the S5JT200-class chip: reserved shared memory, WLBT interrupts, reset/power, mailbox register window.
  - `mifintrbit.c` / `.h` — doorbell/bit manager (`mifintrbit_alloc_tohost`, `mifintrbit_alloc_fromhost`, `mifintrbit_bit_*`).
  - `miframman.c` / `.h` — shared-memory (`scsc_mifram_ref`) allocation and ptr-to-ref conversion.
  - `mifmboxman.c` / `.h` — mailbox allocation and pointer fetch.
  - `mifstream.c` / `.h` — stream read from firmware/GDB channels.
  - `mxman.c` / `.h` — firmware download, header parsing, CRC checks, recovery, reset.
  - `scsc_service.c` — `scsc_mx_service_open/start/stop/close` and service-failed path.
  - `mx_mmap.c` — debug character/map interface (optional).
  - `mxmgmt_transport.*`, `mxlog_transport.*`, `gdb_transport.*` — host↔firmware management channels.
  - `mx140_file.c` — filesystem-based firmware loader and firmware paths.
- **VS** The transport sits behind the same shared-memory abstraction at top level: `scsc_mif_abs.h`, plus wrappers `slsi_drv.c`, `dev.c`, `netif.c`, `cm_if.c`, `hip.c`, `hip4.c`.

### 4.2 License and reuse permission

- **VS** TizenRT root is `Apache-2.0` (fetched `LICENSE`, first lines: "Apache License, Version 2.0, January 2004"). File headers verified on `platform_mif.c`, `mifintrbit.c`, `scsc_mx_impl.c`, `mx140_file.c`, `mxman.c`: all contain the Apache-2.0 header and "Copyright 2017 Samsung Electronics All Rights Reserved."
- **IN/CL** Apache-2.0 is not copyleft: it permits a proprietary/non-open fork (including XNU KEXT use) subject to retaining copyright/license notices, providing a copy of the license, stating significant changes, and honoring the patent grant (Sections 3, 4, 6, 7). For a redistributed port this is materially better than lifting the Linux GPL Samsung driver. **Compatibility between Apache-2.0 and the XNU source tree's own license terms (APSL/BSD-ish) is outside my competence** — legal review required. No entitlement for "trademark" rights exists: "Samsung", "SCSC", "Maxwell", etc. remain Samsung's trademarks. Do not use them as product branding.
- **CL** Apache-2.0 `patent grant` is a benefit: Samsung grants a (conditional) patent license to the code's contribution recipients. This is arguably the only *legal* advantage of TizenRT over a clean room for Samsung-held patents; it is no help against unrelated third-party patents.

### 4.3 Size — measured, not estimated

Remote fetch + line count, 2026-11-08:

| Scope | Files | Lines |
|---|---:|---:|
| TizenRT entire `os/drivers/wireless/scsc/` | 118 | **49,143** |
| TizenRT MX/transport `scsc/misc/` | 49 | **8,260** |
| Local vendor (Eureka, allowed root), entire `drivers/misc/samsung/scsc/` | 100+ | **25,964** |
| Comparable 10 MX transport files in local vendor | 10 | **6,800** |
| Same 10 files in TizenRT | 10 | **3,998** |

Comparable set: `mifintrbit`, `miframman`, `mifmboxman`, `mifstream`, `mx140_file`, `mxman`, `platform_mif`, `scsc_mx_impl`, `scsc_service`, `mx_mmap` C sources (plus headers).

**CL** The transport is ~**41% smaller** than the local vendor equivalent (3,998 vs 6,800 lines) — inside the "≥30% smaller" threshold considered valuable. Two reasons matter: (a) many Linux/Samsung vendor-only diagnostics/procfs/extra features are removed; (b) the S5JT200 platform map is simpler than the S5E7885 8-core platform map. **IN** Do not read the 41% as "41% less engineering work"; the removed code is largely debug, procfs, and extra feature layering. The core sequence and struct definitions are almost unchanged.

### 4.4 Linux dependence (or lack of it)

- **VS** Across all 118 files in TizenRT `os/drivers/wireless/scsc/`, there are **zero** `#include <linux/...>` headers.
- **VS** The same set includes 23 `tinyara/*` headers: `arch.h`, `clock.h`, `irq.h`, `kmalloc.h`, `kthread.h`, `mm/mm.h`, `module.h`, `wqueue.h`, `watchdog/wdog.h`, `fs/*`, etc.
- **VS** There are zero uses of `regmap`, `sk_buff`, `NAPI`, or `request_firmware` in the MX core. Top-level wrapper files use RTOS equivalents, e.g. `struct netdev`, `wpa_ctrl_open`, and TinyAra `work_queue`.
- **VS** Embedded-contract OS primitives used in the MX core (counts in `misc/*.c`):
  - `spin_lock_enter_critical_section` / `spin_unlock_leave_critical_section` are defined as critical-section macros, not Linux spinlocks.
  - TinyAra `irq.h`, `kmalloc.h`, `wqueue.h`, `wdog.h` for interrupts, heap, workqueue, watchdogs.
  - `pthread_*`, `sem_*` mutex/semaphore families (often logical, in-wlan daemon/client code).
- **IN** Translation to XNU is straightforward structurally but not mechanical: Linux's `spinlock_t`/IRQ semantics differ from TinyAra's critical-section macros, and XNU would map these to `IOLock`/`IOInterruptEvent`/`IOWorkLoop`/command gates. The register/doorbell logic is native to Samsung code and can be copied as-in once those wrappers are replaced.

### 4.5 Firmware dependencies — TizenRT does **not** remove these

- **VS** `mx140_file.c` defines:
  - `MX140_FW_BASE_DIR = "/vendor/firmware/mx140/fw"`
  - core image path `/vendor/firmware/mx140/fw/<variant>/mx140.bin` (also `.../<variant>/full-service` legacy variant); `mx140_file_download_fw()` performs `memcpy(dest, firm->data, firm->size)`.
  - config path `/vendor/firmware/mx140/fw/<variant>/conf/<rel>` (so `wlan.hcf` ends up under `.../conf/wlan/wlan.hcf`).
- **VS** The firmware is still a Samsung proprietary binary + MIB tables; TizenRT does not document it or provide source. That alone blocks any "no firmware needed" strategy.
- **VS** Critical caveat in the inspected revision: `mx140_request_file()` allocates a `struct firmware *` and returns success **without visibly opening or reading `path`**; file I/O is either performed elsewhere or left incomplete in this revision. **CL** TizenRT is therefore useful as the canonical statement of *what paths and buffers are needed*, but not as a proven complete loader. Do not assume the file-loading call sequence works unmodified.

### 4.6 Platform mismatch to be aware of

- **VS** TizenRT's `platform_mif.c` guards its register definitions with `#ifdef CONFIG_ARCH_CHIP_S5JT200` (S5JT200-focused). It does **not** contain an S5E7885/Exynos 7885 MIF aperture/register definition in the inspected file.
- **VS** The local vendor tree *does* contain `platform_mif.h`/`platform_mif.c` for the S5E7885 and `mif_reg_S5E7885.h`. **IN** TizenRT is therefore the best available reference for MX protocol behavior, but the Exynos 7885 MMIO register map, shared-memory aperture, and reserved-memory setup must still be taken from the local vendor tree. You cannot blindly compile TizenRT's `platform_mif.c` for `jackpotlte`.

### 4.7 Answer to the specific question

- **TizenRT's MX layer is approximately 41% smaller than the corresponding local vendor transport, is free of Linux idioms, is Apache-2.0 licensed, and implements the same MX service/mailbox/interrupt/ring/firmware-download core.** **CL** It is the single most useful prior art found.
- **But** it still needs the same proprietary firmware, its platform layer is for S5JT200 rather than Exynos 7885, and its client/network layer is TinyAra-specific. **IN** It converts the "reimplement from scratch" problem into a "port an existing, smaller, OS-decoupled reference implementation" problem; it does not erase the Apple-side IO80211 and firmware-root problems.

---

## 5. Part 3 — Analogous successes and what made them portable

Each is labelled for host-transport relevance, not driver usability in general.

### 5.1 MediaTek `mt76`

- **CL** A mainline Linux driver family covering MT7601U, MT7603/7610/7615, MT7663, MT7915 and later CONNAC-era chips over USB/PCIe/SDIO. It is ordinary mainline Linux code, not a second OS or reverse engineering project in the transport sense.
- **IN** It is not a portable shared-memory FullMAC reference in the MX sense; the Linux driver owns bus enumeration, DMA, mac80211, cfg80211, and NAPI. The MT79xx family does use internal co-processors and shared firmware, but that host transport is wrapped in the Linux PCIe driver and cannot be lifted to XNU (GPL and OS-local).
- **CL/IN** Some of its value here is as architecture folklore: it shows that multi-chip FullMAC driver families can be enormous (tens of thousands of lines) and still be merged upstream, but it offers little to the *XNU transport* task. License: GPL v2 (not reusable in a proprietary XNU port without a clean-room rewrite).

### 5.2 `b43` / `bcma` / `openfwwf`

- **CL** `b43` (and `b43legacy`) is a GPL Linux driver for Broadcom 802.11a/b/g/n using `ssb`/`bcma` on PCI/SoC buses. `openfwwf` (academic/open project) provides open firmware for some older Broadcom chips.
- **CL** This is genuine reverse engineering of Broadcom firmware; however, it is bus-controller code with heavy Linux and mac80211 coupling, and `openfwwf` is only relevant to chips where an open firmware exists. It does not help with shared-memory doorbell IPC and is not portable to XNU.
- **IN** Closest lesson: reverse-engineering a FullMAC vendor stack is a multi-year project with its own firmware; it is only worth doing for very old, very popular chips. Not applicable to MX140's proprietary firmware.

### 5.3 `brcmfmac` + NexMon

- **VS/CL** `brcmfmac` is the mainline Linux driver for Broadcom FullMAC devices (SDIO/USB/PCIe). NexMon is a separate project that patches Broadcom firmware to add monitor-mode features.
- **CL** NexMon patches the *chip-side firmware*, not the host transport. It does not give you a host driver; it presupposes one. It is evidence that firmware patching and instrumenting a vendor stack is a real technique, but it is orthogonal to porting a host driver to XNU. It also does not remove the need for firmware binaries or regulatory compliance. **IN** Irrelevant to shortening this port.

### 5.4 Qualcomm WCNSS / "prima" (`wcn36xx`, `ath6kl`)

- **CL** `ath6kl` (mainline) supports Atheros AR600x over SDIO/USB. `wcn36xx` (downstream/mainline) supports the Qualcomm WCN36xx core and is built around shared-memory/RPMSG/SMD-style IPC to the wireless co-processor, not a plain SDIO link.
- **CL/IN** These are community drivers but they are still GPL, Linux-only, and based on Qualcomm's internal protocol. They are the closest historical analogy to "FullMAC over shared memory," but:
  - the transport source is still tied to Linux `remoteproc`/`rpmsg` and shared-memory banks;
  - it took years of community work (Qualcomm providing code, then incremental mainlining spanning 2011–2018 era for SMD/Glink infrastructure) rather than an independent reverse-engineering in months;
  - it is not a portable reference.
- **CL** Recon/zephyr porting the low-level resource table (`qcom_smd`, `qcom_glink_native`) is a useful teaching ground for the *style* of IPC, but none of it is SCSC.

### 5.5 Mailbox / shared-memory IPC precedent

- **VS** Linux has canonical, public implementations of the **pattern**: `drivers/rpmsg/rpmsg_char.c`, `drivers/soc/qcom/smem.c`, `qcom_smd.c`, `qcom_glink_native.c`, `qcom_glink_smem.c`, and `drivers/remoteproc`. These document a host/co-processor IPC with shared memory, circular buffers, and doorbells.
- **CL** MediaTek's `conninfra`/`wlan` families use a similar application-processor + connectivity-processor shared-memory bridge; most public references are Android vendor kernels, not independent rewrites.
- **IN** These are conceptually the same class but not the *protocol*: they cannot tell you MX140 mailbox layout, firmware header format, or MIF register meaning. Do not budget any real savings from them; their main value is architectural confidence that "shared-memory doorbells + rings" is a solvable, well-understood systems problem.

### 5.6 Projects that were mentioned as "portable references" and checked

- **Zephyr `hal_samsung` / TizenRT / NuttX / Tizen / FreeBSD / Coreboot:** **VS** public repo search (`hal_samsung`) returned unrelated Samsung hardware work; no SCSC MX transport. **CL** Claims below: Zephyr has no MX140 wireless stack known; NuttX has no Samsung SCSC driver; Coreboot/FreeBSD do not either. The only one with actual SCSC code is TizenRT.
- **Samsung TizenRT:** see Part 2.

---

## 6. Part 4 — Transferability, per piece of the MX core (uses local function names)

Labels: **Transfers** directly; **Adapt** (structure/sequence applies but API/policy changes); **No** (must be written to XNU/Apple constraints; outside prior art).

| # | MX core piece | Local vendor evidence (`Eureka-Kernel`) | TizenRT/other prior art | Transferability |
|---|---|---|---|---|
| 1 | Shared DRAM allocation & pointer translation | `scsc_mx_service_mifram_alloc/free`, `scsc_mx_service_mif_addr_to_ptr`, `_mif_addr_to_phys`, `_mif_ptr_to_addr` (`scsc_mx.h`) | `miframman.c` (TizenRT) | **Transfers** — keep exact ref semantics, just change allocator |
| 2 | Mailbox alloc/get | `scsc_mx_service_alloc_mboxes`, `scsc_service_free_mboxes`, `scsc_mx_service_get_mbox_ptr` | `mifmboxman.c` | **Transfers** |
| 3 | Doorbell bit set/clear/get/mask/unmask | `scsc_service_mifintrbit_bit_*`, `..._register_tohost/unregister_tohost`, `..._alloc_fromhost/free_fromhost`, `..._get` | `mifintrbit.c` | **Transfers structurally / Adapt** delivery: XNU interrupt handler + `IOWorkLoop` instead of Linux/TizenRT IRQ critical sections |
| 4 | MIF platform power/reset/clock | `platform_mif_power`, `platform_mif_hold_reset`, `platform_mif_start`, `platform_mif_pmu_reset`, `_pmu_reset_release`, `platform_mif_reset` (local S5E7885 implementation) | `platform_mif.c` exists in TizenRT but S5JT200-focused | **Adapt** — logic transfers, but Exynos 7885 MMIO/register layout must come from the local tree, not TizenRT |
| 5 | MX service + lifecycle | `scsc_mx_service_open/start/stop/close`, `scsc_mx_service_service_failed`; orchestration `slsi_sm_wlan_service_open/start` (`cm_if.c`) | `scsc_service.c` | **Transfers** sequence; MX-file driver must be ported |
| 6 | HIP4 config structs/magic/queue constants | `hip4_hip_config_version_3/4` (`0xcaba0401`, init `0xcaaa0400`), 6 queues at `q_len = 256`, `HIP4_DAT_SLOTS = 218`, `HIP4_CTL_SLOTS = 32` (`hip4.h`) | Header ports look structurally identical in TizenRT | **Transfers byte-for-byte** — copy exactly |
| 7 | HIP4 init/rings/scoreboard population | `hip4_init` (`hip4.c:1237`) (interrupt registration at `:1263`) | TizenRT `hip4.c` (~965 lines) | **Adapt** — ~90% logic; replace workqueue/timer/procfs |
| 8 | HIP4 setup/start/stop/deinit | `hip4_setup`, `slsi_hip_start`, `slsi_hip_stop`, `hip4_deinit` (`hip4.c` ~`1625`/`1737`) | TizenRT `hip4.c` | **Adapt** — mutex/IRQ/lock API changes |
| 9 | TX transport (frame → rings) | `scsc_wifi_transmit_frame` (`hip4.c:1525`), `hip4_skb_to_mbulk` (`hip4.c:452`) | mbulk/RQ logic transfers from TizenRT | **Adapt** — mbulk geometry transfers; `sk_buff` conversion (`hip4_skb_to_mbulk`) becomes `mbuf_t`/`IOMemoryDescriptor` |
| 10 | RX transport (interrupt → signal) | `hip4_irq_handler` (`hip4.c:1174`), `hip4_tasklet`/`hip4_wq` (`hip4.c:838`/`840`), `hip4_mbulk_to_skb` (`hip4.c:607`) | TizenRT equivalents | **Adapt** — bottom-half scheduling and `mbuf` conversion rewrite |
| 11 | Watchdog | `hip4_watchdog` (`hip4.c:745`) | watchdog in TizenRT | **Adapt** (timer API only) |
| 12 | Suspend/resume/freeze | `hip4_suspend/resume/freeze` (`hip4.c:1678-1736`) | TizenRT | **Transfers** (policy changes) |
| 13 | MIB framing/hash | `slsi_mib_encode_get_list`, `slsi_mib_decode_get_list`, `slsi_mib_find` (`mib.c`), load calls in `mgt.c` ~`1240+` | TizenRT mib code | **Transfers** framing; loader handles change |
| 14 | Firmware load/state machine | `mx140_file_download_fw`, `mxman_open/start`, `mx140_request_file` etc. in `mxman.c`/`mx140_file.c`, plus `fwhdr_init`/`fw_crc32_checks` | `mx140_file.c` + `mxman.c` | **Adapt** — sequence/CRC transfer; packaging/filesystem adaptation remain, blobs still mandatory |
| 15 | Dev/client attach shell | `slsi_dev_attach` (`dev.c:269`), `slsi_hip_init`, connection data path in `rx.c`/`tx.c` | TizenRT client + RTOS net | **Adapt** as transport client only |
| 16 | cfg80211 ops (`slsi_*` 40 entries in `slsi_ops`) | Entire `cfg80211_ops.c` | TinyAra/WPA driver wrappers | **No** — replaced by Apple `IO80211`/Skywalk glue |
| 17 | Linux data path | `slsi_net_hw_xmit`, `slsi_net_rx_poll` (`netif.c:986`), `sk_buff` rb paths in `rx.c`/`tx.c` | `netif.c`/`slsi_drv_*.c` | **No / Adapt** — none is reusable to XNU net stack exactly |
| 18 | procfs/debug/config knobs | `hip4.c:193-344`, `procfs.c` | TizenRT omits much of this | **No** — drop or rewrite via sysctl/IORegistry |
| 19 | Regulatory platform | `slsi_regd_init` (`dev.c:229`) | none on target | **No** (Apple region handling) |

**IN** Rough risk budgeting against the 12–24 weeks: the "client/driver/platform" portion (cfg80211, data path, Apple glue) is the largest 6–12 weeks; the transport kernel is only part of the envelope. TizenRT's main impact is removing the worst-case guesswork from the transport.

---

## 7. Source list (auditable)

- Samsung TizenRT repository — `https://github.com/Samsung/TizenRT` (license: Apache-2.0; root `LICENSE` fetched; source headers on TizenRT MX files verified).
- Samsung vendor Linux tree — `https://github.com/samsungexynos7885/android_kernel_samsung_universal7885` (SCSC MX/WLAN driver local reference mirrored at `/var/tmp/opencode-keep/Eureka-Kernel`).
- Mainline Linux tree — `https://github.com/torvalds/linux/tree/master/drivers/net/wireless` (directory listing; no SCSC).
- LineageOS Android SCSC HAL hit — `https://github.com/LineageOS/android_hardware_samsung_slsi_scsc_wifibt_wifi_hal` (userspace HAL, not a host transport); also `linux4-bringup-priv/android_hardware_samsung_slsi_scsc_wifibt_wifi_hal`.
- Linux shared-memory IPC reference source families: `drivers/rpmsg`, `drivers/remoteproc`, `drivers/soc/qcom/smem.c`, `qcom_smd.c`, `qcom_glink_native.c` (mainline Linux; class analogy only).

---

## 8. Honest bottom line

- **Prior art for SCSC/MX140 itself:** **VS** none beyond Samsung's own vendor kernels and the TizenRT reference port. No BSD/Darwin/iOS/other-RTOS transport found; no independent mechanism write-up beyond Samsung's source/struct layout.
- **The one thing that genuinely helps:** **VS/CL** `Samsung/TizenRT` is a 3,998-line Apache-2.0, Linux-free, portable-reference implementation of the core MX transport (S5JT200 orientation), with exact firmware paths and CRC logic. **IN** It should save days to a few weeks on register/mailbox/ring correctness and lower early failure risk.
- **Nothing can short-circuit**: obtaining the proprietary `mx140.bin` + two HCF MIB tables, matching the Exynos 7885 register aperture, or implementing Apple's private IO80211/Skywalk glue.
- **Rating:** **CL/IN** "Nothing transfers" would be false only because TizenRT transfers the transport as a reference; "weeks disappear" would also be false. Reasonable planning assumption remains **10–20 engineer-weeks** (reduced modestly from the 12–24-week envelope), with firmware gated before any Apple-side effort. *If the firmware cannot be obtained or the Exynos 7885 MIF aperture differs materially from the local vendor description, no amount of prior art rescues the port.*
