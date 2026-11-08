# Galaxy A8 2018 (jackpotlte, Exynos 7885) — camera research and read-only probe

Date: 2026-11-08. Status: research + one opt-in, read-only probe
(`xnu/patches/0142-jackpotlte-camera-probe.patch`). No streaming, no sensor
register writes, no capture, no device claims. Every hardware value below
cites the vendor tree at `/var/tmp/opencode-keep/Eureka-Kernel/` as
`file:line`. Where the vendor tree does not state a value, the entry says
`unknown` and names the file that would answer it.

## 0. Verdict up front

Two separate questions, two separate answers, and they should not be
confused:

1. **Can the SoC-side camera path be probed and eventually driven from XNU?**
   Yes, plausibly. The sensor buses, the CSI receivers, the ISP and the
   power domains are all documented in the vendor device tree and driver
   source with concrete addresses. This document pins them down and ships a
   probe that reads the state S-Boot leaves them in.

2. **Does that give the A8 a working camera on iOS?** No, not close. A
   sensor + CSIS + ISP driver produces raw frames in memory. What iOS camera
   *clients* talk to is Apple's closed camera stack (the
   `AppleCameraInterface`-class kernel service plus the userland
   `cameraserverd`/AVFoundation side). None of that exists on this port and
   none of it can be reconstructed from the vendor tree. Section 4 spells
   out exactly which pieces are missing and which are merely unknown.

The honest summary: the probe below is the correct *first* step, and it is
several engineering stages away from "frames in memory", which is itself
several stages away from "the Camera app opens".

## 1. Safety contract for this track

- The probe is compiled only for `ARM64_BOARD_CONFIG_A5XELTE && JACKPOTLTE`.
- Default boot is unchanged: with no boot argument the entry point returns
  immediately. Nothing is mapped, read or written.
- `a8cam=1` enables the **safe** report only: PMU power-domain registers,
  pinctrl pin state, and HSI2C controller state. All three blocks are in
  always-on or already-powered domains (see §5.1), so this level performs
  no writes and cannot disturb a running system.
- `a8cam=2` additionally reads the camera power domain (CSIS, sysMMU) — but
  **only after** `PMU + 0x4084` reports `CAM_STATUS == 0xF`, i.e. the domain
  is fully powered and reset-released (`pmucal_local_exynos7885.h:269-271`).
  If the domain is down the probe logs that and stops. This gate exists
  because an MMIO read from an unpowered Exynos block can stall the bus or
  raise SError, which XNU has no handler for.
- Nothing in this patch writes a sensor register, a rail, a clock, a reset
  line, or an interrupt mask. There is no I2C transfer at all.
- Bootloaders, EFS, recovery and the GPT are untouched. The patch changes
  only `bsd/dev/a5xelte_camera.c` (new), `bsd/conf/files` and
  `iokit/bsddev/IOKitBSDInit.cpp`.

## 2. Hardware as the vendor tree describes it

### 2.1 Sensors and their buses

Three image sensors are populated. The DTS `reg` value is authoritative; the
node *names* in the same file are copy-paste artefacts (see §2.6).

| Role | Part | Vendor name id | I2C bus (controller base) | 7-bit addr | DTS |
|---|---|---|---|---|---|
| Rear | Samsung S5K2P6 | `SENSOR_NAME_S5K2P6` = 58 | `hsi2c_1` @ `0x138B0000` | `0x2d` | `exynos7885-jackpotlte_common.dtsi:1601-1609` |
| Front | Samsung S5K3P8SP | `SENSOR_NAME_S5K3P8SP` = 44 | `hsi2c_0` @ `0x138A0000` | `0x10` | `...:1646-1653` |
| Front-2 | Samsung SR846 | `SENSOR_NAME_SR846` = 208 | `hsi2c_3` @ `0x138D0000` | `0x21` | `...:1665-1671` |

Name ids: `drivers/media/platform/exynos/fimc-is2/include/exynos-fimc-is-sensor.h:103,106,141`.
Bus label mismatch is real: `exynos7885.dtsi:821,839,857,875` labels
`hsi2c_0..3` "REARSENSOR / REARAF / FRONTSENSOR / DEPTHSENSOR", but the
board file puts the rear CIS on `hsi2c_1` and the front CIS on `hsi2c_0`.
The board file wins (it is the one that is `status = "okay"` with a populated
child node).

`hsi2c_2` @ `0x138C0000` (`...:1612-1636`) carries no image sensor:

| Device | Type | 7-bit addr | Line |
|---|---|---|---|
| AK7372 | rear AF actuator | `0x0c` | `exynos7885-jackpotlte_common.dtsi:1620-1625` |
| rear EEPROM | `rear-eeprom-i2c` | `0x50` | `...:1627-1630` |
| front EEPROM | `front-eeprom-i2c` | `0x51` | `...:1632-1635` |

Note `...:1620` names the node `fimc-is-actuator@18` while `reg = <0x0C>`
("7bit-addr", `...:1622`); again the `reg` is authoritative.

### 2.2 Sensor pin map (reset, rails, MCLK)

All three sensors are `bank_type_6` pins, so CON/DAT/PUD live at
`+0x00/+0x04/+0x08` with a 4-bit function field per pin
(`drivers/pinctrl/samsung/pinctrl-exynos.c:72-75`).

| Signal | Rear 2P6 | Front 3P8SP | Front-2 SR846 | Line |
|---|---|---|---|---|
| `gpio_reset` | `gpf3-2` | `gpf3-0` | `gpf3-1` | `...:1453,1505,1526` |
| `gpio_mclk` | `gpc0-0` (ch 0) | `gpc0-1` (ch 1) | `gpc0-2` (ch 2) | `...:1452,1504,1525` |
| core/LDO enable | `gpp0-2` (`RCAM_LDO_EN`) | `gpp0-0` (`FCAM1`) | `gpp0-1` (`gpp0-1` "all LDOs") | `...:1454,1507,1527` |
| IO LDO enable | `gpg2-0` (`RCAM_IO_LDO_EN`) | `gpg2-0` (`gpio_rcam_en`) | — | `...:1455,1506` |
| AF 2.8 V enable | `gpp0-3` (`RCAM_AF_2P8_EN`) | — | — | `...:1456` |
| extra rail | — | `gpp0-1` (`FCAM2`) | — | `...:1508` |

Bank offsets: TOP pinctrl `0x139b0000` — `gpp0` `0x000`, `gpg2` `0x160`,
`gpc0` `0x1c0` (`pinctrl-exynos.c:1731-1750`); FSYS pinctrl `0x13430000` —
`gpf3` `0x040` (`pinctrl-exynos.c:1723-1729`). Controller bases:
`exynos7885.dtsi:617-624` (FSYS) and `:631-633` (TOP).

MCLK pinmux (`exynos7885-pinctrl.dtsi:899-939`): `gpc0-0/1/2` are
`pin-function 1` = "mclk out" (idle, driven low) and `pin-function 2` =
"MCLK" (clock function). So the CON field of `gpc0` tells us whether S-Boot
left any sensor clock selected — that is one of the values the probe reports.

Sensor I2C pins are `gpc1-0..7`, `pin-function 2`
(`exynos7885-pinctrl.dtsi:635-672`), i.e. the CON fields of `gpc1` say
whether the sensor buses are muxed at handoff.

### 2.3 Sensor power-on order (vendor)

`fimc-is-device-module-2p6.c:205-233` is the reference sequence, and it is
the same shape for all three parts:

1. reset low (`:206`);
2. AVDD 2.8 V (`:208-212`), 200 µs;
3. DVDD 1.05 V (`:214-218`), 200 µs;
4. IO 1.8 V (`:220-224`), 5000 µs on 2P6 / 500 µs via regulator;
5. AF 2.8 V (`:226-230`), 7000 µs;
6. reset high (`:232`), 500 µs;
7. mux `gpio_mclk` to function 2 (`:233`), 5000 µs.

Order matters in the vendor code but the delays are the vendor's chosen
values, not datasheet constraints we can verify from this tree.

### 2.4 CSI receivers and the ISP cluster

`fimc_is@14450000` owns the camera-side cluster
(`exynos7885-camera.dtsi:22-79`):

| Block | Base | IRQ | Line |
|---|---|---|---|
| CSIS-DMA | `0x14440000` | — | `:24` |
| FIMC-3AA0 | `0x14450000` | 301, 302 | `:25,31-32` |
| FIMC-3AA1 | `0x14460000` | 303, 304 | `:26,33-34` |
| FIMC-ISP | `0x14600000` | 310, 311 | `:27,35-36` |
| MC-SCALER | `0x14640000` | 314 | `:28,37` |
| FIMC-VRA A | `0x14610000` | 312 | `:29,38` |
| FIMC-VRA B | `0x14620000` | — | `:30` |

CSI receivers, one per physical channel
(`exynos7885-camera.dtsi:118-236`):

| CSIS | Base | IRQ | Used by |
|---|---|---|---|
| CSIS0 | `0x14400000` | 297 | front S5K3P8SP (`id = 1`, `csi_ch = 0`) |
| CSIS1 | `0x14410000` | 298 | rear S5K2P6 (`id = 0`, `csi_ch = 1`) |
| CSIS2 | `0x14420000` | 299 | front-2 SR846 (`id = 2`, `csi_ch = 2`) |

The sensor→channel assignment comes from the board file, not the SoC file:
`exynos7885-jackpotlte_common.dtsi:1561-1591` sets `id` and `csi_ch`
(`:1564` rear = `csi_ch 1`, `:1576` front = `csi_ch 0`, `:1586` front-2 =
`csi_ch 2`). The rear camera therefore streams into **CSIS1**, not CSIS0.

MIPI D-PHY blocks (`exynos7885-camera.dtsi:81-116`): rear uses
`mipi_phy_csis_m0s4A` (PMU isolation offset `0x0680`, reset bit 8), front
uses `m0s4B` (`0x067c`, bit 16), front-2 uses `m0s4C` (`0x067c`, bit 24).

CSIS register identity used by this probe
(`drivers/media/platform/exynos/fimc-is2/sensor/csi2/fimc-is-hw-csi-v5_0.h`):
`CSIS_VERSION` offset `0x0000` (`:204`), `PHY_STATUS` offset `0x0700`
(`:243`). This is the `CSIS_V5_0` register map (`:14`).

### 2.5 System MMUs

| Instance | Base | IRQ | Line |
|---|---|---|---|
| `sysmmu_cam` | `0x14540000` | 307, 308 | `exynos7885.dtsi:1609-1615` |
| `sysmmu_isp0` | `0x14730000` | 317, 318 | `exynos7885.dtsi:1661-1667` |
| `sysmmu_isp1` | `0x14760000` | 321, 322 | `exynos7885.dtsi:1683-1689` |

Register layout (`drivers/iommu/exynos-iommu.h:98-106`): `MMU_CTRL 0x000`,
`MMU_CFG 0x004`, `MMU_STATUS 0x008`, `MMU_VERSION 0x034`. The enable value
is `CTRL_ENABLE 0x5`.

### 2.6 Power domains

PMU syscon at `0x11c80000`, size `0x10000` (`exynos7885.dtsi:1289-1292`).
Camera domains (`exynos7885-pm-domains.dtsi:34-48`):

| Domain | PMU config | PMU status | ACPM cal_id | Parent |
|---|---|---|---|---|
| `pd_isp` | `0x11c84060` | `0x11c84064` | `0xB1380002` | — |
| `pd_cam` | `0x11c84080` | `0x11c84084` | `0xB1380003` | `pd_isp` |

The *truth* for "is this domain on" in the vendor kernel is `cal_pd_status()`
via ACPM firmware (`drivers/soc/samsung/exynos-pd.c:50-61`), and XNU has no
ACPM. But the local power sequence tables give the same answer straight from
silicon: `isp_on`/`cam_on` write the configuration word with mask `0xf` and
wait for the status word to read `0xF`
(`pmucal_local_exynos7885.h:160-163,217-220`), and `isp_status`/`cam_status`
read the status word with the same mask (`:213-215,269-271`). So
`PMU + 0x4084 & 0xf == 0xf` means `pd_cam` is up, and the probe can decide
safely without firmware. This is the same idiom the Mali research doc uses
for `pd_g3d` (`0x40a0`/`0x40a4`, `pmucal_local_exynos7885.h:273-276`).

The CAM clock controller (`CMU_CAM`) is at `0x14500000`: the CAM Q-channel
gate registers are `CMU_CAM + 0x3004` (CMU), `+0x3008` (3AA0), `+0x300c`
(3AA1), `+0x3010` (CSIS0), `+0x3014` (CSIS1), `+0x3018` (CSIS2), `+0x301c`
(CSIS3), `+0x3020` (CSIS-DMA), `+0x3028` (SMMU), with readback mirrors at
`+0x7004..0x7028` (`pmucal_local_exynos7885.h:224-256`). The ISP side is
`CMU_ISP` at `0x14700000` (`:165-196`).

### 2.7 Sensor MCLK (CIS clock) generation

This is the one piece of the chain that is *not* in the device tree at all.
The vendor clock framework generates it:

| Stage | Register | Field | Line |
|---|---|---|---|
| mux `CIS_CLK0` | `CMU_CAM + 0x1008` | bit0 SELECT (0 = `OSCCLK_CMU`, 1 = `PLL_SHARED0_DIV4`), bit16 BUSY | `cmucal-sfr.c:196,1525-1527` |
| mux `CIS_CLK1` | `CMU_CAM + 0x100c` | same | `cmucal-sfr.c:197` |
| mux `CIS_CLK2` | `CMU_CAM + 0x1010` | same | `cmucal-sfr.c:198` |
| div `CIS_CLK0` | `CMU_CAM + 0x1810` | bits[4:0] DIVRATIO, bit16 BUSY | `cmucal-sfr.c:190,1507-1509` |
| div `CIS_CLK1` | `CMU_CAM + 0x1814` | same | `cmucal-sfr.c:191` |
| div `CIS_CLK2` | `CMU_CAM + 0x1818` | same | `cmucal-sfr.c:192` |
| gate `CIS_CLK0` | `CMU_CAM + 0x2010` | bit21 CG_VAL (1 = clock on), bit20 MANUAL, bit28 auto-gating | `cmucal-sfr.c:193,1516-1518` |
| gate `CIS_CLK1` | `CMU_CAM + 0x2014` | same | `cmucal-sfr.c:194` |
| gate `CIS_CLK2` | `CMU_CAM + 0x2018` | same | `cmucal-sfr.c:195` |

Parent choices: `cmucal_mux_clkcmu_cis_clk0_parents[] = { OSCCLK_CMU,
PLL_SHARED0_DIV4 }` (`cmucal-node.c:131-134`). The node graph is
`VCLK(CIS_CLK0) -> VCLK_CLKCMU_CIS_CLK0_BLK_CMU`
(`drivers/clk/samsung/clk-exynos7885.c:197-199`), and the board file wires
`CIS_CLK0/1/2` into `fimc_is` and each sensor node
(`exynos7885-camera.dtsi:55-57,126-128`).

**MCLK rate: `unknown`.** The vendor stack takes it from
`sensorInfo.ext_clk` (`fimc-is-cis.h:20`), which is only ever *consumed*
(`fimc-is-cis-2p6.c:128` `gpsSensorExInfo->uiMCLK = sensorInfo.ext_clk;`) and
is never assigned anywhere in this tree — no `ext_clk =` write, no
`ext_clk` device-tree property, no `clk_set_rate` call
(`grep -rn "ext_clk\s*=" drivers/media/platform/exynos/fimc-is2/` → empty).
The value comes from Samsung's generated sensor setfiles, which are not in
this tree. To recover it: read the CIS registers of a sensor that is running
under Android (mode/pll setfile), or measure `gpc0` on hardware. Do not
guess 24 MHz and treat it as fact.

### 2.8 HSI2C controller (how a chip-ID read would work)

`drivers/i2c/busses/i2c-exynos5.c` is the vendor driver for all four sensor
buses. Register map at `:59-82`, bit definitions at `:96-190`:
`HSI2C_CTL 0x00` (`MASTER` bit3, `RXCHON` bit6, `TXCHON` bit7, `SW_RST`
bit31), `HSI2C_FIFO_CTL 0x04`, `HSI2C_CLK_CTL 0x0c`, `HSI2C_INT_ENABLE 0x20`,
`HSI2C_INT_STATUS 0x24`, `HSI2C_ERR_STATUS 0x2c`, `HSI2C_FIFO_STATUS 0x30`,
`HSI2C_TX_DATA 0x34`, `HSI2C_RX_DATA 0x38`, `HSI2C_CONF 0x40` (`AUTO_MODE`
bit31, `HS_MODE` bit29), `HSI2C_AUTO_CONF 0x44` (`READ_WRITE` bit16,
`STOP_AFTER_TRANS` bit17, `MASTER_RUN` bit31), `HSI2C_TIMEOUT 0x48`,
`HSI2C_MANUAL_CMD 0x4c`, `HSI2C_TRANS_STATUS 0x50` (`MASTER_BUSY` bit17),
`HSI2C_TIMING_* 0x54..0x6c`, `HSI2C_ADDR 0x70` (slave address in bits
`[16:10]`, `:998`).

The polling-mode transfer is `exynos5_i2c_xfer_msg()` (`:926-1130`); auto
mode is enabled by `exynos5_i2c_init()` (`:657-673`) and timings are computed
from the input clock in `exynos5_i2c_set_timing()` (`:500-630`). The sensor
register access pattern is a two-message transfer with a **2-byte,
big-endian register address**: `fimc_is_sensor_read8()` sends
`{addr[15:8], addr[7:0]}` then reads 1 byte (`fimc-is-helper-i2c.c:121-159`);
`fimc_is_sensor_read16()` reads 2 bytes and returns `(rbuf[0] << 8) |
rbuf[1]` (`:161-202`).

Chip identity, per vendor:
`fimc-is-cis-2p6.c:353-356` (2P6: `model_id` at `0x0000`, 16-bit),
`fimc-is-cis-3p8sp.c:326-330` (3P8SP: `model_id` at `0x0000`, 16-bit),
`fimc-is-cis-sr846.c:271-274` (SR846: `sensor_id` at `0x0f14`, 8-bit, and
`model_id` at `0x0f16`, 16-bit).

There is already a working HSI2C polling implementation in this tree to copy
from: `bsd/dev/a5xelte_touch.c` (`a5pmic_bus_init`, `a5pmic_message`) drives
the A5's HSI2C3 in polling mode with no registered interrupt. It is tuned
for the Exynos7580 timing values, so the 7885 equivalents must be computed
from the actual `GATE_HSI2C*` input rate (`clk-exynos7885.c:166-169`) before
it can be reused as-is.

## 3. What this port has today

There is no camera code in the XNU tree before this patch. The relevant
prior art that the probe is built from:

| Piece | File | What it gives us |
|---|---|---|
| MMIO read/write + `dmb` fences | `bsd/dev/a5xelte_panel.c` (patch 0133) | the exact helper idiom, `ml_io_map`, bounded polls |
| Pinctrl CON/DAT/PUD interpretation | `bsd/dev/a5xelte_touch.c` (patch 0130) | bank offsets, 4-bit function fields, "read the pin, don't drive it" |
| Boot-arg gated opt-in probe | `a5xelte_touch.c`, `a5xelte_scsc.c` | `PE_parse_boot_argn("a8…")` and early return |
| PMU power-domain read | `a5xelte_scsc.c` (patch 0138) | `ml_io_map(0x11c80000)`, read-only PMU access |
| New-file wiring | patch 0138 | `bsd/conf/files` line + `IOKitBSDInit.cpp` extern + call |

Nothing in the tree reads CSIS, ISP, sysMMU or any sensor register today.

## 4. What a *working* camera would additionally require (iOS side)

This is the part that decides the outcome, so it is stated plainly.

The kernel-side driver this port could write is a *sensor + capture* driver:
power rails, MCLK, I2C chip-ID and mode programming, CSIS configuration,
DMA into a buffer, and a way to hand frames out. That is stage 2-4 in §5.2
and it is a large but ordinary piece of driver work.

What iOS camera clients actually use is not that. On iOS the camera is
reached through Apple's closed stack:

- a kernel service in the `AppleCameraInterface` family that owns the
  capture pipelines and exposes them to userland;
- the `cameraserverd` / AVFoundation userland side, which is built against
  Apple's private camera framework and its XPC protocols;
- Apple's ISP firmware / pipeline configuration for the specific sensor
  (auto-exposure, lens shading, white balance, the "still image" path).

None of that is present in this tree, and none of it can be derived from
the Samsung vendor sources. Consequences:

- **Reachable with effort:** raw frames in memory, sensor identification,
  register-level bring-up, MIPI/CSIS lock, and a debug path that writes
  frames to a known buffer.
- **Not reachable without Apple binaries:** the Camera app opening, photo
  capture, video capture, or any AVCaptureSession client. A third-party
  userland camera app on iOS still goes through the same daemon.

So the correct framing for this track is "camera hardware bring-up", not
"camera support". Anything that implies a working iOS camera is wrong.

## 5. The staged ladder

### 5.1 Implemented by patch 0142 (this commit)

`bsd/dev/a5xelte_camera.c`, called from `IOKitBSDInit.cpp` next to the other
A8 probes, default inert.

`a8cam=1` prints, all read-only:

1. **PMU** `pd_isp`/`pd_cam` config and status words
   (`0x11c84060/64`, `0x11c84080/84`) and whether each is in the `0xf`
   "fully on" state.
2. **Pin state** for every camera pin in §2.2: raw CON/DAT/PUD plus a
   per-pin decode (reset level, rail-enable level, `gpc0` function select =
   MCLK idle vs MCLK function). This answers "did S-Boot leave the sensor
   rails and clock pins configured?" without touching anything.
3. **HSI2C** controller state for `0x138a0000..0x138d0000`: CTL, CONF,
   AUTO_CONF, FIFO_STATUS, TRANS_STATUS, ADDR. This answers "did S-Boot
   leave the sensor buses clocked and configured?" — a controller that reads
   back all-zero was never clocked.

`a8cam=2` additionally, **only if** `PMU + 0x4084 & 0xf == 0xf`:

4. **CSIS** `VERSION` (`+0x0000`) and `PHY_STATUS` (`+0x0700`) for CSIS0,
   CSIS1 and CSIS2.
5. **sysMMU** `MMU_VERSION`/`MMU_CTRL`/`MMU_STATUS` for `sysmmu_cam`,
   `sysmmu_isp0`, `sysmmu_isp1`.
6. **CMU_CAM** Q-channel readback (`0x14500000 + 0x7004..0x7028`) so the
   per-block clock-gate state is logged as well as the domain state.

If `CAM_STATUS != 0xf` the probe says so and skips 4-6 entirely.

### 5.2 Designed but NOT implemented (in order)

**Stage 1 — rails, MCLK, reset, I2C identity.** For one sensor (rear 2P6
first): drive `gpp0-2`/`gpg2-0`/`gpp0-3` high in the vendor order (§2.3),
program `CIS_CLK0` (§2.7) at the rate taken from `sensorInfo.ext_clk`
(`unknown`, §2.7), select `gpc0-0` function 2, release `gpf3-2`, then read
`0x0000` (16-bit) over `hsi2c_1` at `0x2d`. Success criterion: a `model_id`
consistent with S5K2P6. This is the first stage that writes anything, and it
must stay behind its own boot argument (e.g. `a8cam=3`) plus a matching
teardown that returns the rails and clock to their prior state.

**Stage 2 — CSIS configuration and lock.** Configure CSIS1 (rear) from the
vendor setfile path, enable the D-PHY, and confirm `PHY_STATUS` reports lane
lock with the sensor streaming. Needs the sensor's MIPI timing, which lives
in the closed setfiles — plan on capturing it from a running Android or
deriving it from the sensor datasheet, not from this tree.

**Stage 3 — capture to memory.** CSIS-DMA descriptor programming and the
first frame in a buffer. This is the first point at which "frames exist".

**Stage 4 — ISP / 3AA post-processing.** Only if stage 3 produces data that
is usable at capture resolution.

**Stage 5 — iOS glue.** Out of scope for the reasons in §4.

### 5.3 Known unknowns

| Unknown | Why it matters | Where to look |
|---|---|---|
| Sensor MCLK rate | MCLK must be exact or the sensor ignores I2C | `sensorInfo.ext_clk`, `fimc-is-cis.h:20`; not assigned in this tree — read from a live Android sensor or the setfile |
| Sensor mode/PLL setfiles | Without them the sensor cannot be put in a streaming mode | Samsung-generated setfiles, not in this tree |
| CSIS/VRA/MCSC register semantics | Only the register *map* is here, not the programming sequences | `fimc-is-hw-csi-v5_0.h` names offsets; the sequences are in the closed ISP firmware / `fimc-is-hw-csi-v5_0.c` partially |
| PMU isolation offsets `0x067c`/`0x0680` bit semantics | Needed to release the MIPI PHYs deterministically | `exynos7885-camera.dtsi:81-116` gives offset+bit; the surrounding bits are undocumented |
| `pd_cam`/`pd_isp` on-time | The local sequence only says "wait for 0xf", no timing | `pmucal_local_exynos7885.h:160-163,217-220` |
| HSI2C input clock rate on 7885 | Needed to compute 400 kHz timing words | `clk-exynos7885.c:166-169`; rate from `CMU_PERI` dividers, not stated as a number |

## 6. Reachability verdict

- The probe in this patch is safe, opt-in, and tells us something real on
  the first boot with `a8cam=1`: whether S-Boot hands over the camera pin
  muxing, rails and sensor buses in a usable state, and whether the camera
  power domains are already up.
- `a8cam=2` is the first camera-domain experiment. It is gated on the PMU
  status word precisely so that it cannot be a crash button on a
  camera-powered-off boot.
- Everything past that (§5.2) is ordinary but substantial driver work, and
  the two hard dependencies — sensor setfiles and the MCLK rate — are
  `unknown` in this tree.
- Even in the best case the ceiling is raw frames in memory. A working iOS
  camera needs Apple's closed capture stack (§4) and is not a realistic
  goal for this port.

Read this document as "the hardware is mapped, the first probe is safe,
here is the ladder", not as "camera support is coming".