# jackpotlte "easy" sensors — research + first read-only probe

Scope: the simple user-facing sensors of the Galaxy A8 2018 (`jackpotlte`,
SM-A530F, Exynos 7885): **accelerometer, gyroscope, magnetometer,
ambient-light, proximity, barometer, hall (cover flip) and grip**. This is the
sensors track only; it touches no other subsystem.

Every address, register and part name below cites `file:line` in the shared
vendor tree `/var/tmp/opencode-keep/Eureka-Kernel/` (Samsung 4.4.302, "Release
R24U sources"), the donor that the rest of this port already reads. Nothing is
invented; where the vendor tree does not encode a value it says **unknown**.

> **The single central finding:** on this board the accelerometer, gyroscope,
> magnetometer, ambient-light, proximity and barometer are **not** I2C chips on
> the AP bus. They sit behind a Samsung SSP **sensor-hub MCU** (a STM32F on
> SPI4). Only the **grip** sensor is a direct I2C part on the AP, and only the
> **hall** is a plain GPIO. A "probe the I2C WHO_AM_I of each sensor" model does
> not fit this device.

## Sources

| tag | file (under the vendor tree) |
| --- | --- |
| DT | `arch/arm64/boot/dts/exynos/dtb/exynos7885.dtsi` |
| JDT | `arch/arm64/boot/dts/exynos/dtb/exynos7885-jackpotlte_common.dtsi` |
| EURDTS | `arch/arm64/boot/dts/exynos/dtb/exynos7885-jackpotlte_eur_open_03.dts` |
| CFG | `arch/arm64/configs/full/exynos7885-jackpotlte_defconfig` |
| PIN | `drivers/pinctrl/samsung/pinctrl-exynos.c` |
| ABOV | `drivers/input/keyboard/abov_touchkey_3x6.c` |
| SSPDEV | `drivers/sensorhub/stm32/ssp_dev.h` |
| SSPH | `drivers/sensorhub/stm32/ssp.h` |
| SSPI | `drivers/sensorhub/stm32/ssp_spi.c` |
| SSPSL | `drivers/sensorhub/stm32/ssp_sensorlist.h` |
| TMD | `drivers/sensors/tmd3725.{c,h}` |
| FACT | `drivers/sensorhub/stm32/factory/…` |

## Sensor table

`bus` is the AP-side bus the sensor is reachable on. For the hub sensors the
AP can only reach the **hub**, not the chip.

| sensor | part | AP bus | address | id register | expect | source |
| --- | --- | --- | --- | --- | --- | --- |
| accelerometer | LSM6DSL | **hub**: SPI4 `0x13980000`, CS `gpp8-0` | hub slave 0 | none on AP (hub-internal) | unknown | CFG:4406, FACT:Accelometer/accel_lsm6dsl.c:32, EURDTS:40-43 |
| gyroscope | LSM6DSL | hub: SPI4 `0x13980000` | hub slave 0 | none on AP | unknown | CFG:4410, FACT:Gyroscope/gyro_lsm6dsl.c:41 |
| magnetometer | AK09918C | hub: SPI4 `0x13980000` | hub slave 0 | none on AP | unknown | CFG:4414, FACT:Magnetic/magnetic_ak09918c.c:93 |
| ambient light | TMD3725 | hub: SPI4 `0x13980000` | hub slave 0 | chip `CHIPID 0x92`, but read **by the hub** | `0xE4` | CFG:4424, FACT:Light/light_tmd3725.c:23, TMD:tmd3725.h:40 + tmd3725.c:68 |
| proximity | TMD3725 (auto-cal) | hub: SPI4 `0x13980000` | hub slave 0 | chip `CHIPID 0x92` (hub-side) | `0xE4` | CFG:4419, FACT:Proximity/prox_tmd3725.c:53 |
| barometer | LPS22H | hub: SPI4 `0x13980000` | hub slave 0 | none on AP | unknown | CFG:4426, FACT:Barometer/barometer_lps22h.c:29 |
| **hub MCU** | STMicro STM32F (`ssp,STM32F`) | **SPI4** `0x13980000`, CS `gpp8-0` | SPI CS 0 | SSP cmd `WHOAMI 0x0F` → 1 byte | `0x55` | EURDTS:29-41, SSPH:88, SSPI:533-560, SSPDEV:28 |
| hall (cover) | hall switch (part not named in DT) | **GPIO** pinctrl_0 `0x11CB0000` | `gpa0-3` (DAT bit 3) | none (GPIO level) | open/closed | JDT:843-852, DT:601, PIN:1709 |
| certify_hall | hall switch | GPIO pinctrl_0 | `gpa0-4` (DAT bit 4) | none | open/closed | JDT:853-857 |
| grip | **Abov A96T3X6** | **I2C5** (s3c2440) `0x13880000` | `0x20` | `VENDORID 0x12`, `MODEL_NO 0x14`, `FW_VER 0x01` | none encoded | JDT:722-748, DT:1077-1079, ABOV:60-68 |

### The SSP sensor hub (accel / gyro / mag / light / prox / baro)

* The board node is `spi@13980000` in each variant DTS, with a `STM32F@0`
  child `compatible = "ssp,STM32F"` and `spi-max-frequency = <8000000>`
  (EURDTS:29-43). `0x13980000` is USI2_CH0, which the SoC can drive as SPI or
  hsi2c; the board selects **SPI** (DT:969-985 gives the hsi2c personality,
  `status = "disabled"`).
* The hub's own identification is not a memory-mapped WHO_AM_I: the AP sends the
  SSP command `MSG2SSP_AP_WHOAMI` (`0x0F`, SSPH:88) over SPI and expects one
  byte `DEVICE_ID` = `0x55` (SSPDEV:28); `get_chipid()` retries twice on a
  mismatch (SSPI:533-560).
* The hub multiplexes 20 logical sensors (SSPSL:35-54); the six this board wires
  are `accelerometer_sensor`, `gyro_sensor`, `geomagnetic_sensor`,
  `light_sensor`, `proximity_sensor`, `pressure_sensor` (SSPSL:36-41).
* The chip part numbers are chosen by the **hub firmware** and appear only as
  strings in the hub "factory" accessors — `LSM6DSL`, `AK09918C`, `TMD3725`,
  `LPS22H` (`FACT:*_name()`, cited above) — matching the defconfig
  `CONFIG_SENSORS_SSP_*=y` block (CFG:4394-4428). The AP never issues a
  WHO_AM_I to those chips.
* The only chip ID a direct (non-hub) vendor driver encodes is the TMD3725's:
  register `CHIPID 0x92`, value `0xE4` (TMD:tmd3725.h:40, tmd3725.c:68). That
  value is for a *different* driver that this board does **not** build
  (`# CONFIG_SENSORS_TMD3725 is not set`, CFG:4450).

### Grip (Abov A96T3X6)

* The switch is a single I2C part on **I2C5** `i2c@13880000` (a
  `samsung,s3c2440-i2c` controller, DT:1077-1079) at slave **`0x20`**
  (JDT:732-738), with `grip_ldo_en` on `gpg2-5` (JDT:696-704) and I2C pins
  `gpp4-2/3` (JDT:708-720).
* The vendor binds it with `compatible = "abov,a96t3x6"` (ABOV:2786-2788) and
  reads `ABOV_VENDORID 0x12` / `ABOV_MODEL_NO 0x14` / `ABOV_FW_VER 0x01`
  (ABOV:60,65,68). The driver does **not** compare those to a constant — it
  prints them — so there is **no expected id value in the vendor tree**.
* Config: `CONFIG_TOUCHKEY_GRIP=y` and `CONFIG_KEYBOARD_ABOV_TOUCH_3X6=y`
  (CFG:1605,1634). The separate `drivers/sensorhub/stm_v2/a96t3x6.c`
  ("A96T3X6 chip as grip sensor") is **not** built
  (`# CONFIG_SENSORS_A96T3X6 is not set`, CFG:4397) and is not the driver that
  runs here.

### Hall / cover flip

* `flip_cover` (JDT:843-858) declares two GPIO hall inputs: `hall` on `gpa0-3`
  (JDT:848-852) and `certify_hall` on `gpa0-4` (JDT:853-857). Both are in
  pin-controller 0 (ALIVE) at `0x11CB0000` (DT:601); `gpa0`'s bank offset is
  `0x040` with `DAT` at `+0x04` (PIN:1709, bank_type_7 CON/PUD/DAT at
  `+0x00/+0x08/+0x04`, PIN:49-52), i.e. `DAT` at `0x11CB0044`. No register or
  GPIO write is needed to read the level.

## What iOS needs

iOS does not have per-vendor sensor drivers; the whole stack is built on
**`IOHIDEventService`** objects that publish **`IOHIDEvent`s** into
`IOHIDEventSystem`, which `CoreMotion`, `SpringBoard`, `BacklightServices` and
`libsystem_pthread`'s HID clients consume. The right shape:

* Motion (accel/gyro/mag): an `IOHIDEventService` subclass that timestamps and
  dispatches `kIOHIDEventTypeAccelerometer` / `…Gyro` / `…MagneticField`
  events on the generic-desktop HID page. `CoreMotion`'s `CMDeviceMotion` /
  `CMMotionManager` read these; there is no per-chip Apple driver. On Apple
  silicon the same events come out of `AppleSPU`/`AppleSPUHIDDriver`, which is
  why a custom service is the shortest path here.
* Ambient light / proximity: an `IOHIDEventService` publishing the ALS and
  proximity usages; `BacklightServices`/`AppleBacklight` (and the call UI) drive
  the screen and blanking from them.
* Hall / cover flip: a vendor-defined `IOHIDEvent` (or an HID switch usage) that
  `SpringBoard` turns into the cover-open/closed state.
* The port already ships a minimal `IOHIDEventService` and queue
  (`xnu/patches/0058-sweet-hid-event-services.patch`, with `sweet_hid_key` and
  `sweet_hid_touch` helpers). New sensor services should hang off that, not a
  new framework.

Consequence for the hub sensors: because they are **not** AP-visible I2C parts,
the natural iOS mapping is *not* "I2C HID device per chip". Either (a) port the
hub protocol (SPI + the `ssp.h` message framing) as one `IOHIDEventService`
that multiplexes all six sensors, or (b) — the expensive path — leave the hub
idle and talk to the individual chips, which the hub owns and the AP cannot
reach. (a) is the only viable one.

## implemented / stubbed / unknown

**implemented (patch `0143`, opt-in `a8sensors=1`, read-only)**

* Grip: identify the Abov A96T3X6 on I2C5 `0x13880000` slave `0x20` by reading
  `VENDORID 0x12` / `MODEL_NO 0x14` / `FW_VER 0x01` through the same s3c2440
  byte protocol the touch probe uses. Reports the bytes, or "no ACK".
* Hall: read the `gpa0` DAT register once and print the `gpa0-3` / `gpa0-4`
  levels. No interrupt, no input device, no write.

**stubbed (printed but not exercisable from the AP without more work)**

* The sensor hub and its six sensors: the probe prints one line per hub sensor
  with `bus=ssp-spi4@0x13980000` and `part=<LSM6DSL|AK09918C|TMD3725|LPS22H>`
  and part-id `hub`, because reaching the chip needs the exynos SPI master plus
  the SSP framing. It does **not** clock the hub.

**unknown / needs hardware or more source reading**

* Whether `S-Boot` leaves SPI4, the hub reset (`gpg3-5`) and the hub I2C/pin mux
  set up, and whether the hub is already running when XNU starts.
* The exact response of the A96T3X6 to a read of `0x12`/`0x14` on this unit
  (the vendor driver encodes no expected value).
* Whether the grip I2C pins (`gpp4-2/3`) are muxed for I2C at handoff; the probe
  deliberately does not touch the pin controller (read-only rule), so a NACK is
  a valid, safe outcome that answers this.
* The hub MCU WHOAMI (`0x55`) on this unit — the command is known from the
  vendor driver, but confirming it needs an SPI transaction we do not do yet.

## Ladder with kill criteria

Each rung is a separate, independently falsifiable step. A rung is reached only
with a screenshot / serial log from the phone.

1. **Probe compiles and is inert by default.** Kill: a default boot
   (`a8sensors` unset) logs anything new, or panic. Pass = build green, no new
   line without the arg.
2. **Grip identify.** Run `a8sensors=1`; expect one `A8SENSORS: grip …` line
   with real `vendor`/`model` bytes. Kill: a bus timeout/hang, or the line
   never appears. A printed "no ACK" is a *finding*, not a failure (the pin mux
   question above).
3. **Hall read.** Expect the `hall`/`certify_hall` levels to track a magnet
   over the cover sensor. Kill: levels frozen regardless of the magnet.
4. **Hub identity.** Send the SSP `WHOAMI 0x0F` over SPI4 and read back `0x55`.
   This needs the exynos SPI master (clock + pinctrl), so it is a real patch of
   its own; kill if SPI4 is unclocked at handoff and enabling it stalls the bus.
5. **One hub sensor stream.** Request one sensor (e.g. accelerometer) and
   publish `IOHIDEvent`s. Kill: no data, or the hub never ACKs the enable.
6. **iOS consumption.** `CMDeviceMotion` / the cover state / the ALS actually
   react. Kill: events reach `IOHIDEventSystem` but no client sees them.

The first four are in scope for this track; 5–6 are the follow-on hub port.

## Verification

`0143` was generated as a `git diff` in a private copy of
`/tmp/opencode/xnu-xnu-8796.141.3` with `0001`–`0139` applied and committed
(`base`). It adds one new file (`bsd/dev/a5xelte_sensors.c`) and edits
`bsd/conf/files` + `iokit/bsddev/IOKitBSDInit.cpp` exactly the way `0138` does.
`git apply --check` against a fresh `base` and a full CI build are recorded in
the handoff; compile errors, if any, are settled by the first CI run.

On merge, `0142` (the flashlight probe) had already taken the two wiring slots
this patch anchors on, so its three wiring hunks were re-anchored to the
post-`0142` files without touching the new-file hunk:

* `bsd/conf/files`: trailing context is now `wifi` → `flashlight` → `pmic`
  (the probe is still inserted between `scsc` and `wifi`, matching the call
  order below).
* `iokit/bsddev/IOKitBSDInit.cpp` extern block: trailing context is now
  `wifi` → `flashlight` → `#if defined(JACKPOTLTE)`.
* `iokit/bsddev/IOKitBSDInit.cpp` call block: anchored on the SCSC block
  (`a5xelte_scsc_probe()`) instead of the WiFi block, so the call lands between
  `a5xelte_scsc_probe()` and `a5xelte_wifi_probe()` and leaves the flashlight
  block intact.

Verified with `patch -p1 --dry-run` and `git apply --check` against a tree
reconstructed to the exact post-`0142` state: all hunks apply with zero fuzz.

## Not verified

* Nothing has been flashed or booted. No device claim is made.
* The probe has not been compiled locally (XNU needs macOS tooling).
* The vendor values above are quoted from the donor tree; they have not been
  checked against this unit's bootloader-supplied device tree.
