# jackpotlte Bluetooth: the SCSC BT service, as the vendor drives it

Notes for the endpoint added in `xnu/patches/0183-jackpotlte-scsc-bt.patch`
(`bsd/dev/a5xelte_bt.{c,h}`).  The target is the Galaxy A8 2018 (`jackpotlte`,
Exynos 7885), whose WiFi and Bluetooth are two services of the same Samsung
SCSC / Maxwell MX140 Cortex-R4.

Everything here is read out of the donor vendor tree under
`/var/tmp/opencode-keep/Eureka-Kernel/` (not in git).  Layout numbers were
checked by compiling the vendor headers on the host with `offsetof`/`sizeof`,
not by transcription.

| tag | file |
| --- | --- |
| SHM | `drivers/misc/samsung/scsc_bt/scsc_shm.c` |
| BTMOD | `drivers/misc/samsung/scsc_bt/scsc_bt_module.c` |
| BSMHCP | `include/scsc/api/bsmhcp.h` |
| BHCS | `include/scsc/api/bhcs.h` |
| SERV | `drivers/misc/samsung/scsc/scsc_service.c` |
| SERVMSG | `drivers/misc/samsung/scsc/servman_messages.h` |
| MXMGMT | `drivers/misc/samsung/scsc/mxmgmt_transport_format.h` |
| MIFBIT | `drivers/misc/samsung/scsc/mifintrbit.c` |

## Why Bluetooth lives on the WiFi transport

The R4 is started once, by the WiFi side of the block: `mx140.bin` is loaded
into MIF DRAM, the WLBT reset is released and the R4 announces itself with
`MM_START_IND` on the mxmgmt stream.  After that, each radio is a **service**
the AP asks the service manager (in the firmware) to start, over the same
mxmgmt stream.  There is no separate BT firmware, no separate reset and no
separate interrupt line.

So 0183 does not touch PMU, mailbox or firmware load at all.  It waits for
`a5scsc_transport_up()`, then uses the exported MIF allocator and interrupt-bit
allocator from `a5xelte_scsc.c`.

## The service-manager protocol

mxmgmt packets are 8 bytes: `{ u8 channel_id; u8 payload[7]; }`
(MXMGMT:15-23).  Channel 1 is the service manager.  Its payloads are
`sm_msg_packet` — packed, `{ u8 service_id; u8 msg; u32 optional_data; }`,
6 bytes (SERVMSG:29-33).

| message | value | meaning |
| --- | --- | --- |
| `SM_MSG_START_REQ` | 0 | AP asks for a service; `optional_data` = config block offset |
| `SM_MSG_START_CFM` | 1 | firmware accepted it |
| `SM_MSG_STOP_REQ` | 2 | |
| `SM_MSG_STOP_CFM` | 3 | |

Service ids: WLAN 1, BT 2.  The AP registers a handler for channel 1 **before**
sending the request (`mxman.c:1024-1026`), then waits
`sm_completion_timeout_ms` = 1000 ms (SERV:26).

For BT the `optional_data` is the MIF offset of a `BHCS`.

## BHCS — the Bluetooth Host Configuration Structure

`struct BHCS`, 28 bytes (BHCS:39-50):

| off | size | field | who writes |
| --- | --- | --- | --- |
| 0 | u32 | `version` = 2 | AP |
| 4 | u32 | `bsmhcp_protocol_offset` | AP |
| 8 | u32 | `bsmhcp_protocol_length` | AP |
| 12 | u32 | `configuration_offset` | AP (0: no blob) |
| 16 | u32 | `configuration_length` | AP (0) |
| 20 | u32 | `bluetooth_address_lap` | AP (0 here) |
| 24 | u8 | `bluetooth_address_uap` | AP (0) |
| 26 | u16 | `bluetooth_address_nap` | AP (0) |

`setup_bhcs()` (BTMOD:556-575) fills version, protocol offset/length and the
configuration fields; the address comes from the host stack (or a property) and
is zero when nothing supplies it.

## BSMHCP — the shared-memory HCI protocol

`sizeof(struct BSMHCP_PROTOCOL)` is `0x165c0` (91 584) bytes and the magic the
AP must write is `BSMHCP_PROTOCOL_MAGICVALUE` = **`0x155160c2`** for this
geometry (`BSMHCP_ACL_BUFFER_SIZE` 1024, `BSMHCP_CMD_EVT_BUFFER_SIZE` 258,
`offsetof(acl_tx_buffer)`).  Both numbers are computed by the vendor macro from
the header's own constants; the header changes them if any of the three do.

Layout (from `offsetof` on the vendor header):

| off | size | field |
| --- | --- | --- |
| `0x00000` | 256 | `header` (BSMHCP_HEADER) |
| `0x00100` | 2080 | `hci_cmd_transfer_ring`, 8 × 260 B |
| `0x00920` | 192 | `avdtp_transfer_ring`, 16 × 12 B (unused) |
| `0x009e0` | 8128 | `to_air_reserved` (unused) |
| `0x029a0` | 256 | `acl_tx_data_transfer_ring`, 32 × 8 B |
| `0x02aa0` | 30720 | `acl_tx_buffer`, 30 × 1024 B |
| `0x0a2c0` | 8448 | `hci_evt_transfer_ring`, 32 × 264 B |
| `0x0c3c0` | 2304 | `timing_transfer_ring` (unused) |
| `0x0ccc0` | 416 | `information` (unused) |
| `0x0dea0` | 1440 | `iq_reporting_transfer_ring` (unused) |
| `0x0e440` | 33024 | `acl_rx_transfer_ring`, 32 × 1032 B (unused) |
| `0x16540` | 128 | `acl_tx_free_transfer_ring`, 32 × 4 B |

Header fields this port uses:

| off | size | field |
| --- | --- | --- |
| `0x00` | u32 | `magic_value` |
| `0x14` / `0x16` / `0x18` | u16 | `ap_to_bg` / `ap_to_fg` / `bg_to_ap` interrupt bit |
| `0x24` / `0x44` | u32 | command ring write / read index |
| `0x28` / `0x48` | u32 | event ring read / write index |
| `0x2c` / `0x60` | u32 | ACL TX ring write / read index |
| `0x30` / `0x64` | u32 | ACL free ring read / write index |
| `0x40` | u16 | `panic_deathbed_confession` |
| `0x56` | u8 | `service_state` |
| `0x06` / `0x08` / `0x0c` / `0x10` / `0x1c` | | `service_request`, `acl_buffer_size`, `cmd_evt_buffer_size`, `acl_tx_buffers`, `mailbox_offset` — **left 0**, as the vendor leaves them |

Ring entry shapes:

- command: `struct BSMHCP_TD_CONTROL { u16 length; u8 data[258]; }`, 260 B;
- event: `struct BSMHCP_TD_HCI_EVT { u16 length; u16 hci_connection_handle;
  u16 event_type; u8 data[258]; }`, 264 B;
- ACL TX: `struct BSMHCP_TD_ACL_TX_DATA { u16 length; u8 buffer_index;
  u8 flags; u16 hci_connection_handle; u16 l2cap_cid; }`, 8 B;
- ACL TX free: `struct BSMHCP_TD_ACL_TX_FREE { u8 buffer_index; u8 reserved;
  u16 handle; }`, 4 B.

Every ring holds one free slot (`BSMHCP_HAS_ROOM`).  The rings do **not** carry
the H4 type byte: a command entry is `[opcode lo, opcode hi, plen, params...]`
with `length = 3 + plen`, and an event entry is `[event code, plen, params...]`
with `length = plen + 2`.  The vendor strips the H4 type on the way in
(`scsc_shm.c:1482` passes `&h4_write_buffer[1]`) and re-adds it when copying an
event to the host (`scsc_shm.c:579-600`).

## Interrupt bits

BT needs three bits, and allocates them in the order the vendor driver does
(SHM:95-131):

1. one **to-host** bit, `scsc_service_mifintrbit_register_tohost()`, which is
   the one the AP's poll thread calls back for — published as
   `bg_to_ap_int_src` (R4 → AP);
2. two **from-host** bits on target R4, `alloc_fromhost(..., TARGET_R4)` —
   published as `ap_to_bg_int_src` and `ap_to_fg_int_src`.

The AP raises `ap_to_bg` after writing a command or ACL ring entry
(SHM:258, 428, 1416); the firmware raises `bg_to_ap` when it has put something
in an event or RX ring.

The transport allocates its own bits for mxmgmt/gdb/mxlog first, from bit 0
upwards, skipping from-host bit 0 because it is reserved for forcing a
firmware panic (MIFBIT:236-237).  BT therefore gets whatever is left, which is
why it must not hard-code bit numbers — and why the `A5XELTE BT: config:` line
prints the three it got.

## The boot-time proof

`a8bt=1` runs, in a kernel thread:

1. wait for the transport (120 × 250 ms);
2. allocate BHCS + BSMHCP, zero them, write the magic;
3. allocate the three bits and publish them;
4. fill BHCS;
5. register the channel-1 handler, send `SM_MSG_START_REQ(BT, bhcs_ref)`,
   wait 1000 ms for `SM_MSG_START_CFM`;
6. write `[03 0c 00]` (HCI Reset, opcode `0x0c03`, parameter length 0; no H4
   type byte, which is host-side only) to the command ring, raise `ap_to_bg`,
   and wait 1000 ms for an event whose `data[0]` is the Command Complete event
   code `0x0e`, whose `length` is at least 6, and whose opcode at `data[3..4]`
   is `0x0c03`.

Step 6 succeeding means: the R4 is running, the BT service started, the rings
and interrupt bits are wired the way the firmware expects, and the controller
executes HCI commands.  Nothing short of that proves the endpoint.

## What 0183 implements, stubs, and does not know

**Implemented**

- BHCS + BSMHCP allocation and publication, magic and version.
- The three interrupt bits, in the vendor's order.
- Command ring TX, event ring RX, ACL free-ring reaping.
- Service start/confirm handshake, with a bounded wait.
- HCI Reset proof and one-line-per-stage `A5XELTE BT:` logging.
- `a5scsc_bt_acl_send()`: one ACL packet (descriptor + data buffer), no
  fragmentation, buffer reuse only after the free ring returns it.

**Stubbed / absent**

- No HCI character device, no Bluetooth host-controller interface: nothing can
  open the controller, and nothing calls `a5scsc_bt_acl_send()` yet.
- No ACL RX path, no AVDTP detection, no timing packets, no IQ reports, no
  BT log rings, no A-box audio window.
- No `.hcf` configuration blob; the vendor works without one too when the file
  is missing.
- No stop, restart or recovery; a failure prints one line and leaves the port
  disabled.
- Device address left 0.

**Unknown until a boot**

- Whether the firmware accepts `SM_MSG_START_REQ` for service 2 on this board's
  firmware build at all.
- Whether the BT service needs `service_request` or `mailbox_offset` set
  (vendor leaves both 0, so this port does too).
- Whether HCI Reset is the first command the controller expects, or whether it
  wants a vendor-specific setup first (the vendor stack's first write is
  whatever the host stack sends).
- Whether ACL TX needs the free ring primed before the first packet; the vendor
  starts with all 30 buffers owned by the AP and learns otherwise from the
  ring.

## Test matrix

| `a8wifi` | `a8bt` | expected |
| --- | --- | --- |
| 0 | 0 | no SCSC and no BT lines |
| 1 | 0 | six `A5XELTE SCSC: stage n/6` lines |
| 0 | 1 | the six SCSC stage lines, then the BT `transport`, `config`, `service`, `hci`, `up` lines; no `en0` |
| 1 | 1 | stages, then BT `transport`, `config`, `service`, `hci`, `up` lines |

`a8bt=1` alone is not a WiFi-silent mode at the transport level: the BT thread
calls `a5scsc_transport_up()`, which is the function that maps the mailbox,
loads the firmware, builds mxconf and releases reset, so the R4 comes up and
the six `A5XELTE SCSC:` stage lines appear.  Only the interface stays absent,
because `a5xelte_wifi_probe()` is gated by `a8wifi`.

If `service:` prints but `hci:` does not, the rings or bits are wrong — compare
the published bit numbers with the `stage 2/6 mxconf` line's allocation and
check that the firmware wrote `mailbox_hci_evt_write`.  If `service:` does not
print, the firmware rejected the service start or never saw the request:
suspect the BHCS offset or the mxmgmt bit, not the BT rings.