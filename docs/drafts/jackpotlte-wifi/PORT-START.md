# jackpotlte WiFi (SCSC / Maxwell MX140) — port start

Bringing up the Galaxy A8 2018 (`jackpotlte`, Exynos 7885) WiFi+BT block under
XNU.  This is the first session: a working skeleton, breadth-first, not a
finished driver.  Everything below is labelled **implemented**, **stubbed** or
**unknown**.  No build has been run and nothing has been booted; see
[Verification](#verification) and [Not verified](#not-verified).

The board's radio is Samsung's SCSC / Maxwell block (compatible
`samsung,scsc_wifibt`), not an Apple/Broadcom part.  The AP talks to its Cortex-R4
over a mailbox bank plus a reserved DRAM window, and starts it by loading
`mx140.bin` (a "smxf" image) into that window and releasing the WLBT reset.
There is no `mx140.c` in the vendor tree; the transport lives in
`scsc_mx_impl.c`, `mxman.c`, `mif*.c`, `platform_mif.c`, `fwhdr.c` and
`fwimage.c`, and that is what this port reads.

Sources quoted below (all under `/var/tmp/opencode-keep/Eureka-Kernel/`, the
donor tree; blobs stay out of git):

| tag | file |
| --- | --- |
| DT | `arch/arm64/boot/dts/exynos/dtb/exynos7885.dtsi` |
| DT-MEM | `arch/arm64/boot/dts/exynos/dtb/exynos7885-rmem.dtsi` |
| REG | `drivers/misc/samsung/scsc/mif_reg_S5E7885.h` |
| MIF | `drivers/misc/samsung/scsc/platform_mif.c` |
| MXMAN | `drivers/misc/samsung/scsc/mxman.c` |
| FWHDR | `drivers/misc/samsung/scsc/fwhdr.c` |
| FWHDR-H | `drivers/misc/samsung/scsc/fwhdr.h` |
| MXCONF | `drivers/misc/samsung/scsc/mxconf.h` |
| FILE | `drivers/misc/samsung/scsc/mx140_file.c` |

## Deliverables

1. `xnu/patches/0138-jackpotlte-scsc-mx-transport.patch` — mailbox + MIF shared
   memory + interrupt bits + firmware download + PMU reset release.  Adds
   `bsd/dev/a5xelte_scsc.{c,h}` and wires it into `bsd/conf/files` and
   `iokit/bsddev/IOKitBSDInit.cpp`.
2. `xnu/patches/0139-jackpotlte-apple80211-glue.patch` — an `en0` ifnet that
   answers `SIOCS/GA80211` so wifid finds an interface.  Adds
   `bsd/dev/a5xelte_wifi.c` and wires it in the same two files.
3. This file.

Both patches are opt-in.  A default boot does not map the mailbox, does not
release WLBT reset and does not create a WiFi interface.  Only `a8wifi=1`
turns those on.

## Hardware facts (quoted, not guessed)

### Addresses and interrupts

| fact | value | source |
| --- | --- | --- |
| R4 mailbox bank | `0x120c0000`, size `0x180` | DT:1800 |
| M4 mailbox bank | `0x120e0000`, size `0x180` | DT:1801 |
| mailbox IRQ | SPI 57 (`<0 57 4>`) → GIC ID 89 | DT:1803 |
| "ALIVE" IRQ | SPI 21 → GIC ID 53 | DT:1803 |
| "WDOG" IRQ | SPI 286 → GIC ID 318 | DT:1803 |
| PMU syscon | `0x11c80000`, size `0x10000` | DT:1291 |
| MIF shared DRAM (`wifibt_if`) | `0xE9000000`, size `0x400000` | DT-MEM:55-58 |

The first cell of each `interrupts` triple is `0` == SPI; GICv2 maps SPI *n* to
ID *n* + 32.  Only the MBOX line matters for this patch; ALIVE and WDOG are
recorded for the next session.

### Mailbox registers (offset from a bank base, 8 mailboxes)

`MCUCTRL 0x000`, `INTGR0 0x008`, `INTCR0 0x00c`, `INTMR0 0x010`,
`INTSR0 0x014`, `INTMSR0 0x018` (bank R0, bits `[31:16]`, interrupts **from**
R4/M4); `INTGR1 0x01c`, `INTCR1 0x020`, `INTMR1 0x024`, `INTSR1 0x028`,
`INTMSR1 0x02c` (bank R1, bits `[15:0]`, interrupts **to** R4/M4);
`MIF_INIT 0x04c`, `IS_VERSION 0x050`, `ISSR(r) = 0x080 + 4*r`,
`SEMAPHORE_BASE 0x180`; `NUM_MBOX_PLAT 8` — REG:11, 13, 16-40.  On 7885 the
"to M4" interrupts reuse the R1 offsets in the **M4** bank (MIF:803-826), so both
banks are mapped.

### PMU registers (offset from PMU base)

| name | off | bits used | source |
| --- | --- | --- | --- |
| `WIFI_CTRL_NS` | `0x140` | `WIFI_PWRON` b1, `WIFI_RESET_SET` b2, `WIFI_ACTIVE_CLR` b6 | REG:46-50 |
| `WIFI_CTRL_S` | `0x144` | `WIFI_START` b3 | REG:79-80 |
| `WIFI_STAT` | `0x148` | `WIFI_PWRDN_DONE` b0, `WIFI_ACCESS_MIF` b4 | REG:86-88 |
| `CENTRAL_SEQ_WIFI_CONFIGURATION` | `0x380` | `SYS_PWR_CFG_16` b16 | REG:124 |
| `CENTRAL_SEQ_WIFI_STATUS` | `0x384` | `STATES 0xff0000`; UP == `0x80` | REG:130-131, MIF:526 |
| `WIFI2AP_MEM_CONFIG0` | `0x7300` | MEM_SIZE (4 KiB units) | REG:100 |
| `WIFI2AP_MEM_CONFIG1` | `0x7304` | BASE ADDR (4 KiB units) | REG:101 |
| `WLBT_BOOT_TEST_RST_CFG` | `0x7330` | 0 == boot from DRAM | REG:112, MIF:684 |
| `RESET_AHEAD_WIFI_PWR_REG` | `0x1360` | | REG:117 |
| `CLEANY_BUS_WIFI_SYS_PWR_REG` | `0x1364` | | REG:118 |
| `LOGIC_RESET_WIFI_SYS_PWR_REG` | `0x1368` | | REG:119 |
| `TCXO_GATE_WIFI_SYS_PWR_REG` | `0x136c` | | REG:120 |
| `WIFI_DISABLE_ISO_SYS_PWR_REG` | `0x1370` | | REG:121 |
| `WIFI_RESET_ISO_SYS_PWR_REG` | `0x1374` | | REG:122 |

The vendor reaches these through the `pmu_system_controller` syscon regmap
(MIF:1473); here they are direct MMIO (`ml_io_map`).

### Firmware image format ("smxf", header version 0.2)

Little-endian, fixed byte offsets: magic `"smxf"` @ 8; header length @ 16;
fw-api minor @ 20, major @ 22; fw runtime length @ 36; entry point @ 40;
build id @ 48; header CRC @ `hdr_length - 4`; fw CRC @ 24 — FWHDR:16-42,
FWHDR-H:19-29.

### Mailbox startup contract

`MBOX0` = firmware entry point; `MBOX1` = R4-relative mxconf address; `MBOX2` =
`MBOX2_MAGIC_NUMBER` (`0xbcdeedcb`); `MBOX3` = startup flags (0 == normal) —
MXMAN:764-792.  `mxconf.magic = 0x79828486`, version 0.1 — MXCONF:35,43-44.
`mxconf` is 176 bytes (magic 4 + version 4 + 3×48 transconf + 24 logconf); this
port reserves 0x200 and zeroes it.  DRAM `0x400000`, firmware runtime region
`0x100000` — MXMAN:49-50.

## What 0138 implements / stubs / leaves unknown

**Implemented** (`bsd/dev/a5xelte_scsc.c`)

- Mapping of the PMU, both mailbox banks and the MIF DRAM window, with a
  read-only identity report of `WIFI_CTRL_NS/S`, `WIFI_STAT`,
  `CENTRAL_SEQ_WIFI_STATUS` and the mailbox `IS_VERSION`/`INTMR0`/`INTSR0`.
- Mailbox bit operations: read/mask/unmask/clear of the from-R4/M4 bank and
  generate-to-R4/M4 (MIF:766-908).
- A bump allocator over MIF DRAM (64-byte block, `miframman.h:23`).
- Reading `mx140.bin` from the staged iOS root and parsing the "smxf" header.
- The mailbox startup contract (entry point, config ref, magic, flags).
- The PMU reset-release sequence with the power-state handshake (MIF:410-534).
- A bring-up thread that retries the firmware read for 60 × 500 ms so the iOS
  root can mount first.

**Stubbed**

- The mxconf transport streams (mxmgmt / gdb / mxlog) that the vendor serialises
  before releasing reset.  A zeroed 0x200 reservation with the correct
  magic/version is placed instead, so R4 can at least read it.
- The GIC interrupt is not registered, so the mailbox ISR is not hooked and no
  firmware handshake message is waited for.
- Firmware CRC32 checks (`fwimage.c`) are not run.
- The 20 MHz USB PLL handover is off: the jackpotlte DT leaves
  `samsung,syscon-cmu_fsys` commented out (DT:1809).

**Unknown / blocked**

- Whether the `0x80` `CENTRAL_SEQ_WIFI_STATUS` handshake actually completes on
  this board's PMU without the secure `CENTRAL_SEQ_WIFI_CONFIGURATION` dance
  (MIF:506-510) — needs hardware.
- Whether the plain `mx140.bin` on the staged root matches this board's firmware
  API (FILE:57 selects variants by HW version).
- Whether the mailbox banks are clock/power gated at the point
  `IOKitBSDInit` runs; the default probe therefore does not touch them.

## Firmware load path

`mx140.bin` is read from **`/private/var/wifi/mx140.bin`** into the MIF window
with `vnode_lookup` + `vnode_getattr(va_data_size)` +
`vn_rdwr(UIO_READ, …, UIO_SYSSPACE, IO_NOCACHE, …, vfs_context_proc(ctx))`.

Why: the iOS root is mounted by the time wifid runs, and the bring-up thread
waits for it.  The vendor reads from a base directory (`CONFIG_SCSC_CORE_FW_LOCATION`,
FILE:96-98); on iOS that directory does not exist, so the staged root is the
only filesystem that can carry the blob.  The path is a single `#define`
(`A5SCSC_FW_PATH`) so it can be moved once the wlan service lands.

The `mx140.bin` file itself is a blob and is **not** in git; it must be staged
by hand for any bring-up run.

## 0139 — Apple80211 glue

wifid and Settings reach a radio over `SIOCSA80211` (`0x802869c8`) /
`SIOCGA80211` (`0xc02869c9`) with a `struct apple80211req`.  XNU hands an
unknown `IOCGROUP == 'i'` ioctl to the interface
(bsd/net/if.c:3555-3565 → bsd/net/dlil.c:7596-7599), whose first 16 bytes
(`req_if_name`) alias `ifreq.ifr_name`, so `ifioctl`'s `ifunit_ref` lookup finds
it.  The layouts and event numbers are Apple's, taken from the same reference
used for the rest of the port.

**Implemented**: an `en0` ifnet (family Ethernet, `IFT_ETHER`), `if_ioctl`
answering `POWER` (19) and `STATE` (13), refusal (`EOPNOTSUPP`) for everything
else, a bounded per-request log, and a class-6/IEEE-80211 kernel event on power
change.

**Stubbed**: there is no wlan service yet, so there is no scan, no join, no link
state and no RSSI.  `STATE` always reads "not associated"; `POWER` only records
wifid's intent.  The whole glue is off unless `a8wifi=1`.

**Unknown**: which requests wifid actually needs on this OS version — the log in
`a5wifi_if_ioctl` exists to answer that on the first hardware run.

## Verification

Both patches were produced with `git diff` in a private copy of
`/tmp/opencode/xnu-xnu-8796.141.3` (tag `stock`), with the existing patch set up
to `0137` applied and committed as `base` (`241a1aa`).

`git apply --check --verbose` of `0138` against a fresh `base` extraction:

```
Checking patch bsd/conf/files...
Checking patch bsd/dev/a5xelte_scsc.c...
Checking patch bsd/dev/a5xelte_scsc.h...
Checking patch iokit/bsddev/IOKitBSDInit.cpp...
exit=0
```

`git apply --check --verbose` of `0139` against `base` + `0138`:

```
Checking patch bsd/conf/files...
Checking patch bsd/dev/a5xelte_wifi.c...
Checking patch iokit/bsddev/IOKitBSDInit.cpp...
exit=0
```

## Next step

Build a CI kernel with `0138` + `0139` applied (the A5XELTE/JACKPOTLTE config)
and boot it **without** `a8wifi`, to confirm the read-only SCSC probe prints the
PMU and mailbox identity lines and that nothing else changed — the first
observation that says the mapping is real.  Only after that should `a8wifi=1`
bring-up be attempted.

Ordered follow-ups (each its own patch):

1. Register the GIC mailbox interrupt (SPI 57 → ID 89) and hook the ISR.
2. Serialise the mxconf transport streams and match them to the streams the
   driver expects.
3. Add the firmware CRC32 checks (`fwimage.c`).
4. Wait for the R4 "services ready" mailbox message before declaring the
   transport up.
5. Bring in the mx140 wlan service (scan/join/link) and drive 0139's `STATE`,
   RSSI and the scan result layout from it.

## Not verified

- Neither patch has been compiled: XNU needs macOS tooling, and no CI build has
  been run.  Compile errors are possible; review them on the first CI run.
- Nothing has been booted.  No claim is made that the transport, the firmware
  load or the Apple80211 glue works.
- The DT/register/IRQ values above are quoted from the donor vendor tree; they
  have not been checked against this unit's bootloader-supplied device tree.
