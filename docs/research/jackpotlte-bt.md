# jackpotlte Bluetooth (SCSC / Maxwell MX140) — research and plan

Date: 2026-11-08. Track: Bluetooth. Device: Galaxy A8 2018 (`jackpotlte`,
SM-A530F, Exynos 7885). Kernel: XNU 8796.141.3 / Darwin 22.

This document is the Bluetooth half of the SCSC work. It is **research plus one
opt-in patch** (`xnu/patches/0140-jackpotlte-scsc-bt-transport.patch`). Nothing
here has run on the phone. Build success is not device evidence; every claim
below is tagged:

- **vendor** — read directly from Samsung's 4.4 tree at
  `/var/tmp/opencode-keep/Eureka-Kernel/` (read-only, shared).
- **dump** — a fact about the local firmware dump
  `/var/tmp/opencode-keep/jackpotlte-wifi-fw/` (from
  `VDavid003/samsung_jackpotlte_dump`, build `A530FXXS8CSK2`; blobs stay out of
  git).
- **port** — what this repo's patches actually do.
- **inference** — reasoned, not proven. Treat as a hypothesis.

Companion documents: `docs/research/exynos7885-scsc-wifibt.md` (web survey and
a verifier's note), `docs/drafts/jackpotlte-wifi/PORT-START.md` (the accurate
WiFi register facts and what `0138`/`0139` implement).

---

## 0. Bottom line

- Bluetooth is **not a separate chip**. It is `SCSC_SERVICE_ID_BT` (id 2) on the
  same Maxwell R4 that WLAN is service id 1 on, over the same mailbox + MIF
  transport and — on this board — the same `mx140.bin` core image. **vendor**
  (`include/scsc/scsc_mx.h:27-35`; no BT-specific core image in the dump).
- The host-facing contract is **standard HCI**: either HCI H4 on a character
  device (`/dev/scsc_h4_0`), or a virtual HCI device registered with the Linux
  Bluetooth stack. So the Bluetooth *protocol* does not need reimplementing.
  **vendor** (`scsc_bt_priv.h:51-57`, `scsc_bt_module.c:1144-1159`,
  `scsc_bluez.c:173-194`). A prior note in `exynos7885-scsc-wifibt.md`
  over-stated the difficulty; its own verifier's note corrects this.
- What is **proprietary and unported** is the transport to the chip: the MX
  mailbox/MIF core (partly done in `0138`) and the BT-specific shared-memory
  protocol (BHCS + BSMHCP rings + interrupt bits + the service-start message),
  which is not ported.
- `0138` gives us the shared R4 bring-up (PMU rails, mailbox, MIF DRAM, `smxf`
  load, MBOX0-3, reset release) but **stubs** the mxconf transport streams, the
  GIC mailbox ISR and the firmware handshake. Without those the R4 cannot send
  `SM_MSG_START_CFM`, so a *working* BT service is blocked behind work that
  belongs to the shared transport, not to Bluetooth. **port**
- Patch `0140` therefore implements the **BT-service descriptor** on top of the
  `0138` transport: it reads the BT configuration table, lays out the BHCS and a
  zeroed/magic BSMHCP region in MIF DRAM, and logs the service-start message it
  does **not** send. It is gated by a separate boot argument, `a8bt=1`, default
  inert. It does not fake the handshake and does not touch the `a8wifi` gate.
  **port**
- iOS is the hardest unknown: there is no Apple source here for
  `IOBluetoothFamily` / `bluetoothd`, so §5 is inference and the honest unknowns
  are named. **inference**

---

## 1. How the vendor driver acquires the R4/BT service

All paths below are relative to `/var/tmp/opencode-keep/Eureka-Kernel/`.

### 1.1 Service identity and lifecycle

| step | vendor code | what it does |
| --- | --- | --- |
| open | `drivers/misc/samsung/scsc_bt/scsc_bt_module.c:770-773` | `scsc_mx_service_open(core, SCSC_SERVICE_ID_BT, &mx_bt_client, &err)` |
| alloc | `scsc_bt_module.c:792-811` | `scsc_mx_service_mifram_alloc` of `sizeof(struct BHCS)` and `sizeof(struct BSMHCP_PROTOCOL)`, both aligned to `BSMHCP_ALIGNMENT` (32) |
| config | `scsc_bt_module.c:879-884`, `555-695` | `setup_bhcs()` fills BHCS and copies the config file into MIF |
| shm | `scsc_bt_module.c:890`, `scsc_shm.c:1574-1603` | zero BSMHCP, write magic, initialise rings/interrupts |
| irq | `scsc_bt_module.c:897`, `scsc_shm.c:95-128` | allocate 1 to-host + 2 from-host MIF interrupt bits, store their indices in the BSMHCP header |
| start | `scsc_bt_module.c:918` | `scsc_mx_service_start(service, bhcs_ref)` |

`SCSC_SERVICE_ID_BT` is `2`; WLAN is `1`, ANT is `3`
(`include/scsc/scsc_mx.h:27-35`). The service is opened on first `open()` of the
H4 character device (`scsc_bt_module.c:1144-1159`, fops at `1389-1391`,
device created at `2079-2094`).

`scsc_mx_service_open()` itself does **not** boot the R4: it calls
`mxman_open()` (`scsc_service.c:539-614`), whose first invocation downloads the
core firmware and starts the R4; later opens are effectively no-ops on an
already-running core. That is the same one-shot core bring-up `0138` models.

### 1.2 The service-start message

`scsc_mx_service_start()` sends a 6-byte packet on the mxmgmt transport and
blocks for the confirmation:

```c
struct sm_msg_packet { uint8_t service_id; uint8_t msg;
                       scsc_mifram_ref optional_data; } __packed;
```

`msg = SM_MSG_START_REQ` (0), `optional_data = bhcs_ref`
(`servman_messages.h:15-38`; send at `scsc_service.c:147-168`, wait at
`:100-136`, export at `:237-276`). The firmware replies `SM_MSG_START_CFM` (1)
on `MMTRANS_CHAN_ID_SERVICE_MANAGEMENT`, handled at `scsc_service.c:197-235`.

This is the BT service's real "on switch", and it rides on the mxmgmt stream
that `0138` does **not** serialise. So the BT service cannot truly start until
the shared transport's mxconf streams and mailbox ISR exist.

### 1.3 Configuration and firmware files the driver asks for

| what | vendor call | exact name | in the dump? |
| --- | --- | --- | --- |
| BT config table | `mx140_file_request_conf(core, &firm, "bluetooth", "bt.hcf")` (`scsc_bt_module.c:578-579`) | `bt.hcf` (`scsc_bt_priv.h:51`) | **yes**, as `mx140_bt.hcf`, 116 B |
| BT address (only under `SCSC_BT_BLUEZ`) | `mx140_request_file(core, "/csa/bluetooth/.bd_addr", &firm)` (`scsc_bt_module.c:653`) | `/csa/bluetooth/.bd_addr` (`scsc_bt_priv.h:53`) | no |
| core image | requested by the MX core, not the BT driver | `mx140.bin` (`mx140_file.c:38`) | **yes**, 1,360,017 B, shared with WLAN |

`mx140_file.c` has two layouts. The flat layout builds
`"<base>/<variant><suffix>_<filename>"` (`mx140_file.c:145-152`), i.e.
`<base>/mx140_bt.hcf` for the BT config — **exactly the name in the dump**
(`mx140_bt.hcf`; likewise `mx140_wlan.hcf` for `wlan.hcf`). The non-flat layout
adds `conf/<platform>/<rel_path>/`, e.g. `.../conf/bluetooth/bt.hcf`
(`mx140_file.c:153-171`). Either way the *filename* is `bt.hcf`; only the
directory depends on the module parameters. **vendor + dump**

Notes:

- A missing BT config is **not fatal** in the vendor driver — it is silently
  ignored and `configuration_offset/length` stay 0 (`scsc_bt_module.c:580-626`).
- The BT address comes from the SoC unique id when `CONFIG_ARCH_EXYNOS`
  (`scsc_bt_module.c:628-635`) and may be overridden by the `.bd_addr` file
  under `SCSC_BT_BLUEZ` (`:648-679`). XNU has neither source yet.
- There is **no separate BT firmware blob** in the dump: BT shares the `mx140`
  core image, and `bt.hcf` is configuration data, not code. That matches the
  architecture (two services, one R4). **dump**

### 1.4 Shared-memory structures

**BHCS** — "Bluetooth Host Configuration Structure", written by the host and
passed to the firmware as the service-start argument (`bhcs.h:9-16`):

```c
struct BHCS {                       /* bhcs.h:33-42 */
    uint32_t version;               /* BHCS_VERSION == 2, bhcs.h:31 */
    uint32_t bsmhcp_protocol_offset;
    uint32_t bsmhcp_protocol_length;
    uint32_t configuration_offset;
    uint32_t configuration_length;
    uint32_t bluetooth_address_lap;
    uint8_t  bluetooth_address_uap;
    uint16_t bluetooth_address_nap;
};
```

**BSMHCP_PROTOCOL** — the HCI transfer rings shared with the firmware
(`bsmhcp.h:275-321`): command/event/ACL transfer rings, ACL buffers, a timing
ring, firmware info and IQ-report ring. The host zeroes it and writes the header
magic (`scsc_shm.c:1583-1585`); the header also carries the three interrupt-bit
indices (`ap_to_bg_int_src`, `ap_to_fg_int_src`, `bg_to_ap_int_src`) and the
ring read/write indices, which the vendor fills from its MX allocators
(`scsc_shm.c:95-128`; header fields at `bsmhcp.h:196-240`).

`BSMHCP_ALIGNMENT` is 32 (`bsmhcp.h:78`). The MIF interrupt bit space is 16 bits
with bit 0 reserved for panic (`mifintrbit.h:17-21`), so three service bits are
available without special casing.

### 1.5 Host-facing contract (what userspace sees on Linux)

- **`SCSC_BT=y`** (set in the jackpot defconfig): a character device
  `/dev/scsc_h4_0` (`SCSC_H4_DEVICE_NAME`, `scsc_bt_priv.h:57`) carrying **HCI
  H4** framing. `open()` starts the service (`scsc_bt_module.c:1144-1159`);
  `read()`/`write()` move `H4_RECV_*`/HCI packets through the BSMHCP rings
  (`scsc_shm.c` read paths; packet types are `H4_RECV_ACL`/`H4_RECV_EVENT`).
- **`SCSC_BT_BLUEZ`** (default **N**, not set in the jackpot defconfig):
  registers a virtual controller with `hci_alloc_dev()`,
  `hdev->bus = HCI_VIRTUAL`, `hdev->dev_type = HCI_BREDR`,
  `hci_register_dev()` (`scsc_bluez.c:173-194`). Same HCI packets, no char
  device. (`Kconfig` in `drivers/misc/samsung/scsc_bt/`.)

Either way the seam to the OS is **standard HCI**; what is Samsung-specific is
the transport underneath (BSMHCP rings + MX mailbox/MIF).

---

## 2. What is reusable from `0138` verbatim, and what is BT-specific

`0138` adds `bsd/dev/a5xelte_scsc.{c,h}`. Its pieces split cleanly:

| piece | in `0138` | belongs to | reusable for BT? |
| --- | --- | --- | --- |
| PMU/mailbox/MIF mapping + read/write | yes | shared R4 | **yes, verbatim** |
| mailbox bit ops (mask/clear/generate, to-R4/to-M4) | yes | shared R4 | yes |
| MIF-DRAM bump allocator | yes (no free) | shared R4 | yes (needs an exported API) |
| `smxf` header parse + `mx140.bin` read | yes | shared R4 | yes |
| MBOX0-3 startup contract | yes | shared R4 | yes |
| PMU reset-release handshake | yes | shared R4 | yes |
| mxconf transport streams | **stubbed** | shared R4 | needed by BT too (bt is blocked on it) |
| GIC mailbox ISR | **not registered** | shared R4 | needed by BT too |
| firmware CRC32 | **not run** | shared R4 | same |
| service id / `mx140_file_request_conf("bluetooth","bt.hcf")` | no | **BT** | new |
| BHCS + BSMHCP layout, magic, header | no | **BT** | new |
| 3 MIF interrupt-bit allocation + header indices | no | **BT** (uses shared allocator) | new |
| `SM_MSG_START_REQ` / `CFM` service start | no | shared transport, per-service | new |
| BT address source (SoC uid / `.bd_addr`) | no | **BT** | new |
| H4 char device / virtual HCI | no | **BT / iOS glue** | new |

`0138`'s helpers were `static`. Patch `0140` exports exactly the four the BT
side needs (`a5scsc_transport_up`, `a5scsc_mifram_alloc`, `a5scsc_mifram_ptr`,
`a5scsc_read_file`) and factors the one-shot R4 bring-up into
`a5scsc_transport_up()` so both boot arguments share it; it does **not** change
the `a8wifi` gate or the default (inert) probe behaviour.

---

## 3. What patch `0140` does

File: `xnu/patches/0140-jackpotlte-scsc-bt-transport.patch`. Adds
`bsd/dev/a5xelte_bt.{c,h}`, wires them into `bsd/conf/files` and
`iokit/bsddev/IOKitBSDInit.cpp`, and exports the transport API from
`a5xelte_scsc.{c,h}`.

**Implemented** (`a5xelte_bt.c`, all behind `a8bt=1`):

- calls `a5scsc_transport_up()` (reuses `0138`: mapping, mailbox, `mx140.bin`
  load, MBOX0-3, reset release; idempotent);
- allocates a `BSMHCP_PROTOCOL` region in MIF, zeroes it and writes
  `BSMHCP_PROTOCOL_MAGICVALUE` — the two operations `scsc_shm.c:1583-1585` does;
- allocates and fills `struct BHCS`: `version = 2`, `bsmhcp_protocol_offset/len`,
  config offset/len;
- reads the BT config table from `A5BT_CONF_PATH`
  (`/private/var/wifi/mx140_bt.hcf`, a staging choice mirroring
  `A5SCSC_FW_PATH`; the dump name is `mx140_bt.hcf`) into MIF and records it in
  BHCS; a missing file is non-fatal, as in the vendor driver;
- logs the service-start message it would send (`id=2`, `SM_MSG_START_REQ`,
  `bhcs_ref`) and the H4 device name.

**Stubbed** (logged at runtime, and deliberately not faked):

- the three MIF interrupt bits (1 to-host, 2 from-host) — the MX
  interrupt-bit allocator is not ported, so the BSMHCP interrupt-source fields
  stay 0;
- the BSMHCP transfer-ring mailbox indices — same reason;
- the `SM_MSG_START_REQ` send and the `SM_MSG_START_CFM` wait — the mxmgmt
  streams are not serialised in `0138` and no mailbox ISR is registered, so the
  message could never be answered;
- the Bluetooth address — the vendor's sources (SoC unique id, `.bd_addr`) do
  not exist on XNU yet, so it stays 0;
- no H4 character device and no virtual HCI device.

**Derived constant, flagged:** `A5BT_BSMHCP_SIZE = 0x165C0` and
`A5BT_BSMHCP_MAGIC = 0x155460C2` are arithmetic on quoted layout values
(`bsmhcp.h:29-30, 78, 275-321, 326-329`); the derivation is written in
`a5xelte_bt.h` so it can be re-checked, not trusted. Everything else in
`a5xelte_bt.h` is a quoted value with a `file:line`.

**Boot argument:** `a8bt=1` (separate from `a8wifi`). Default boot prints
`A5XELTE BT: probe only (a8bt=0); BT service not started`. `a8wifi` behaves
exactly as before.

---

## 4. Verification status

- **Compiles:** to be decided by CI (`gh workflow run xnu.yml --ref
  jackpotlte-bt -f extra_cflags=-DJACKPOTLTE=1`). Local XNU builds are not
  possible (macOS tooling), and no phone action is in scope for this track.
- **Applies cleanly:** `git apply --check` of `0140` on a tree with `0001-0139`
  applied (base `0138`+`0139`) passes for all six files.
- **Never booted.** No claim is made that the transport, the descriptor or the
  reset release works. The `0138` transport it builds on is itself unproven.

---

## 5. What iOS expects of a Bluetooth controller (inference; no Apple source)

This section is **inference**. There is no `IOBluetoothFamily`, `bluetoothd` or
`BlueTool` source in this repository or on the allowed vendor tree, and the
flashed iOS root was not opened for this track. Names below come from general
Darwin knowledge, not from a file I can cite.

### 5.1 The shape of the stack (inference)

- Userspace `bluetoothd` (plus `BlueTool` for firmware load on Apple's own
  controllers) talks to the kernel over `IOBluetoothFamily`.
- `IOBluetoothFamily` provides `IOBluetoothHCIController` (the HCI state
  machine) and expects a **transport** object that moves HCI packets and does
  board-specific power/firmware setup. On Apple hardware that transport is the
  UART/PCIe one for the Broadcom part, with Apple-private vendor HCI commands
  used during bring-up.
- Therefore, in principle, a new IOKit transport driver that speaks HCI to the
  Samsung firmware could satisfy the HCI layer — the Bluetooth protocol is
  standard, as §1.5 shows.

### 5.2 Honest unknowns (these decide feasibility)

1. **Is `IOBluetoothFamily` linkable from the kernel build?** The port builds
   with `XNU_TARGET_OS_OSX` userland and a hand-stubbed device layer; there is
   no evidence the private `IOBluetoothFamily` kext symbols are available at
   build or load time. Unknown until a kext is attempted against the real
   family binary on the iOS root.
2. **Does `bluetoothd` accept a non-Apple controller?** Apple's own controller
   path issues Broadcom-specific commands and reads Apple NVRAM/board ids. A
   Samsung firmware may not answer them; `bluetoothd` may refuse to bring the
   controller up. Unknown.
3. **What does `BlueTool`/`bluetoothd` require for firmware load?** On Apple
   parts the host pushes firmware over HCI. Here the firmware is downloaded by
   the R4 host side (`0138`) and the host only speaks HCI to a running
   controller. Whether the iOS stack tolerates "firmware already loaded" is
   unknown.
4. **Entitlements / private symbols.** Loading a third-party transport kext on
   this root already depends on `launchd_unsecure_cache=1` and the AMFI
   workarounds used for SpringBoard. Whether a Bluetooth family plugin can be
   loaded the same way is unknown.
5. **The exact Apple transport API.** Without the family headers/kext, the
   subclass and method names cannot be verified; anything written against them
   is a placeholder until the real binary is in hand.

None of these can be answered from source in this repo. The first three are
answerable by inspecting/disassembling the `IOBluetoothFamily` kext from the
iOS root image — a separate, explicitly-scoped reverse-engineering task.

### 5.3 What is *not* an unknown

- The chip does not need a Bluetooth-stack rewrite: the host side of the
  Samsung BT service is HCI (`§1.5`). The work is transport + iOS glue, not a
  new protocol.
- The BT service shares WLAN's R4 and `mx140.bin`; there is no separate BT
  processor to boot.

---

## 6. Staged ladder and kill criteria

Each rung counts as reached only with output from the phone (serial or screen),
the same standard as `JACKPOTLTE_PROGRESS.md`. A build is never evidence.

### L0 — shared transport comes up, BT descriptor is placed
- **Prereq:** the `0138` transport is proven (its own L0) and `0140` compiles.
- **Try:** boot with `a8bt=1` (and `-DJACKPOTLTE=1` kernel). Watch for, in
  order:
  1. `A5XELTE SCSC: PMU WIFI_CTRL_NS ...` (mapping is live);
  2. `A5XELTE SCSC: WLBT released from reset; transport up ...`;
  3. `A5XELTE BT: config /private/var/wifi/mx140_bt.hcf 116 bytes ...`;
  4. `A5XELTE BT: service id 2 descriptor bhcs=... bsmhcp=...`.
- **Kill:** if (1) does not print, the PMU/MIF mapping is wrong — stop, this is
  an `0138` bug, not BT. If (2) times out with `STATUS 0x...`, the reset release
  is wrong for this board — stop and debug the PMU state machine before any BT
  work. If (3) reports a read error once the iOS root is mounted, the blob is
  not staged — a packaging problem, not a driver problem.

### L1 — BT service start handshake completes (R4 runs the service)
- **Needs new work:** serialise the mxconf streams (`0138` stub), register the
  GIC mailbox ISR (SPI 57 → ID 89), allocate the three MIF interrupt bits, write
  the BSMHCP ring indices, then send `SM_MSG_START_REQ` and wait for
  `SM_MSG_START_CFM` (`scsc_service.c:147-168`).
- **Evidence:** a log line when `SM_MSG_START_CFM` is decoded for service id 2.
- **Kill:** if the streams/ISR are correct but no `CFM` arrives within the
  vendor timeout, the mxconf serialisation or the service id/`bhcs_ref` contract
  is wrong. If the R4 never even signals on the mailbox, the transport is still
  the blocker — do not continue to HCI.

### L2 — HCI boundary: the controller answers HCI
- **Needs new work:** implement the BSMHCP rings (command/event/ACL) and expose
  them (H4 semantics or an in-kernel HCI channel). Send HCI `Reset` and
  `Read Local Version Information`; expect Command Complete events.
- **Evidence:** HCI Reset completes and a real `HCI Version`/manufacturer comes
  back through the ring.
- **Kill:** if the controller never answers HCI Reset, the BSMHCP ring layout or
  the mailbox doorbells are wrong. This is a hard stop: nothing above HCI can be
  debugged without a live controller.

### L3 — the iOS stack sees a controller
- **Needs new work:** an IOKit transport under `IOBluetoothFamily` (see §5) and
  a way to load it on this AMFI-limited root.
- **Evidence:** `bluetoothd`/IORegistry reports a controller (or a screen shows
  Bluetooth available).
- **Kill:** if `IOBluetoothFamily` cannot be linked/loaded, or `bluetoothd`
  rejects a non-Apple controller after L2 works, stop the iOS-integration leg
  and reassess (§5.2 unknowns 1-4). Do not spend HCI-ring effort hoping the
  iOS glue will land.

---

## 7. Next concrete action

1. **Get `0140` compiled** (CI, `-DJACKPOTLTE=1`) and confirm it applies after
   `0139`. This is the only thing this track can do without the phone.
2. **Only then, and only on the owner's phone:** boot the CI kernel with
   `a8bt=1` (not `a8wifi`) and capture the L0 markers above. Without (1)-(4) no
   BT claim is possible.
3. The gating prerequisite for L1 is shared with WLAN: the mxconf streams + GIC
   mailbox ISR. That should be one transport patch used by both services, not a
   BT-only hack.

---

## 8. Sources (all `file:line` under `/var/tmp/opencode-keep/Eureka-Kernel/`)

- Service ids: `include/scsc/scsc_mx.h:27-35`.
- BT service open/alloc/config/shm/irq/start: `drivers/misc/samsung/scsc_bt/scsc_bt_module.c:555-695, 724-946, 1144-1159, 1389-1391, 2079-2094`.
- BT names: `drivers/misc/samsung/scsc_bt/scsc_bt_priv.h:51-57`.
- H4 / BlueZ: `drivers/misc/samsung/scsc_bt/scsc_shm.c:95-128, 1574-1603`; `scsc_bluez.c:173-194`; `Kconfig`.
- BHCS: `include/scsc/api/bhcs.h:9-42`.
- BSMHCP: `include/scsc/api/bsmhcp.h:29-30, 78, 196-240, 275-321, 326-329`.
- MX service start / mxmgmt / servman: `drivers/misc/samsung/scsc/scsc_service.c:100-276, 539-614`; `servman_messages.h:15-38`.
- mxman bring-up order and MBOX contract: `drivers/misc/samsung/scsc/mxman.c:52, 764-792, 976-991`.
- Firmware/config files: `drivers/misc/samsung/scsc/mx140_file.c:24-38, 96-98, 145-171, 180-230`.
- MIF interrupt bits: `drivers/misc/samsung/scsc/mifintrbit.h:17-21`.
- Firmware dump: `/var/tmp/opencode-keep/jackpotlte-wifi-fw/` (`mx140.bin`, `mx140_bt.hcf`, `mx140_wlan.hcf`, `platform.txt`, `hydra_config.sdb`).
- This repo: `xnu/patches/0138-jackpotlte-scsc-mx-transport.patch` (transport), `0139-jackpotlte-apple80211-glue.patch` (WLAN glue, reference for wiring), `0140-jackpotlte-scsc-bt-transport.patch` (this track).