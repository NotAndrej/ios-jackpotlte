# Mainline Linux support for Samsung Exynos 7885 (`jackpotlte`)

Research note: independent upstream (non-vendor) hardware facts. Written 2026-11-08.
Read-only research; no device, git, CI, or repository changes were made. This
file is the deliverable of the mainline research task.

### Verifier's note (2026-11-08)

Re-fetched from `torvalds/linux` `master` and checked:

- Confirmed: `arch/arm64/boot/dts/exynos/exynos7885.dtsi` (487 lines) and
  `exynos7885-jackpotlte.dts` (114 lines) both exist; the board DTS's only memory
  child is `memory@80000000` with three banks.
- Confirmed: all 8 CPUs use `enable-method = "psci"` and the node is
  `compatible = "arm,psci"; method = "smc"; cpu_on = <0xc4000003>`.
- Confirmed: `exynos850.dtsi:318` is `gpu@11500000` with
  `compatible = "samsung,exynos850-mali", "arm,mali-bifrost"`, `reg = <0x11500000 0x5000>`,
  IRQs 125/126/127 and a single `core` clock — a useful precedent, but note our
  vendor INTIDs are 190/191/192, so it is a *shape* match only.
- Confirmed: `arm,mali-bifrost.yaml` lists exactly one Samsung compatible,
  `samsung,exynos850-mali`. There is no 7885 GPU binding.
- **Correction on the RAM point.** The "Samsung's claim is 1 GiB" framing is wrong:
  Samsung's own DT does **not** state a high-bank size at all —
  `exynos7885.dts:126-129` gives `memory@880000000` with size `0x0`, i.e. a
  bootloader-patched placeholder. So neither 1 GiB (`board.h`) nor 2 GiB (mainline)
  is documented in static vendor DT; mainline's `0x80000000` is itself an assertion
  that needs a bootloader memory-map read before we widen `JT_XNU_RAM_END`. The
  advice "verify S-Boot before expanding" stands, but it is not a vendor-vs-mainline
  disagreement — the vendor simply says nothing.

## Source basis

This note cites only the upstream Linux tree (`torvalds/linux`, GitHub mirror of
`git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git`) and kernel commit
history. It deliberately did **not** use our repo docs, `shim/jackpotlte/board.h`,
or Samsung's 4.4 vendor tree as authority for the main table; where those are
mentioned below it is labelled as vendor/local, not mainline. Current upstream
tip used for path/line citations: `master` (tags include `v7.3`/`v7.4-rc1`).
Lore/kernel.org pages were not directly fetchable during this research (Anubis
anti-bot wall), so commit history and `MAINTAINERS` were used instead; the
commit author/maintainer attribution should be re-checked against
`lore.kernel.org` when an unblocked client is available.

Supporting independent cross-check: authenticated GitHub Code Search over
`torvalds/linux` for `exynos7885`. It returns exactly the files listed below
(no Exynos 7885 references in DTS other than `exynos7885*`, no `mali`/GPU
compatible, no USB/thermal/cpufreq nodes):

- `arch/arm64/boot/dts/exynos/exynos7885{,-pinctrl}.dtsi`
- `arch/arm64/boot/dts/exynos/exynos7885-jackpotlte.dts`
- `drivers/clk/samsung/clk-exynos7885.c`
- `drivers/pinctrl/samsung/pinctrl-exynos-arm64.c`
- `drivers/soc/samsung/exynos-chipid.c`
- `Documentation/devicetree/bindings/*` (clock, chipid, PMU, pinctrl wakeup, I2C, UART)

and no `samsung,exynos7885-mali`, no `exynos7885-decon`/DSI, no 7885 USB or
thermal bindings.

## Summary

- Exynos 7885 **SoC enablement exists in mainline**, led by Dávid Virág and
  merged by Krzysztof Kozlowski. First SoC DTS/clock driver entered mainline in
  `v5.18` (commit `06874015327b`, 2022-02-21).
- The **only upstream-enabled board is `jackpotlte`** (Galaxy A8 2018); DTS
  enables memory, gpio-keys, internal eMMC, oscillator and `serial_2` console.
- Mainline gives a usable SoC skeleton: 6 x A53 + 2 x A73, GIC-400, arch timer,
  PSCI, four CMUs, four pinctrl instances, chipid, system PMU/reset, 3 UARTs,
  8 I2C controllers, one eMMC controller.
- Mainline does **not** enable: USB, SDIO, UFS, display/DECON/DSI/panel,
  GPU/Mali (no DT node), PMIC, thermal, cpufreq, cpuidle, generic power
  domains, ramoops. Those are Sony/Samsung vendor-tree territory.
- Mainline clock and PMU compatibility for this SoC are deliberately based on
  the Exynos7/850 fallbacks, not on Exynos 7885-specific reset/power-up code.
- There is no dedicated `MAINTAINERS` entry named "Exynos7885"; it falls under
  the generic `ARM/SAMSUNG S3C, S5P AND EXYNOS` entry maintained by Krzysztof
  Kozlowski (and Peter Griffin), with mailing lists `linux-samsung-soc@vger.kernel.org`
  and tree `git://git.kernel.org/.../krzk/linux.git`.

## Feature table (upstream, mainline)

| Feature | Upstream? | Citation (file:line or commit/URL) | Kernel version / notes |
|---|---:|---|---|
| SoC compatible `samsung,exynos7885` | Yes | `arch/arm64/boot/dts/exynos/exynos7885.dtsi:13`, board `.../exynos7885-jackpotlte.dts:16` | v5.18 (`06874015327b`, 2022-02-21) |
| Board DTS `samsung,jackpotlte` | Yes | `arch/arm64/boot/dts/exynos/exynos7885-jackpotlte.dts:15-16` | v5.18 |
| CPU: 6xA53 MPIDR `0x100`-`0x103`, `0x200`-`0x201`; 2xA73 `0x0`-`0x1` | Yes | `exynos7885.dtsi:92,95,99,106,113,120,123,127,134,141` | Not usable as vendor cpuidle recipe |
| `enable-method = "psci"` for every CPU | Yes | `exynos7885.dtsi:96,103,110,117,124,131,138,145` | Mainline relies on PSCI, not Exynos CPU power-up registers |
| PMU system controller @ `0x11c80000` | Yes (fallback) | `exynos7885.dtsi:298-301`; binding `Documentation/.../soc/samsung/exynos-pmu.yaml:37,45` | `compatible = "samsung,exynos7885-pmu", "samsung,exynos7-pmu", "syscon"` |
| `syscon-reboot` (offset `0x0400`) | Yes | `arch/arm/boot/dts/samsung/exynos-syscon-restart.dtsi` (included at `exynos7885.dtsi:487`); `offset = 0x0400` (fragment lines 14-19, `offset` line 17) | Matches `board.h` `JT_PMU_SWRESET` |
| `syscon-poweroff` (offset `0x330c`) | Yes | `exynos-syscon-restart.dtsi:7-12` (`offset = <0x330c>`, line 10) | New mainline-only hardware fact for board bring-up |
| GIC-400 @ `0x12301000` | Yes | `exynos7885.dtsi:186-195` (reg lines 191-194) | Four reg banks: dist/CPU/virt/VCPU |
| Arch timer (PPI 13/14/11/10) | Yes | `exynos7885.dtsi:157-164` | No `clock-frequency` property; hyp timer not wired |
| Fixed `oscclk` | Yes (freq in board) | SoC `exynos7885.dtsi:167-171`; board `exynos7885-jackpotlte.dts:85-86` `clock-frequency = <26000000>` | 26 MHz confirmed upstream |
| Clock controllers (CMU top/core/peri/fsys) | Yes | `exynos7885.dtsi:198-294`; binding `.../clock/samsung,exynos7885-clock.yaml`; driver `drivers/clk/samsung/clk-exynos7885.c` | Maintainers named in binding (Virág, Choi, Kozlowski, Nawrocki, Figa) |
| G3D clock gating (CMU core) | Partial | `exynos7885.dtsi:225-238` (`CLK_DOUT_CORE_G3D`); `clk-exynos7885.c:33,50,173,206,264,306` | Clock **exists upstream**; no GPU node consumes it |
| Pin control, 4 instances | Yes | `exynos7885.dtsi:268-294`; `drivers/pinctrl/samsung/pinctrl-exynos-arm64.c:769-847`; match `pinctrl-samsung.c:1499-1500` | `samsung,exynos7885-pinctrl` |
| `gpa1` bank offset | Yes | `pinctrl-exynos-arm64.c:774` (`EXYNOS850_PIN_BANK_EINTW(8, 0x060, "gpa1", ...)`), binding `samsung,pinctrl-wakeup-interrupt.yaml` | Confirms `0x060` |
| Wakeup EINT | Yes | `exynos7885.dtsi:270-277` (`samsung,exynos7885-wakeup-eint`, `samsung,exynos7-wakeup-eint`) |  |
| ChipID / ASV ID | Yes (fallback) | `exynos7885.dtsi:180-183`; `drivers/soc/samsung/exynos-chipid.c:68` (`"EXYNOS7885", 0xE7885000`); `.../chipid.c:246-250` (`samsung,exynos850-chipid`); binding `.../samsung,exynos-chipid.yaml:29` | DB row: 7885 -> `0xE7885000` |
| GPIO buttons (mainline board DTS) | Yes | `exynos7885-jackpotlte.dts:38-62` (gpio-keys) plus pinmux `...:89-110` (gpa1 5/6/7) | Mainline board support includes buttons |
| Internal eMMC (`mmc_0`) | Yes | `exynos7885.dtsi:304-316`; board enable `exynos7885-jackpotlte.dts:64-82` | Commit `ced37411d7f5` (2022-06-01), v6.0 |
| SDIO controller | No DT node | Clock present: `exynos7885.dtsi:257-264` (`CLK_DOUT_FSYS_MMC_SDIO`) only | No controller binding in DTS |
| UFS | No | Absent from all `exynos7885*` DTS/drivers per code search | Vendor-todo |
| USB | No DT node | Clock present: `exynos7885.dtsi:259,265` (`CLK_DOUT_FSYS_USB30DRD`) only | No USB/PHY DT node upstream |
| UART (x3) | Yes | `exynos7885.dtsi:319-358` (`samsung,exynos7885-uart`, fallback `samsung,exynos5433-uart`); board `.../exynos7885-jackpotlte.dts:112-113` (`serial_2` okay) | Clock fix commit `f84d83d81655` (2022-05-26), v5.19 |
| I2C (x8) | Yes | `exynos7885.dtsi:364+` (`samsung,exynos7885-i2c`, fallback `samsung,s3c2440-i2c`) | Binding `samsung,s3c2410-i2c.yaml` lists it |
| Display/DECON/DSI/panel | No | No compatible or node in any `exynos7885*` DTS; code search has no `exynos7885-decon`/`dsi` | Vendor-only (SAM-K values in our `board.h`) |
| GPU/Mali-G71 DT node | No | No `gpu@`/`mali@`/`panfrost` in `exynos7885.dtsi` or board DTS; code search: no `samsung,exynos7885-mali` (vendor repos only) | Universal vendor DTS uses `mali@0x11500000`; mainline path/binding is `samsung,exynos850-mali` |
| arm,mali-bifrost binding | Generic only | `Documentation/devicetree/bindings/gpu/arm,mali-bifrost.yaml:37` allows `samsung,exynos850-mali`, not `samsung,exynos7885-mali` | Mainline would require a new Exynos 7885 compatible |
| PMIC (`s2mps*`/`s2mpu*`) | No | No node in `exynos7885*` DTS; generic `mfd/regulator` drivers elsewhere not wired for this SoC | Vendor-only |
| Thermal | No | Absent from DTS and code search for 7885 | Vendor-only |
| cpufreq (OPP/clocks) | No | `exynos7885.dtsi` has CMUs but no `operating-points-v2`, no CPU clocks in CPU nodes | Generic cpufreq may bind CPU nodes but no DT operating points upstream |
| cpuidle | No | `exynos7885.dtsi` has no idle states or Exynos PMU CPU power registers in DT | See `board.h` vendor note |
| Generic power domains (PD) | No | No `power-domains` nodes in `exynos7885.dtsi` (contrast is same absence in `exynos850.dtsi`) | No mainline genpd wiring for this SoC |
| Ramoops/pstore | No | Absent from DTS | Samsung Exynos Snapshot vendor feature |

## Per-topic notes with citations

### 1. SoC core
- SoC DT begins with `compatible = "samsung,exynos7885"` at
  `arch/arm64/boot/dts/exynos/exynos7885.dtsi:13`.
- Chip ID reg: `chipid@10000000` at `.../exynos7885.dtsi:180-183`,
  reg `<0x10000000 0x24>`, compatible specific + `samsung,exynos850-chipid`.
- GIC-400 reg banks exactly as in `shim/jackpotlte/board.h` (`JT_GICD_BASE` etc.):
  `.../exynos7885.dtsi:191-194` → `0x12301000`, `0x12302000`,
  `0x12304000`, `0x12306000`.
- PMU system controller at `.../exynos7885.dtsi:299-301`:
  reg `<0x11c80000 0x10000>`, compat fallback to `samsung,exynos7-pmu`.
- Reboot/poweroff are supplied by the included common `syscon-restart` DT
  fragment (`.../exynos7885.dtsi:487`), giving `SWRESET` offset `0x0400` and
  `PS_HOLD_CONTROL` offset `0x330c`.

### 2. Clock subsystem
- Binding: `Documentation/devicetree/bindings/clock/samsung,exynos7885-clock.yaml`
  (maintainers Dávid Virág, Chanwoo Choi, Krzysztof Kozlowski, Sylwester
  Nawrocki, Tomasz Figa).
- Driver: `drivers/clk/samsung/clk-exynos7885.c` (847 lines), built by
  `drivers/clk/samsung/Makefile:26` under `CONFIG_EXYNOS_ARM64_COMMON_CLK`.
- SoC DTS instantiates four CMUs at `.../exynos7885.dtsi:198-294`:
  `cmu_peri@10010000`, `cmu_core@12000000`, `cmu_top@12060000`,
  `cmu_fsys@13400000`, each `#clock-cells = <1>`.
- `dout_core_g3d` is present in CMU top/core, but no DT consumer exists
  (`.../exynos7885.dtsi:233`; `clk-exynos7885.c:33,50,173,206,264,306`).
- The oscillator frequency is deliberately left unset in the SoC DTS
  (`oscclk: osc-clock`, no frequency) and set per board at
  `.../exynos7885-jackpotlte.dts:85-86` to `26000000`.

### 3. Pin control
- Pinctrl driver table match: `drivers/pinctrl/samsung/pinctrl-samsung.c:1499-1500`
  (`"samsung,exynos7885-pinctrl"` → `exynos7885_of_data`).
- Per-instance bank tables:
  `drivers/pinctrl/samsung/pinctrl-exynos-arm64.c:769-847`.
- Pin mux bank `gpa1` starts at `0x060` in instance 0 (ALIVE):
  `.../pinctrl-exynos-arm64.c:774`
  (`EXYNOS850_PIN_BANK_EINTW(8, 0x060, "gpa1", ...)`), matching
  `shim/jackpotlte/board.h` `JT_PINCTRL_GPA1 = 0x060`.
- Board DTS enables `key_volup`/`key_voldown`/`key_power` on `gpa1` 5/6/7:
  `.../exynos7885-jackpotlte.dts:38-62,89-110`.

### 4. PMU, reset, power management
- Mainline only binds this SoC's PMU to the generic Exynos 7 driver path:
  DTS compat specific + fallback (`.../exynos7885.dtsi:299-300`), and
  `drivers/soc/samsung/exynos-pmu.c` has a table entry for
  `samsung,exynos7-pmu` only (generic path).
- Binding lists a specific 7885 entry:
  `Documentation/devicetree/bindings/soc/samsung/exynos-pmu.yaml:37`
  (`samsung,exynos7885-pmu`) with fallback line 45 (`samsung,exynos7-pmu`).
- Mainline does **not** publish Exynos 7885 CPU power-up register layouts,
  secondary-core sequencing, or cpuidle states. Values in our `board.h`
  `JT_PMU_CPU_*` block (PMU offset `0x2000`, step `0x80`, core power `0xF`,
  Aff1-based numbering) are vendor-tree (SAM-K) claims and must be validated by
  phone readback if cited in ARM code.
- No power domains/genpd are present in mainline 7885 DTS (and the same is
  true for the sibling Exynos 850 DTS checked).

### 5. ChipID and ASV
- DTS: `chipid@10000000`, compatible specific + `samsung,exynos850-chipid`
  at `.../exynos7885.dtsi:180-183`.
- Driver ID table: `drivers/soc/samsung/exynos-chipid.c:68`:
  `{ "EXYNOS7885", 0xE7885000 }`.
- Driver match table: `.../exynos-chipid.c:246-250`
  (`samsung,exynos850-chipid` → `exynos850_chipid_drv_data`).
- Binding: `Documentation/devicetree/bindings/hwinfo/samsung,exynos-chipid.yaml:29`
  (`samsung,exynos7885-chipid` → fallback `samsung,exynos850-chipid`),
  and line 13 calls the binding maintainable by Krzysztof Kozlowski.

### 6. Storage
- eMMC is the only storage controller upstream:
  `mmc_0: mmc@13500000` at `.../exynos7885.dtsi:304-316`,
  compatible `samsung,exynos7885-dw-mshc-smu` / `samsung,exynos7-dw-mshc-smu`,
  clocks `CLK_GOUT_MMC_EMBD_ACLK` and `CLK_GOUT_MMC_EMBD_SDCLKIN`.
- Board enables eMMC with HS400 tuning and 8-bit bus at
  `.../exynos7885-jackpotlte.dts:64-82`.
- Commit history: initial SoC support `06874015327b` (2022-02-21); eMMC added
  by David Virag in `ced37411d7f5` (2022-06-01); UART clocks corrected in
  `f84d83d81655` (2022-05-26); several hard-coded blocks gained specific
  compatibles in `0ffc692ad836` (2023-11-08); style/coding changes in
  `873fa6bd9a4f`/`bf1aee1ad1c0` (2026-08-01). Merge tag
  `samsung-dt64-5.20` (`813b08089075`, 2022-07-01) carried the early work.
- No UFS, no SDIO node, no watchdog in mainline DTS (SDIO is represented only
  by `CLK_DOUT_FSYS_MMC_SDIO` in the SoC clock tree, lines 257-264).
- `CMDQ` window / SMU registers quoted in `shim/jackpotlte/board.h`
  (`JT_EMMC_CMDQ = 0x13508000`) are not present in the mainline DTS node.

### 7. Serial, I2C, board bring-up
- 3 UARTs at `0x13800000`, `0x13810000`, `0x13820000`
  (`.../exynos7885.dtsi:319-358`); board enables `serial_2` only and sets
  `stdout-path = &serial_2` (`.../exynos7885-jackpotlte.dts:28,112-113`),
  so mainline supports early console on this phone.
- 8 I2C controllers at `0x13830000` onward and `0x11cd0000`
  (e.g. `.../exynos7885.dtsi:364+`), with compat `samsung,exynos7885-i2c`.
- Board mainline DTS also provides `gpio-keys` and memory banks; no PMIC,
  no panel, no USB, no secondary storage.

### 8. GPU / Mali-G71
- Mainline has generic DRM Panfrost support for ARM Mali Bifrost, and its
  device-tree binding (`.../gpu/arm,mali-bifrost.yaml`) allows compatible
  `samsung,exynos850-mali`, `arm,mali-bifrost`, but **not**
  `samsung,exynos7885-mali` (binding line 37 and enum through line ~30).
- There is **no `gpu@11500000` / `mali@11500000` node in the mainline 7885 DTS**,
  and independent code search for `exynos7885-mali` finds only third-party vendor
  trees (e.g. `topser9/kernel_samsung_universal7885`, Eureka/Sliva trees), not
  `torvalds/linux`.
- For comparison only, mainline does include a Mali node for the sibling SoC:
  `arch/arm64/boot/dts/exynos/exynos850.dtsi:318-328`:
  `gpu: gpu@11500000`, compatible `samsung,exynos850-mali`, `arm,mali-bifrost`,
  reg `0x11500000 0x5000`, interrupts job/mmu/gpu
  (`GIC_SPI 126/127/125`, lines 322-324), clocks `cmu_g3d` core clock,
  status disabled. **Exynos 7885 has no equivalent upstream.**
- Mainline panfrost itself (`drivers/gpu/drm/panfrost/`) supports G71 by
  GPU ID at probe time (see sibling `docs/research/panfrost-v6-g71.md`), but
  without DTS/transformation this does not enable the phone. Vendor tree uses
  `compatible = "arm,mali"` (not `arm,mali-bifrost`) per that note.
- Therefore, for an Exynos 7885 Panfrost binding you would need a new compatible
  (likely `samsung,exynos7885-mali`, `"arm,mali-bifrost"`) and a DT node. The
  exact mainline memory/interrupt/clock/regulator/power-domain facts for the 7885
  GPU are **not** available upstream; only `mali@0x11500000`, INTIDs 190/191/192,
  `pd_g3d`, CMU_G3D, etc. are vendor/Darwin-side values in `docs/jackpotlte-mali.md`.

### 9. Display / DECON
- Mainline has no Exynos 7885 DPU/DECON/DSI nodes or compatibles. Search finds
  no `exynos7885-decon`, no `exynos7885-dsi`. Thus display trigger registers
  (`DECON @ 0x14860000`, `TRIG @ +0x70`, value `0x1281`) and panel facts in
  `board.h` are not independently supported by mainline.

### 10. USB
- SoC DTS has no USB or PHY node. The only upstream reference is a clock:
  `CLK_DOUT_FSYS_USB30DRD` at `.../exynos7885.dtsi:259,265`. Any USB bring-up
  requires vendor/phone-derived data.

### 11. Thermal / cpufreq / cpuidle / CPU power
- Mainline DTS has no thermal zones for 7885, no CPU `operating-points-v2`,
  no cpuidle states, and no generic power domains.
- Mainline CPU nodes carry only PSCI (section 1 above); secondary-core gating
  remains firmware/vendor responsibility.

### 12. Maintainers and mailing-list discussion
- Generic MAINTAINERS entry "ARM/SAMSUNG S3C, S5P AND EXYNOS ARM
  ARCHITECTURES" (`MAINTAINERS:3629-3666`): maintainer Krzysztof Kozlowski
  `<krzk@kernel.org>`, co-maintainer Peter Griffin, reviewer Alim Akhtar;
  lists `linux-samsung-soc@vger.kernel.org`; tree
  `git://git.kernel.org/pub/scm/linux/kernel/git/krzk/linux.git`; includes
  `arch/arm64/boot/dts/exynos/` and `drivers/soc/samsung/`.
- Clock binding lists additional component maintainers: Dávid Virág,
  Chanwoo Choi, Krzysztof Kozlowski, Sylwester Nawrocki, Tomasz Figa.
- Upstream authorship: initial SoC support and eMMC support by Dávid Virág
  (`virag.david003@gmail.com`), merged/committed by Krzysztof Kozlowski
  (`krzk@kernel.org`, Linaro at the time). Commit IDs are the best public
  evidence available here; direct `lore.kernel.org` archives could not be
  fetched through the Anubis filter during this session.

## Kernel-version timeline

| Event | Commit | Date | Mainline release evidence |
|---|---|---|---|
| Initial Exynos7885 SoC + clock support | `06874015327b` | 2022-02-21 | Ancestor of `v5.18` (appears in `v5.18-rc1`) |
| UART clocks corrected | `f84d83d81655` | 2022-05-26 | Ancestor of `v5.19` |
| Internal eMMC (`jackpotlte`) | `ced37411d7f5` | 2022-06-01 | Ancestor of `v6.0` |
| `samsung-dt64-5.20` merge | `813b08089075` | 2022-07-01 | Merge into arm/dt |
| SoC DTS path/file moves | `724ba6751532` | 2023-05-04 | ARM DTS vendor subdir migration |
| Specific compatibles added to several blocks | `0ffc692ad836` | 2023-11-08 | Present by `v6.8` (v6.7 path diverged) |
| Coding-style style pass | `873fa6bd9a4f` / `bf1aee1ad1c0` | 2026-08-01 | Current master |

## What we can cite from mainline in `shim/jackpotlte/board.h`
*(only facts where mainline agrees; keep vendor-only values explicitly marked vendor)*

| `board.h` symbol | Mainline value + citation | Assessment |
|---|---|---|
| `JT_LOW_RAM_BASE` / `JT_LOW_RAM_END` | `0x80000000` + `0x3da00000` → `0xbda00000`; `.../exynos7885-jackpotlte.dts:31-32` | Match |
| `JT_XNU_RAM_BASE` | `0x880000000` (`0x8` high cell + `0x80000000` low cell); `.../exynos7885-jackpotlte.dts:33` | Match for base |
| `JT_XNU_RAM_END` currently `0x8c0000000` | Mainline size is `0x80000000` (2 GiB, third bank reg size), giving `0x900000000`; `.../exynos7885-jackpotlte.dts:33` | **Mismatch in size**: current `board.h` assumes 1 GiB (Samsung's claim). Safe window, but mainline grants 2 GiB. Verify S-Boot memory map before expanding. |
| `JT_BOOT_MPIDR` | `cpu@100` MPIDR `0x100`; `.../exynos7885.dtsi:92,95` | Match |
| `JT_GICD_BASE` ... `JT_GICV_BASE` | `0x12301000`, `0x12302000`, `0x12304000`, `0x12306000`; `.../exynos7885.dtsi:186,191-194` | Match |
| `JT_PMU_BASE` | `0x11c80000`, reg size `0x10000`; `.../exynos7885.dtsi:298-301` | Match |
| `JT_PMU_SWRESET` | `0x0400`; `arm/samsung/exynos-syscon-restart.dtsi` offset (included at `.../exynos7885.dtsi:487`) | Match |
| `JT_PMU_CPU_CFG` / `STATUS` / `STEP` / `PWR` / Aff1 numbering | **Not** in mainline; system PMU compat falls back to `samsung,exynos7-pmu` | Vendor-only (SAM-K / patch 0135); validate before relying on it |
| `JT_TIMEBASE` / timer INTIDs | Mainline DTS arch timer lists PPIs 13, 14, 11, 10 at `.../exynos7885.dtsi:160-163`; PPI 14 → INTID 30 and PPI 11 → INTID 27, exactly the non-secure/virtual INTIDs defined in `board.h` | Match for those two INTIDs. Mainline has no timer clock-frequency; board DTS sets `oscclk` 26 MHz at `.../exynos7885-jackpotlte.dts:85-86` |
| `JT_EMMC_BASE` | `0x13500000`; `.../exynos7885.dtsi:304-316` | Match |
| `JT_EMMC_CMDQ` | Not in mainline node | Vendor/DMA discovery needed |
| `JT_PINCTRL_ALIVE` | `0x11cb0000`; `.../exynos7885.dtsi:268-270` | Match |
| `JT_PINCTRL_GPA1` / `CON`/`DAT`/`PUD` | `0x060` bank in pinctrl instance 0; `drivers/pinctrl/samsung/pinctrl-exynos-arm64.c:774` | Match |
| `JT_FB_*`, `JT_DECON_*`, panel `s6e3fa7`, 1080x2220 | **Not** in mainline; no DECON/DSI node or compatible | Vendor-only (SAM-K) |
| `JT_RAMOOPS_*` | **Not** in mainline; ESS layout is Samsung driver-specific | Vendor/ROM-specific; mainline not applicable |

Additional mainline facts worth carrying into code or docs:
- `SWRESET = 0x0400` and poweroff `PS_HOLD_CONTROL = 0x330c` are in the mainline
  common syscon fragment (included at
  `arch/arm64/boot/dts/exynos/exynos7885.dtsi:487`).
- Mainline board supports the volume/power buttons on `gpa1` 5/6/7; use it as
  the independent pin-name source instead of vendor bank tables.
- `oscclk` is fixed in the SoC DT; `26 MHz` is set in the board DTS
  (`.../exynos7885-jackpotlte.dts:85-86`), not in the chip timer node.

## Caution when using mainline as evidence
- Mainline uses fallback compatibles (`exynos850-chipid`, `exynos7-pmu`,
  `exynos7885-pinctrl` has its own data) and inherits Exynos 7 behavior for
  PMU/reboot. A phone-specific register claim that differs from mainline (for
  example CPU power-up offsets) should be verified separately; mainline does
  not override it for Exynos 7885.
- Mainline's RAM high bank is 2 GiB; our `board.h` conservatively uses 1 GiB
  for the XNU window. Do not "correct" this in source without checking S-Boot's
  actual memory map.
- Mainline has no 7885 GPU node; any citation of `mali@0x11500000`,
  INTIDs 190-192, `pd_g3d`, CMU/PLL G3D, clock gates, or coherency quirk is
  vendor/HakiOS-side, not independent mainline evidence.
- All counts above are from `torvalds/linux` and are by definition public,
  non-confidential hardware facts.
