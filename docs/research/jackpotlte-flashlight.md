# jackpotlte flashlight / torch LED — controller, registers, and port status

**Scope:** only the camera flash / torch LED of the Galaxy A8 2018
(`jackpotlte`, SM-A530F, Exynos 7885). The camera *sensors* (FIMC-IS, the
S5K2P6/3P8SP/SR846 modules) are a separate track and are not analysed here.

**This file describes a prerequisite, not a working torch.** The board has a
real LED controller and Samsung's Linux driver for it is fully readable, so the
*hardware path* is understood. But **iOS does not expose the LED directly**: a
torch is requested through `AVCaptureDevice` (AVFoundation) and reaches the
hardware inside the camera stack. This tree has no camera stack, so on the A8 as
it stands today there is nothing that can drive this controller end to end.
Everything below is host-side research from Samsung's own sources; **nothing has
been run on the phone** and the probe that ships with this track never drives
the line.

Sources are quoted from the read-only vendor tree
`/var/tmp/opencode-keep/Eureka-Kernel/` ("Release R24U sources", Samsung 4.4.302
for Exynos 7885), with `file:line` citations there. Repo paths are `xnu/patches/…`.

## 1. Answer to "which controller drives the flash?"

| fact | value | source |
| --- | --- | --- |
| Controller | **Kinetic KTD2692** flash/torch LED driver, `compatible = "ktd2692"` | `arch/arm64/boot/dts/exynos/dtb/exynos7885-jackpotlte_common.dtsi:1537-1538` |
| Board's declared flash product | `FLASH_KTD2692` (`product_name = <5>`) under the FIMC-IS `vender` node | `…jackpotlte_common.dtsi:1465-1469` |
| FIMC-IS flash module binding | `compatible = "samsung,sensor-flash-ktd2692"`, `id = <0>` (rear sensor) | `…jackpotlte_common.dtsi:1531-1535` |
| Control interface | **single GPIO, not I2C**: `flash_control = <&gpg1 2 0x1>` (gpg1 pin 2) | `…jackpotlte_common.dtsi:1539` |
| Bus | none. There is **no I2C address** and **no ID/status register**; the part is write-only over its S-Wire pin. | driver below; no i2c/flash node in the DT |
| Is there an I2C flash (ktd*/max77*/rt*)? | **No.** The only I2C PMIC-adjacent part is the S2MU004 charger (`s2mu004@3D`, `0x3D` on `i2c@13840000`), which has no flash/LED child. | `…jackpotlte_common.dtsi:1100-1110` |

The GPIO flag `0x1` is `GPIO_ACTIVE_LOW`
(`include/dt-bindings/gpio/gpio.h:13-14`). Samsung's driver drives the pin with
raw `__gpio_set_value()` and never applies that flag
(`drivers/leds/leds-ktd2692.c:33-40`), so the flag is informational; the S-Wire
line is idle **high** (the driver leaves it high at the end of every frame,
`leds-ktd2692.c:167-168`).

## 2. Where the pin lives (SoC side)

| fact | value | source |
| --- | --- | --- |
| Pin controller | `pinctrl_3: pinctrl@139B0000` ("TOP"), `reg = <0x0 0x139B0000 0x1000>` | `arch/arm64/boot/dts/exynos/dtb/exynos7885.dtsi:631-635` |
| gpg1 bank offset in TOP | `0x140` (`exynos7885_pin_banks3`, pin-controller 3) | `drivers/pinctrl/samsung/pinctrl-exynos.c:1742`; instance at `:1771-1778` |
| Bank register layout | `bank_type_6`: CON `+0x00`, DAT `+0x04`, PUD `+0x08`, DRV `+0x0c` | `pinctrl-exynos.c:72-75` |
| **So** GPG1CON / GPG1DAT / GPG1PUD | `0x139B0140` / `0x139B0144` / `0x139B0148` | derived from the two rows above |
| Pin-function encodings | `fimc_is_flash_host` = output (`pin-function = <1>`), `fimc_is_flash_is` = input with pull-down (`<0>`, `pin-pud = <1>`) | `…jackpotlte_common.dtsi:681-692` |
| DT pinctrl states of the LED node | `"default"` = `is`, `"host"` = host, `"is"` = is | `…jackpotlte_common.dtsi:1548-1551` |

The same TOP window is already mapped read-only by the panel port
(`xnu/patches/0133-jackpotlte-panel-dsim-bringup.patch`, `A5P_TOP_BASE`), so
reading it is known not to fault.

## 3. The controller's "register map" (S-Wire protocol)

The KTD2692 has no memory-mapped or I2C registers. Samsung's driver bit-bangs
`flash_control` (gpg1-2) with microsecond timing to send **8-bit frames**. Each
frame is *register select* (top 3 bits, fixed addresses) OR'ed with a 5-bit data
field, sent MSB first.

Frame format (`drivers/leds/leds-ktd2692.c:112-171`, addresses and timings in
`include/linux/leds-ktd2692.h`):

| element | value | source |
| --- | --- | --- |
| Data start | high for `T_SOD` = **200 µs** | `leds-ktd2692.c:127-129`; `leds-ktd2692.h` `T_SOD` |
| bit `1` | low `T_L_HB` **5 µs**, high `T_H_HB` **80 µs** | `leds-ktd2692.c:77-98` |
| bit `0` | low `T_L_LB` **80 µs**, high `T_H_LB` **5 µs** | `leds-ktd2692.c:42-63` |
| Data end | low `T_EOD_L` **25 µs**, then high `T_EOD_H` **400 µs** | `leds-ktd2692.c:163-168` |
| Frame width | 8 bits, MSB first | `leds-ktd2692.c:131-161` |

Register-select addresses (top 3 bits; `leds-ktd2692.h` `KTD2692_ADDR_*`):

| name | frame address | meaning |
| --- | --- | --- |
| `LVP_SETTING` | `0x00` | low-voltage protection threshold |
| `FLASH_TIMEOUT_SETTING` | `0x20` | hardware flash timeout |
| `MIN_CURRENT_SETTING` | `0x40` | minimum current (timer mode) |
| `MOVIE_CURRENT_SETTING` | `0x60` | **torch / movie current** |
| `FLASH_CURRENT_SETTING` | `0x80` | **strobe current** |
| `MOVIE_FLASHMODE_CONTROL` | `0xA0` | **mode: off / torch / flash** |

Mode word written = `mode_status | 0xA0` (`leds-ktd2692.c:295/322/352`), so the
three frames on the wire are:

| mode | frame | source |
| --- | --- | --- |
| off (disable) | `0xA0` | `leds-ktd2692.c:293-296` |
| torch / movie | `0xA1` (`KTD2692_ENABLE_MOVIE_MODE` = 1) | `leds-ktd2692.c:346-353` |
| flash / strobe | `0xA2` (`KTD2692_ENABLE_FLASH_MODE` = 2) | `leds-ktd2692.c:316-323` |

**Torch and strobe are the same controller, different mode words**, with
different current registers — this is the "continuous vs single-shot" split the
brief asked about. The vendor's own mode enum is
`DISABLES_MOVIE_FLASH_MODE = 0`, `ENABLE_MOVIE_MODE = 1`, `ENABLE_FLASH_MODE = 2`
(`leds-ktd2692.h`, `enum ktd2692_mode_control_t`).

## 4. Enable / brightness / mode path, and the values from the DT

| fact | value | source |
| --- | --- | --- |
| `max_current` (IMax) | **1500 mA** in the common node; **1360 mA** in the `eur_open_06` board DTS | `…jackpotlte_common.dtsi:1540`; `exynos7885-jackpotlte_eur_open_06.dts:147` |
| `flash_current` | 1200 mA | `…jackpotlte_common.dtsi:1541` |
| `movie_current` | 175 mA | `…jackpotlte_common.dtsi:1542` |
| `torch_current` | 75 mA | `…jackpotlte_common.dtsi:1544` |
| torch level table | `torch_table = <1 2 2 3 3 4 4 4 5 5>`, enabled | `…jackpotlte_common.dtsi:1545-1546` |
| current encode macro | `KTD2692_MOVIE_CURRENT(mA,max)`, `KTD2692_FLASH_CURRENT(mA,max)` (5-bit) | `include/linux/leds-ktd2692.h` |

With the common node's `max_current = 1500`, the macros give
`torch_current_value = 0x01` and `movie_current_value = 0x05`; the on-the-wire
torch-on sequence (state 3, `leds-ktd2692.c:330-359`) is therefore:

1. select pinctrl state `"host"` → gpg1-2 becomes an output;
2. write `MOVIE_CURRENT` frame `0x60 | 0x01`;
3. write mode frame `0xA1` (torch on).

Torch off (state 1, `leds-ktd2692.c:285-304`) writes mode `0xA0` and then drives
the line low. Strobe (state 2, `:306-329`) writes `LVP`, then
`FLASH_CURRENT` (`0x80 | encoded`), then mode `0xA2`.

On Android this is reached from the camera side: the FIMC-IS flash sub-device
maps `CAM2_FLASH_MODE_OFF/SINGLE/TORCH` to `ktd2692_led_mode_ctrl(1/2/3)`
(`drivers/media/platform/exynos/fimc-is2/sensor/module_framework/flash/fimc-is-flash-ktd2692.c:62-108`;
enum in `fimc-is2/include/fimc-is-metadata.h:315-322`). The sysfs nodes
`rear_flash` / `rear_torch_flash` do the same from `camera_class`
(`leds-ktd2692.c:389-392, 497-509`).

## 5. What iOS needs

iOS never hands an app the LED. The public, documented path is:

* `AVCaptureDevice` (AVFoundation) — `hasTorch` / `torchAvailable` /
  `torchMode` / `setTorchModeOn(level:)`, guarded by
  `lockForConfiguration()`. Apple requires camera permission for it even with no
  capture. The Control Center flashlight uses the same camera path, not a
  separate LED service.
  ([Apple: Capture device lighting](https://developer.apple.com/documentation/avfoundation/capture-device-lighting),
  [setTorchModeOn(level:)](https://developer.apple.com/documentation/avfoundation/avcapturedevice/settorchmodeon(level:)))

So on the kernel side the torch has to be exposed **by the camera driver**. On
this port there is no camera stack at all: `docs/upstream-README.md:41` lists
camera among the unsupported subsystems ("camera … daemons start and fail"), and
the tree has no FIMC-IS or Apple camera kext. Reaching `AVCaptureDevice`'s torch
therefore needs the camera bring-up track first, and the LED work is a leaf of
that, not a standalone feature.

**Implemented / stubbed / unknown on the iOS side**

* Implemented in this tree: **nothing** for iOS torch. The only code here is the
  read-only probe (section 6).
* Stubbed: nothing yet — there is no `torchd`-style service, no `AppleCamIn`
  binding, no user client. (Whether iOS 16 has a dedicated torch daemon at all
  is **unknown** to this author; the documented API is `AVCaptureDevice`, which
  is a camera-stack request. Do not assume a `torchd` exists.)
* Unknown: which iOS 16 kernel class would own the LED on this board, and
  whether it can be fed without the full camera pipeline — needs the camera
  track's findings.

## 6. The probe that ships with this track (read-only)

`xnu/patches/0142-jackpotlte-flashlight-probe.patch` adds
`bsd/dev/a5xelte_flashlight.c`, wired through `bsd/conf/files` and
`iokit/bsddev/IOKitBSDInit.cpp` like `0138`.

**Implemented:** with `a8torch=1` only, it maps the TOP pin-controller window
(read-only), reads `GPG1CON` / `GPG1DAT` / `GPG1PUD`, extracts the gpg1-2
function field and DAT bit, and prints one line. This is the only software-readable
state of the controller, because the KTD2692 itself has no readable register.

**Not implemented, deliberately:** any write to gpg1-2, any S-Wire frame, any
enable of the LED. The whole enable path is kept as a comment block in the file,
not as executed code.

**Default-inert:** with no boot argument the probe returns immediately — it does
not map the window and prints nothing, so a default boot is byte-for-byte the
same behaviour as before.

## 7. Ladder and kill criteria

Each rung is a separate patch/experiment, opt-in, and gated on `a8torch=1` plus
(where it writes) a second explicit argument. A rung is only "reached" with a
serial log, screen output, or a scope capture.

* **L0 — read-only line state (this patch).** Boot with `a8torch=1`; expect
  `A5XELTE FLASH: KTD2692 s-wire on gpg1-2 … con=… dat=… pud=… func=…`.
  *Kill:* if the TOP window read faults or the map fails, the block is not
  accessible on this board — stop and re-derive the address before anything else.
* **L1 — confirm the boot state.** Compare L0's `con`/`func`/`pud` against the
  DT's `"is"` (input, pull-down) and `"host"` (output) states to see what S-Boot
  left the pin as. *Kill:* if `func` shows a non-GPIO alternate function owned by
  another block, the GPIO write path is blocked.
* **L2 — send only the OFF frame (`0xA0`), still no light.** Implement the
  S-Wire encoder and send exactly the disable frame. This frame cannot light the
  LED by construction. Verify the timing on a scope/logic analyser against the
  four constants in section 3. *Kill:* no scope ⇒ do not go past L2 (the vendor
  timing is µs-sensitive and cannot be trusted blind).
* **L3 — torch on at the lowest level, with a hard auto-off.** Requires the
  second opt-in arg and a watchdog that restores `0xA0`. *Kill:* if this is done
  outside the camera stack it produces a light that iOS will not control, and it
  exercises current with no camera rail context — do not treat it as a torch.
* **L4 — real iOS torch.** Only after the camera track exists: bind the LED to
  whatever camera driver exposes `AVCaptureDevice` torch. This is the actual
  deliverable that makes "flashlight" true, and it is owned by the camera track.

## 8. Unknowns (do not guess)

| item | why unknown | where it would come from |
| --- | --- | --- |
| State of gpg1-2 at handoff (S-Boot / DECON left it input or output?) | not in any source | the L0 probe on the phone |
| KTD2692 power-on default mode | needs the datasheet / a scope | L0 + L2 |
| Whether the KTD2692 is populated on this exact SM-A530F unit | vendor DT says yes, unit untested | L2 (OFF frame + observe) |
| Physical polarity vs the DT active-low flag | vendor drives raw levels, flag arguably ignored | scope at L2 |
| Which iOS 16 class owns the torch | no camera stack in this tree | camera track |

## 9. Verification (host only) / not verified

* Host: `0142` (was `0144` on branch `jackpotlte-flashlight`) was generated against an `xnu-8796.141.3` tree with `0001`-`0139`
  applied, and applies with `git apply --check` cleanly (three files:
  `bsd/conf/files`, `bsd/dev/a5xelte_flashlight.c`,
  `iokit/bsddev/IOKitBSDInit.cpp`).
* Not verified: no boot, no device run, no CI result recorded in this file. The
  probe has never been executed on the phone, and nothing here proves the
  controller is present or works. That is what L0 is for.
