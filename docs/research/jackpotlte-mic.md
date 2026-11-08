# jackpotlte microphone capture (DMIC / ADC → ABOX)

Galaxy A8 2018 (`jackpotlte`, SM-A530F, Exynos 7885), XNU-8796.141.3 / Darwin 22
/ iOS 16 userspace.

This is the **capture** track only. Playback and the ABOX core (power, clocks,
DSP firmware, IPC) are a **separate** track (`jackpotlte-audio`). ABOX is a
**shared dependency**: nothing here powers it up, clocks it, loads its firmware
or talks to its CPU. This file records what the vendor tree says, what iOS would
need, and a read-only probe that observes the capture configuration without
touching the block.

**Status:** research + one read-only probe patch. Nothing has been compiled or
booted on the phone by this track. No device claim is made. Every address, IRQ,
pin and register below is quoted from Samsung's 4.4 vendor tree
(`/var/tmp/opencode-keep/Eureka-Kernel/`, "Release R24U sources", Linux
4.4.302) with `file:line`; anything the tree does not state is marked
**unknown**.

Sources quoted (paths relative to the vendor tree; DTB = `dtb` device tree,
DTBO = `dtbo` override overlay):

| tag | file |
| --- | --- |
| DT | `arch/arm64/boot/dts/exynos/dtb/exynos7885.dtsi` |
| DT-BOARD | `arch/arm64/boot/dts/exynos/dtb/exynos7885-jackpotlte_common.dtsi` |
| DT-PINCTRL | `arch/arm64/boot/dts/exynos/dtb/exynos7885-pinctrl.dtsi` |
| DT-PD | `arch/arm64/boot/dts/exynos/dtb/exynos7885-pm-domains.dtsi` |
| CLKID | `include/dt-bindings/clock/exynos7885.h` |
| PMUCAL | `drivers/soc/samsung/cal-if/exynos7885/pmucal_local_exynos7885.h` |
| PINCTRL | `drivers/pinctrl/samsung/pinctrl-exynos.c` |
| MACH | `sound/soc/samsung/exynos7885_cod3035.c` |
| CODEC | `sound/soc/codecs/cod3035x.c` / `cod3035x.h` |
| ABOX-H | `sound/soc/samsung/abox/abox.h` |
| ABOX | `sound/soc/samsung/abox/abox.c` |
| ABOX-WDMA | `sound/soc/samsung/abox/abox_wdma.c` |

## 1. What this board actually has

The A8 has **both** a digital-mic path and analog mic inputs, and they meet at
one codec, not at the SoC:

- The SoC-side capture engine is the **ABOX** audio DSP block. It has no DMIC
  interface of its own on the 7885: the DT/regmap expose **UAIF0..3** (I2S),
  **DSIF/SPDYIF** (Samsung serial audio) and **RDMA/WDMA** DMA channels, but no
  DMIC clock/data register (`ABOX-H:187-317`; `DT:1427-1594`).
- The microphones are wired to the **`cod3035x` audio codec** on the SPEEDY bus
  (`DT-BOARD:170-194`). That codec has DMIC1/DMIC2 digital inputs and
  MIC1/MIC2/MIC3/LINEIN analog inputs (`CODEC` register map
  `cod3035x.h:260-266`, DMIC mux `cod3035x.h:1147-1183,1232-1247`).
- The codec is bound to the SoC over **UAIF0** (I2S): the machine driver's UAIF0
  link pairs the ABOX `UAIF0` DAI with codec DAI `cod3035x-aif` (`MACH:566-580`).
- A single board pin, `gpg2-1`, named `aud_dmic_on` / `aud_dmic_off`, is driven
  as an **output** as part of the ABOX `default` pinctrl group (`DT:1434-1436`,
  `DT-PINCTRL:941-953`). It is one control line, not a clock/data pair, so it
  gates a DMIC path rather than carrying DMIC data.
- There is also an always-on voice-trigger chip, **dbmdx** (DSPG DBMD4) on
  SPI2 with its own VA mic config (`DT-BOARD:1801-1849`). That is a separate
  capture consumer and is out of scope here.

The DT does **not** say which physical microphone (earpiece/bottom DMIC vs.
headset analog) is wired to which codec input. That is **unknown** from source.

## 2. Hardware facts (quoted, not guessed)

### 2.1 SoC audio block (ABOX)

| fact | value | source |
| --- | --- | --- |
| ABOX SFR base | `0x14A50000`, size `0x10000` (`reg-names = "sfr"`) | DT:1427-1430 |
| ABOX sysreg | `0x14820000`, size `0x10000` (`"sysreg"`) | DT:1429-1430 |
| ABOX SRAM | `0x14B00000`, size `0x28000` (`"sram"`) | DT:1429-1430 |
| ABOX GIC distributor | `0x14AF1000`, size `0x1000` | DT:1420-1423 |
| ABOX GIC CPU interface | `0x14AF2000`, size `0x1004` | DT:1420-1423 |
| ABOX GIC IRQ | SPI 119 (`<0 119 0>`) → GIC INTID 151 | DT:1424 |
| ABOX sysmmu | `0x14A30000`, size `0x9000` | DT:1405-1407 |
| ABOX sysmmu IRQ | SPI 129 (`<0 129 0>`) → GIC INTID 161 | DT:1408 |
| ABOX power domain | `pd_dispaud` | DT:1437 |
| ABOX firmware blobs | `tfadsp.bin`, `AP_AUDIO_SLSI.bin`, `APBargeIn_AUDIO_SLSI.bin` | DT:1574-1593 |

### 2.2 Power and clocks (shared with the audio/ABOX track)

| fact | value | source |
| --- | --- | --- |
| `pd_dispaud` PMU reg | `0x11C84020`, size `0x20` | DT-PD:18-20 |
| `pd_dispaud` cal id | `0xB1380000`; `need_smc = <0x148F0000>` | DT-PD:21-24 |
| DISPAUD power CONFIG | `0x11C80000 + 0x4020`, mask `0xf` | PMUCAL:2,100 |
| DISPAUD power STATUS | `0x11C80000 + 0x4024`, mask `0xf` (`0xf` = on) | PMUCAL:3,101,105 |
| ABOX PLL / gates | `PLL_OUT_AUD`, `GATE_ABOX_ABOX`, `DOUT_CLK_ABOX_AUDIF`, `DOUT_CLK_ABOX_UAIF0`, `GATE_UAIF0` | DT:1444-1449 |
| Clock ids | `GATE_ABOX_ABOX` … `GATE_FM` = `CLK_DISPAUD_BASE(200)+0..6` | CLKID:115-122 |
| UAIF0 clock-gate (QCH) | `QCH_CON_ABOX_QCH_UAIF0` at `0x14800000+0x3010` | PMUCAL (dispaud_save) |
| ABOX clock-gate (QCH) | `DMYQCH_CON_ABOX_QCH_ABOX` at `0x14800000+0x3000` | PMUCAL (dispaud_save) |
| CMU_DISPAUD base | `0x14800000` (PLL_AUD / CLK_CON / QCH registers) | PMUCAL (dispaud_save) |

### 2.3 Codec, SPEEDY and microphones

| fact | value | source |
| --- | --- | --- |
| SPEEDY bus controller | `0x11CE0000`, size `0x2000`, IRQ SPI 34 (`<0 34 0>`) → INTID 66 | DT:664-673 |
| Codec node | `cod3035x@03` under `speedy@11CE0000`, `compatible = "codec,cod3035x"`, `reg = <0x03>` | DT-BOARD:170-174 |
| Codec supply | `vdd-supply = <&l36_reg>` (S2MPU08 LDO36) | DT-BOARD:176 |
| Mic biases | `mic-bias1-voltage=<3>`, `mic-bias2-voltage=<1>`, `mic-bias-ldo-voltage=<3>` | DT-BOARD:178-180 |
| Jack / button ADC | `use-btn-adc-mode`, `adc-ear` = ADC3, `adc-gdet` = ADC7, `mic-adc-range=<1404>` | DT-BOARD:181-193 |
| Sound card | `compatible = "samsung,exynos7885-cod3035"`, `mic-bias-mode = <0 0 2 0>` | DT-BOARD:1888-1891 |
| Capture route (FM) | `"ABOX SPEEDY Capture", "VINPUTFM"` | DT-BOARD:1898 |
| Capture route (call) | `"ABOX UAIF2 Capture", "VINPUTCALL"` | DT-BOARD:1897 |
| Speaker amp | `tfa98xx@34` on `i2c@13860000` (playback only) | DT-BOARD:1855-1861 |
| DMIC enable pin | `gpg2-1`, function 1 (output), `pin-val` 1 = on | DT-PINCTRL:941-953 |
| DMIC enable pin (board override) | both `aud_dmic_on` **and** `aud_dmic_off` set `pin-val = <1>` | DT-BOARD:1712-1724 |
| gpg2 bank base | offset `0x160` in pinctrl bank 3, 8 pins | PINCTRL:1743 |
| `bank_type_6` field offsets | CON `+0x00`, DAT `+0x04`, PUD `+0x08`, DRV `+0x0c` | PINCTRL:71-75 |
| gpg2 pinctrl window | `pinctrl@139B0000`, size `0x1000` | DT:631-633 |

Note the board override: the base pinctrl sets `aud_dmic_off` `pin-val = <0>`
(`DT-PINCTRL:952`), but the jackpotlte board file sets **both** the "on" and
"off" states to `pin-val = <1>` (`DT-BOARD:1712-1724`). So on this board the
line is held high in either state; whether that means the DMIC path is always
enabled or the pin drives a different net is **unknown** from source.

### 2.4 Capture engine registers (ABOX SFR, offsets from `0x14A50000`)

| fact | value | source |
| --- | --- | --- |
| UAIF base / stride | `0x500`, `0x10` | ABOX-H:187-188 |
| UAIFn CTRL0 / CTRL1 / STATUS | `+0x0` / `+0x4` / `+0xC` | ABOX-H:189-191 |
| UAIF CTRL0 `MIC_ENABLE` | bit 1; `MODE` bit 2; `START_FIFO_DIFF_MIC` bits 28-31 | ABOX-H:193-204 |
| UAIF CTRL1 `SLOT_MAX` | bits 18-20; `SBIT_MAX` bits 12-17; `VALID_STR` 6-11; `VALID_END` 0-5; `FORMAT` 24-28 | ABOX-H:208-232 |
| SPEEDY interface control | `SPDYIF_CTRL = 0x560` | ABOX-H:260 |
| DSIF control / status | `0x550` / `0x554` | ABOX-H:242-243 |
| WDMA base / stride | `0x2000`, `0x100` | ABOX-H:293-294 |
| WDMA `CTRL` / `STATUS` | `+0x0` / `+0x30` (`ENABLE` bit 0) | ABOX-H:295-307 |
| RDMA base / stride | `0x1000`, `0x100` (playback) | ABOX-H:263-264 |
| ABOX `SYSPOWER_CTRL` / `STATUS` | `0x10` / `0x14` | ABOX-H:29-30 |
| ABOX `ROUTE_CTRL0/1/2` | `0x40` / `0x44` / `0x48` | ABOX-H:36-38 |

So the capture registers this track cares about are:
**UAIF0 `+0x500/+0x504/+0x50C`** (I2S slot/format config) and
**WDMA0 `+0x2000/+0x2030`** (capture DMA enable/status); `ABOX_SYSPOWER_*` and
`ABOX_ROUTE_*` give the block and routing state.

### 2.5 Codec DMIC registers (inside `cod3035x`, on SPEEDY, not SoC MMIO)

| fact | value | source |
| --- | --- | --- |
| DMIC1 mux/enable | reg `0x49`: `SEL_DMIC_L` bits 4-6, `EN_DMIC` bit 3, `SEL_DMIC_R` bits 0-2 | `cod3035x.h:1147-1157` |
| DMIC source codes | `AMICL=0, AMICR=1, AMICC=2, ZEROD=3, DMIC1L=4, DMIC1R=5, DMIC2L=6, DMIC2R=7` | `cod3035x.h:1163-1166` |
| DMIC2 gain / OSR | reg `0x4A`: `DMIC_GAIN1` bits 4-6, `DMIC_OSR` bits 2-3 (`OSR64=1`) | `cod3035x.h:1168-1183` |
| DMIC3 (second pair) | reg `0x4E`: `DMIC_CLK_ZTE` bit 7, `SEL_DMIC_2L` 4-6, `SEL_DMIC_2R` 0-2 | `cod3035x.h:1232-1242` |
| DMIC4 gain | reg `0x4F`: `DMIC4_GAIN` bits 4-6 | `cod3035x.h:1245-1247` |

These are I2C/SPEEDY registers behind a codec driver, not SoC MMIO; there is no
XNU SPEEDY or cod3035x driver, so this track cannot read them.

## 3. Capture data path

```
 physical mic (DMIC or analog, unknown which)
        │  (codec-side DMIC clock/data, or analog ADC input)
        ▼
   cod3035x codec   (DMIC1/DMIC2 or MIC1..3/LINEIN → ADC)
        │  I2S ("cod3035x-aif")
        ▼
   ABOX UAIF0  ── ABOX routing (DSP/IPC) ──►  WDMA (capture DMA)
        │                                        │
        ▼                                        ▼
   (ABOX SRAM / SFR)                      AP DRAM ring buffer
```

- The codec↔SoC link is **UAIF0**: `MACH:566-580` pairs `cpu_dai_name =
  "UAIF0"` with `codec_dai_name = "cod3035x-aif"`, I2S, `CBS_CFS`
  (codec supplies BCLK/WS), `dpcm_playback = 1`, `dpcm_capture = 1`.
- Capture DMA is **WDMA**, not RDMA: `MACH:501-512` declares WDMA0 with
  `dpcm_capture = 1`, and `ABOX-WDMA:39` defines its stream as
  `SNDRV_PCM_STREAM_CAPTURE` (`IPC_PCMCAPTURE`). RDMA is playback
  (`abox_rdma.c:42`).
- The SPEEDY bus has its own capture DAI (`"ABOX SPEEDY Capture"`,
  `ABOX:1350-1365`), used here for FM/call routes (`DT-BOARD:1897-1898`), not
  for the mic.

## 4. What iOS needs, and what exists today

iOS capture normally goes **CoreAudio input → `mediaserverd` → the
`VirtualAudio` HAL plug-in → a hardware audio driver** (AppleHDA / the SoC
equivalent). On this port **none of that exists**:

- There is **no audio driver in the tree**: no `AppleHDA`, no ABOX driver, no
  codec driver (`grep` over `xnu/patches/` finds only unrelated hits, e.g. the
  SCSC `btabox` register name). Playback and capture both end at "no kernel
  audio path" today.
- The donor root tweak **removes the only audio HAL** and turns off the recorder
  path: `VirtualAudio.plugin` is moved to `HAL.disabled`
  (`tools/sweet-root-tweaks.py:110-117`) and every
  `SubServerConfiguration.plist` gets `StartRecorderServer = False`
  (`tools/sweet-root-tweaks.py:118-122`; rationale in the file header,
  `:19-20`). So even if a kernel path existed, userspace has **no capture
  client** wired today.

Consequence: "microphone works" requires, in order, (a) the ABOX/codec capture
path in the kernel (this track + the audio track), and (b) re-enabling a iOS
audio HAL and the recorder server (a root/userspace change, out of scope here).
Neither exists. This track produces only the read-only observation step.

## 5. Implemented / stubbed / unknown (this track)

**Implemented** — `xnu/patches/0145-jackpotlte-mic-probe.patch`, read-only and
off by default:

- Opt-in whole-file: `a5xelte_mic_probe()` returns immediately unless
  `a8mic=1`, before it maps anything, so a default boot is byte-for-byte the
  same as today.
- Reads the TOP pinctrl `gpg2` bank **CON/DAT** (`0x139b0160`/`0x139b0164`) and
  reports the `gpg2-1` DMIC-enable pin state (always-on block; the panel probe
  already reads this window).
- Reads the PMU `DISPAUD_CONFIGURATION`/`DISPAUD_STATUS`
  (`0x11c84020`/`0x11c84024`) to report whether `pd_dispaud` is powered
  (always-on PMU block).
- **Only if `DISPAUD_STATUS & 0xf` says the domain is already on**, maps the
  ABOX SFR (`0x14a50000`) and reads `SYSPOWER_CTRL/STATUS`, `ROUTE_CTRL0/1/2`,
  `UAIF0 CTRL0/CTRL1/STATUS`, `SPDYIF_CTRL`, and `WDMA0/WDMA1 CTRL/STATUS`.
  It **writes nothing**, powers nothing, clocks nothing and loads no firmware.

**Stubbed** — the probe does not decode every field, does not read the codec
(no SPEEDY driver), and does not touch the ABOX DSP/IPC or the routing state
machine.

**Unknown / blocked** (needs hardware or a driver this track does not own):

- Whether S-Boot leaves `pd_dispaud` on. The probe reports the status bit; the
  source cannot answer it. (Same shape as the GPU `pd_g3d` unknown.)
- The codec's DMIC clock rate / divider: no DT property; the codec datasheet is
  not in the tree. **unknown**.
- Which physical mic routes to DMIC1/DMIC2 vs. analog MICn: **unknown** from
  source.
- Whether a plain `UAIF0 → WDMA` register path captures without the ABOX DSP
  routing the vendor runs over IPC (`IPC_PCMCAPTURE`): **unknown**; the vendor
  capture path is driven through the ABOX DSP. This is the audio track's area.
- Whether `gpg2-1` must be low for DMIC off on this board (the override sets
  both states high): **unknown**.

## 6. Dependency on the audio / ABOX track (explicit boundary)

ABOX is **shared**. This track deliberately does **not**:

- power `pd_dispaud` on, sequence PMU/CAL, or release any reset;
- enable `PLL_OUT_AUD` / `GATE_ABOX_ABOX` / `GATE_UAIF0` or any QCH gate;
- load `tfadsp.bin` / `AP_AUDIO_SLSI.bin`, start the ABOX CA7, or use the ABOX
  mailbox/IPC;
- program `ABOX_ROUTE_CTRL*` or the DSP routing tables — routing a UAIF to a
  WDMA is the ABOX core's job;
- write any codec register (needs a SPEEDY + cod3035x driver).

The boundary: **the audio track owns everything needed to make ABOX and the
codec operational (power, clocks, firmware, IPC/routing, SPEEDY/codec). This
track owns only the capture-side observation — DMIC-enable pin, capture-DMA and
UAIF slot/format registers — and the iOS capture-path plan.** The probe reads
ABOX SFR registers *only after* the PMU status says the domain is already up, so
it never drives a bus into an unpowered block; if the audio track is not up, the
probe prints the pin + PMU state and stops.

Until the audio track's ABOX bring-up lands and is device-verified, this
track's ABOX reads are expected to print the "DISPAUD off" branch on a default
boot (or with `a8mic=1` before the audio work).

## 7. The probe (why it is safe)

- **Default-inert:** `a8mic=1` is parsed first and a zero value returns before
  any `ml_io_map`.
- **Always-on blocks only, unconditionally:** TOP pinctrl (`0x139b0000`) and
  PMU (`0x11c80000`). Both are read by existing probes (panel, keys) without
  incident.
- **Gated read of the powered block:** ABOX SFR is read only when
  `DISPAUD_STATUS & 0xf != 0`. Accessing a gated block can stall the bus, so the
  probe refuses to read it when the PMU says the domain is off.
- **No writes, no side effects:** only volatile loads + `dmb oshld`.
- Log prefix `A5XELTE MIC:` so the capture state is greppable in a serial log.

## 8. Ladder (each rung own evidence; kill criteria are explicit)

1. **Compile.** CI builds `0145` with `-DJACKPOTLTE=1`; the binary must contain
   `A5XELTE MIC:`. *Kill:* compile error or symbol missing → fix patch, no
   device claim.
2. **Default boot unchanged.** Boot without `a8mic`; no `A5XELTE MIC:` line.
   *Kill:* any `A5XELTE MIC:` output on a default boot → the gate is wrong.
3. **Probe reads the safe state.** Boot with `a8mic=1`; expect the `gpg2-1`
   con/dat line and the `dispaud cfg/status` line. *Kill:* hang or data abort →
   the PMU/pinctrl windows are not safe on this board; drop those reads.
4. **ABOX powered by the audio track.** Once `jackpotlte-audio` powers
   `pd_dispaud`, boot with `a8mic=1` and expect the `abox ... uaif0/wdma`
   register line. *Kill:* status says on but reading ABOX faults → block
   boundary/clock assumption is wrong; escalate to the audio track, do not add
   power code here.
5. **Capture bring-up (audio track + this track).** Route UAIF0 → WDMA and pull
   one period of PCM; this track only says what the UAIF slot/format registers
   report. *Kill:* no WDMA progress with codec output present → routing is
   DSP-IPC-only; that is the audio track's ladder, not this one.
6. **iOS capture.** Re-enable a capture HAL (un-disable `VirtualAudio`, or
   substitute a capture plug-in) and `StartRecorderServer`, then read one buffer
   through CoreAudio. *Kill:* userspace path still absent → the kernel is done
   but iOS is not; this is a root-tweak task.

## 9. Not verified

- `0145` has not been compiled on this host (XNU needs macOS tooling); CI is the
  only build evidence, and build success is not device evidence.
- Nothing has been flashed or booted by this track. The `a8mic=1` output has
  never been seen on the phone.
- The DT/register/IRQ values are quoted from the Samsung vendor tree; they have
  not been checked against this unit's bootloader-supplied live DT.
- The assumption that reading the PMU `DISPAUD_STATUS` bit is a valid proxy for
  "ABOX SFR is safe to read" is from the CAL power-up sequence
  (PMUCAL:2-3,100-101), not from a device readback.