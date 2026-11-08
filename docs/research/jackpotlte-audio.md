# jackpotlte audio (ABOX + codec) — port start

Speaker / headphone **playback** only. This document scopes the Galaxy A8 2018
(`jackpotlte`, SM-A530F, Exynos 7885) audio front-end (ABOX) and codec, records
the hardware facts with a source for every value, says what iOS actually needs,
and lays out a staged ladder. It is a research note plus a read-only probe —
**no audio driver exists yet** and nothing here has run on the phone.

Everything below is labelled **implemented**, **stubbed** or **unknown**.
Blunt version up front: the port has **no audio driver at all**, and the current
iOS root deliberately turns the audio stack **off** (VirtualAudio disabled,
`StartRecorderServer=false`, `tools/sweet-root-tweaks.py:110-122`). "Registers
readable" is a long way from "sound out of the speaker"; see
[The gap](#the-gap-registers-are-not-sound).

Sources are the read-only vendor tree `/var/tmp/opencode-keep/Eureka-Kernel/`
(Samsung 4.4.302, "Release R24U sources"). Paths below are relative to it (or to
this repo, where named). No firmware blob, dump or serial is committed.

## Hardware facts (quoted, not guessed)

| fact | value | source |
| --- | --- | --- |
| playback codec | Samsung `cod3035x` | `.../dtb/exynos7885-jackpotlte_common.dtsi:172-173` |
| codec bus | Samsung **SPEEDY** serial, `speedy@11CE0000`, client addr `0x03` | `exynos7885-jackpotlte_common.dtsi:170,174` |
| codec compatible | `"codec,cod3035x"` (driver file is `cod3033x.c`) | `exynos7885-jackpotlte_common.dtsi:173`; `sound/soc/codecs/cod3033x.c:1684` |
| codec supply | `vdd-supply = <&l36_reg>` | `exynos7885-jackpotlte_common.dtsi:176` |
| speaker amp | NXP `tfa98xx`, I2C3 addr `0x34` | `exynos7885-jackpotlte_common.dtsi:1855-1860` |
| amp bus | `samsung,s3c2440-i2c` `i2c_3@0x13860000` size `0x100`, IRQ SPI 251 (→GIC 283) | `exynos7885.dtsi:1049-1052` |
| machine (card) | `sound` node, `compatible = "samsung,exynos7885-cod3035"` | `exynos7885-jackpotlte_common.dtsi:1888-1890` |
| machine driver | `sound/soc/samsung/exynos7885_cod3035.c` (`of_match` :918-919) | S8 × 7885 ASoC card |
| ABOX base | `abox@0x14A50000` | `exynos7885.dtsi:1427` |
| ABOX windows | `sfr 0x14A50000+0x10000`, `sysreg 0x14820000+0x10000`, `sram 0x14B00000+0x28000` | `exynos7885.dtsi:1429` |
| ABOX power domain | `pd_dispaud` | `exynos7885.dtsi:1437` |
| ABOX clocks | `PLL_OUT_AUD`, `GATE_ABOX_ABOX`, `DOUT_CLK_ABOX_{AUDIF,UAIF0,UAIF2,UAIF3}`, `GATE_UAIF{0,2,3}` | `exynos7885.dtsi:1444-1449` |
| ABOX IPC / mailbox | tx `0x22000`, rx `0x22300`, tx_ack `0x222FC`, rx_ack `0x225FC`, mbox `0x22600` | `exynos7885.dtsi:1438-1442` |
| ABOX own GIC | `gicd 0x14AF1000+0x1000`, `gicc 0x14AF2000+0x1004`; AP line SPI 119 (→GIC 151) | `exynos7885.dtsi:1420-1424` |
| ABOX SMMU | `sysmmu_abox@0x14A30000+0x9000`, SPI 129 (→GIC 161) | `exynos7885.dtsi:1405-1408` |
| ABOX version reg | `ABOX_VERSION 0x0004` (SFR) | `sound/soc/samsung/abox/abox.h:28` |
| ABOX syspower | `ABOX_SYSPOWER_CTRL 0x0010`, `ABOX_SYSPOWER_STATUS 0x0014` | `sound/soc/samsung/abox/abox.h:29-30` |
| ABOX CA7 state | PMU register `0x2524` (bit0 run, bit28 WFI, bit29 WFE) | `sound/soc/samsung/abox/abox.c:75-78,4854` (`pmu_alive` @ `:5875`) |
| ABOX firmware | `calliope_dram.bin`, `calliope_sram.bin` (+ on-die `tfadsp.bin`, `AP_AUDIO_SLSI.bin`, `APBargeIn_AUDIO_SLSI.bin`) | `abox.c:5522,5868`; `exynos7885.dtsi:1574-1593` |
| ABOX reserved DRAM | `abox_rmem 0xE9400000 + 0x400000` | `exynos7885-rmem.dtsi:60-64` |
| `pd_dispaud` register | `0x11c84020 + 0x20`, `cal_id 0xB1380000`, `need_smc 0x148F0000` | `exynos7885-pm-domains.dtsi:18-26` |
| PMU | `pmu_system_controller@0x11C80000 + 0x10000` | `exynos7885.dtsi:1289-1292` |
| UAIF (I2S) ports | codec on UAIF0, amp on UAIF3 | `exynos7885-jackpotlte_common.dtsi:2009-2016,2033-2043` |
| codec register space | 8-bit reg / 8-bit value, `0x00..0xff`, base reg `0x00` | `sound/soc/codecs/cod3033x.c:111-122`; `cod3033x.h:107` |

The routing in the machine node (`exynos7885-jackpotlte_common.dtsi:1895-1903`)
names the two playback sinks directly:

```
"VOUTPUT",   "ABOX UAIF0 Playback"        <- codec (headphone/earpiece/…)
"SPK",       "AIF Playback-8-34"          <- tfa98xx speaker amp on I2C3
"AIF Playback-8-34", "ABOX UAIF3 Playback"
```

So the topology is: **ABOX Calliope core → UAIF0 (I2S) → cod3035x** for the
coded outputs, and **ABOX UAIF3 → tfa98xx → speaker** for loudspeaker. The ABOX
is the master: it owns the audio DMA (RDMA slots 0-7 playback, WDMA slots for
capture) and the I2S ports, and the AP only feeds it PCM over shared memory.

## What iOS CoreAudio / mediaserverd needs

Playback on iOS 16 is not a single driver. The chain is:

1. **mediaserverd** hosts CoreAudio/AVFoundation and opens a HAL plug-in.
2. The HAL plug-in (in the donor, `VirtualAudio.plugin`) is *userspace*; it
   talks to either a kernel audio service or a real device driver.
3. A hardware backend: on a real iPhone this is the platform audio kext
   (`AppleEmbeddedAudio`-family) under `IOAudioFamily`. This port has **none of
   it** — a grep of `xnu/patches/` finds no `IOAudioFamily`,
   `AppleEmbeddedAudio` or `AppleHDA` (only the IOHID `MasterAudioFunctions`
   volume hooks in `0058`, which are unrelated).

Two very different pieces of work are therefore implied:

- **Kernel:** a driver that powers and clocks the ABOX, loads `calliope_*`, sets
  up one UAIF + the codec path (speedy writes) or the amp (I2C3), and plays PCM.
- **Userspace:** make mediaserverd/CoreAudio open it (`AppleEmbeddedAudio`, a
  HAL plug-in, or the donor's VirtualAudio route) — none of which exists here.

At bring-up the donor deliberately avoided both by disabling the audio HAL:
`tools/sweet-root-tweaks.py:110-122` moves `VirtualAudio.plugin` aside
("mediaserverd crash") and sets `MediaToolbox … StartRecorderServer=false`
("camera filter prewarm … → mediaserverd crash loop"). `NOTES.md` records the
same `mediaserverd` SIGSEGV loop in `VirtualAudio.plugin`
(`docs/research/darwin-boot-prior-art.md:84,159`).

## The gap: registers are not sound

State it plainly, because it is the whole point of this track:

- Today the audio stack is **stubbed out on purpose**. `VirtualAudio` is
  disabled and `StartRecorderServer=false`; there is no kernel audio driver.
  mediaserverd has nothing to open. A default boot makes **no sound** by design.
- The probe in this track (below) only **reads** the ABOX identity word and the
  SPEEDY controller status. Even with every register reading back perfectly, the
  speaker stays silent: the Calliope core is not started, no firmware is loaded,
  no UAIF is clocked, the codec DAC is in reset, and the amp is unconfigured.
- "Registers readable" answers exactly one question — *is the audio block alive
  and what is it?* — and earns the right to write the next stage. It is not a
  playback result and must never be reported as one.

## What `0141` implements / stubs / leaves unknown

`xnu/patches/0141-jackpotlte-audio-probe.patch` adds
`bsd/dev/a5xelte_audio.{c,h}` and wires it through `bsd/conf/files` +
`iokit/bsddev/IOKitBSDInit.cpp` the same way `0138` does. A default boot is
unchanged: the probe returns immediately and silently unless `a8audio=1`.

**Implemented** (read-only, `a8audio=1` only)

- Maps the PMU and reads `pd_dispaud` (`0x11c84020`) raw and the ABOX CA7 state
  (`0x2524`, run/WFI/WFE bits) — always-on PMU, cannot stall an audio bus.
- Maps the ABOX SFR (`0x14A50000`) and reads `IP_INDEX`, `VERSION` (printed as
  the 4-char Calliope string), `SYSPOWER_CTRL`, `SYSPOWER_STATUS`.
- Maps the ABOX GIC and reads `GICD_CTLR` / `GICC_CTLR`.
- Maps the SPEEDY controller (`0x11CE0000`) and reads `CTRL`, `FIFO_STATUS`,
  `CTRL_STATUS` (AP-side registers).

**Stubbed**

- The codec ID / register read. The cod3035x has **no MMIO window**; it is a
  SPEEDY client. A real read needs a SPEEDY transaction, which is a driver, not
  a probe. The probe prints the DT identity and says so.
- The speaker amp (tfa98xx, I2C3) is not touched.
- Nothing is written anywhere; no firmware loaded; no clock or power enabled.

**Unknown / blocked** (name where to read it)

| Item | Read from |
| --- | --- |
| Is `pd_dispaud` powered at handoff, and is the ABOX clock-gated but powered? | `0141`'s PMU + `ABOX_CA7_STATUS` readback on the phone |
| Is the SPEEDY controller clocked at handoff (does its register read hang)? | same run; the probe reads it last on purpose |
| Does S-Boot leave the codec powered (l36_reg) and its SPEEDY pins muxed? | a scope / the codec reg read after a SPEEDY driver exists |
| Which codec register reports a device ID (if any) | `sound/soc/codecs/cod3033x.h` register map — no explicit ID reg; base reg `0x00` is a control reg (`cod3033x.c:579`) |
| How mediaserverd/CoreAudio binds a device on this OS version | the userspace side, not this tree |
| ABOX secure/TrustZone access model | `abox_gic.c:41-48,124-131` writes go through `exynos_smc(SMC_CMD_REG,…)`; a direct AP write may be rejected |

## Staged ladder (each stage its own patch; kill criteria included)

| # | Stage | Shape | Kill criterion |
| --- | --- | --- | --- |
| **A0** | **read-only identity probe** (this patch, `0141`) | `a8audio=1`: print ABOX version, syspower, CA7 state, SPEEDY ctrl | none — read-only; if the SPEEDY read hangs, drop that one access and keep the PMU/ABOX prints |
| A1 | codec identity over SPEEDY | minimal SPEEDY single-register read (base reg `0x00`), behind `a8audio=2` | SPEEDY transfer never completes → stop; the bus timing needs a datasheet, not guessing |
| A2 | ABOX power + clocks + firmware | enable `pd_dispaud`, `GATE_ABOX_ABOX`, load `calliope_dram.bin`/`calliope_sram.bin` into `abox_rmem`, release Calliope; expect `ABOX_VERSION` to go non-zero and CA7 `run=1` | firmware CRC/entry rejected, or CA7 never leaves reset → ABOX is secure-only; escalate to SMC |
| A3 | one UAIF + one sink, fixed tone | clock UAIF3 + configure tfa98xx (I2C3) or UAIF0 + cod3035x (SPEEDY) and play a canned PCM buffer | no BCLK on the pins, or amp/codec never leaves reset → analog/PMIC gap (l36_reg, TZ) |
| A4 | iOS integration | make mediaserverd/CoreAudio open the device (AppleEmbeddedAudio / HAL plug-in) | CoreAudio never enumerates it → userspace work, not kernel |

A0 is safe to try as soon as a BOOT image is available. A1–A3 are real driver
work. A4 is where "sound out of the speaker" actually happens, and it is not
started.

## Verification

The patch was produced with `git diff` in a private copy of
`/tmp/opencode/xnu-xnu-8796.141.3` with the repo's `0001..0139` applied
(the same recipe `docs/drafts/jackpotlte-wifi/PORT-START.md` uses).

- `git apply --check` of `0141` against a `0001..0139` tree: **applies**.
- `git apply --check` of `0141` against a `0001..0139 + 0140 (a5xelte_bt)` tree:
  **applies** (the two hunks in `bsd/conf/files` and `IOKitBSDInit.cpp` are
  anchored on stable neighbours, so they do not depend on 0140's insertion).
- The two patches that failed to apply in the scratch tree
  (`0031-sweet-read-write-root`, `0041-sweet-hfs-link-notes`) only touch the
  `bsd/sweet_hfs` tree staged by `0026-stage-hfs.sh`; they are unrelated to this
  patch's files.

Compile verdict is the CI run named in the handoff (this document does not claim
it passed until the run is green).

## Not verified

- Nothing has been compiled locally (XNU needs macOS tooling); CI is the first
  compiler to see `bsd/dev/a5xelte_audio.c`.
- Nothing has been booted. No claim is made that the ABOX or SPEEDY reads
  succeed, or that any register value above is what the A8 returns.
- The DT/register values are quoted from the donor vendor tree; they have not
  been checked against this unit's bootloader device tree.
- The `pd_dispaud`-is-powered inference (DECON shares the domain) is a
  *hypothesis* that the `0141` PMU readback is meant to test, not a finding.

## Next step

Build the `0141` kernel (`xnu.yml`, `EXTRA_MACHINE_FLAGS=-DJACKPOTLTE=1`) and
boot it **without** `a8audio` to confirm a default boot is unchanged and silent;
then boot with `a8audio=1` and read the ABOX version / CA7 state / SPEEDY
status lines on the framebuffer console. Those four lines decide A1 (SPEEDY
driver) versus A2 (ABOX power + Calliope).
