# Read-only Mali-G71 (Bifrost gen1, v6) probe design — Exynos 7885 / jackpotlte

Date: 2026-11-08. Status: research design only. Read-only everywhere: no
writes to PMU, CMU, PLL, GPU, MMU or interrupt registers; CPU-side reads of
always-on blocks first, GPU-block reads only after the power-domain gate says
G3D is already up. Aligns with M0 in `docs/jackpotlte-mali.md` §6 and the
draft `docs/drafts/jackpotlte-mali/0137-jackpotlte-mali-probe.patch.draft`.
Answers "what state does S-Boot leave `mali@0x11500000` in?"

### Verifier's note (2026-11-08)

This one checked out unusually well. Re-fetched and confirmed line-for-line:

- `panfrost_gpu.c`: quirk test at `:161`, `GPU_MODEL(g71, 0x6000, GPU_REV_EXT(g71, 0, 0, 1, _05dev0))` at `:222-223`, and the feature reads at `:262-271` — all as cited.
- `panfrost_regs.h`: `GPU_ID 0x00`, `GPU_INT_STAT 0x2c`, `GPU_FAULT_STATUS 0x3C`, `GPU_MMU_FEATURES 0x014`, `GPU_THREAD_* 0xA0-0xAC`. Matches.
- Samsung `cal-if/exynos7885/pmucal_local_exynos7885.h:274-275`: `g3d_on[]` really is write `0x11C80000+0x40A0 = 0xF` then wait `0x40A4 == 0xF`.
- `cal-if/exynos7885/cmucal-sfr.c`: `PLL_CON0_PLL_G3D` at `0x0120` (`:712`), `_ENABLE` bit 31 and `_STABLE` bit 29 (`:3014-3015`), and the G3D CMU PCLK gate at `0x2004` (`:717`). Matches.

Only caveat: `0x11c840a0`/`0x11c840a4` are PMU-relative offsets (`PMU_BASE + 0x40A0`), so the patch must add the base rather than absolute-address them.

## Summary

- S-Boot boots Android first; we must not assume G3D is powered, clocked or
  reset-released. The only safe first observation is the always-on
  `pd_g3d` status register (`0x11c840a4`, mask `0xF`) and the CMU_G3D PLL
  word, neither of which requires the G3D block itself to be alive.
- `0x11c840a4 == 0xF` means the Exynos `pd_g3d` domain is fully in the
  powered/reset-released state that Samsung's `g3d_on[]` sequence waits for
  (pmucal `pmucal_local_exynos7885.h:274-275`). Anything else means "off or
  partially clocked" and the probe stops without touching `0x11500000`,
  because an MMIO read from an unpowered Exynos block can stall the bus /
  raise SError.
- The GPU MMIO window (`0x11500000`, size `0x5000`, `arm,mali`,
  `exynos7885-mali_common.dtsi:15-17`) is the Bifrost `panfrost` register
  block. If and only if the domain reports up *and* the PLL reports enabled
  and locked, a short read sequence (`GPU_ID` first, then feature/present/
  ready/interrupt/fault registers) proves addressability and identifies the
  part.
- Expected identity for this SoC: `GPU_ID & 0xFFFF0000 == 0x60000000`
  (Mali-G71, `panfrost_model_eq(pfdev, 0x6000)` in `panfrost_gpu.c:161,336`),
  with the vendor model revision row `GPU_REV_EXT(g71, 0, 0, 1, _05dev0)`,
  i.e. `GPU_ID == 0x60000001` for the matching r0p1. This is the exact test
  upstream uses to select the G71 quirk (`COHERENCY_ACE` → force
  `ACE_LITE | ACE` on the job manager, `panfrost_gpu.c:159-163`).
- Coherency, tiler features, present/ready bitmasks, thread counts and MMU
  feature widths are a mix of *read from the GPU* (panfrost reads
  `GPU_COHERENCY_FEATURES` at `panfrost_gpu.c:271` when `HW_FEATURE_COHERENCY_REG`,
  `GPU_TILER_FEATURES` at `:262`, `GPU_MMU_FEATURES` at `:265`,
  `GPU_THREAD_*` at `:266-269`; present/ready bitmaps at `:301-312`) and
  *hardcoded per model* (Mesa `pan_model.c:76`: only `MODEL_ANISO(NONE)` and
  `MODEL_TB_SIZES(4096, 4096)` for G71 — no core counts, no thread counts, no
  rates, no quirks beyond anisotropy). `pan_model.h:105-117` says G71 is
  Bifrost arch 6 (`PAN_ARCH_MAJOR(0x6000…)==6`).
- Exynos 7885 has **no System MMU for the GPU** (no `samsung,sysmmu` node in the
  DT, `exynos7885.dtsi`); the `GPU_MMU_FEATURES`/AS registers describe Mali's
  own bundled MMU, and the probe must not assume an IOMMU.
- Partition/safety: the read-only PMU/CMU registers are safe unconditionally
  (always-on block); the GPU block is only safe *after* `status == 0xF` *and*
  a stable PLL. When in doubt the probe returns without a single GPU read.

## Safe sequence (no writes)

1. Map (do not access contents) `PMU` `0x11c80000`/`0x20`, `CMU_G3D`
   `0x11400000`/`0x8000`. These blocks are always-on.
2. Read `PMU + 0x40A0` (control) and `PMU + 0x40A4` (status). Mask `0xF`.
   Log comparison; never write either.
3. Read `CMU + 0x0120` (PLL_G3D CON0): extract enable bit 31 and stable bit 29
   (cmucal `PLL_CON0_PLL_G3D_ENABLE` / `_STABLE`; `cmucal-sfr.c:712`,
   `pmucal_local_exynos7885.h:288-290`).
4. Read `CMU + 0x1800` (bus divider) and `CMU + 0x2004` (G3D_CMU PCLK gate)
   as context only.
5. **Gate.** If `status != 0xF` **or** enable/stable are not both 1, print the
   finding and return. Do **not** map or read `0x11500000`.
6. If gated in, map `GPU` `0x11500000`/`0x5000` and read `GPU_ID` (offset
   `0x00`) **first**. Then, in the order upstream `panfrost_gpu_init` uses, read
   (a) `GPU_L2_FEATURES`..`GPU_THREAD_*`/`GPU_THREAD_TLS_ALLOC`, (b) present
   bitmaps, (c) ready bitmaps, (d) `GPU_INT_*`, `GPU_FAULT_STATUS`, `GPU_STATUS`.
   No soft reset, no `GPU_PWR_KEY` unlock, no interrupt masking or unmasking, no
   writes at all. Upstream does power-on *before* these reads
   (`panfrost_gpu_init`: soft reset at `:524`, then `panfrost_gpu_init_features`
   at `:528`, then `panfrost_gpu_power_on` at `:551`; clocks/regulators/PM
   domains/optional reset deassert all happen earlier in
   `panfrost_device_init:276-314`).

## Probe table

Columns: **precondition** `C` = safe cold (always-on block), `P` = only after
`pd_g3d status == 0xF` and stable PLL, `P+` = after a readable `GPU_ID`.

| register (absolute) | offset in block | precondition | plausible value | conclusion |
|---|---|---|---|---|
| PD_G3D control (PMU `0x11c80000`) | `+0x40A0` (`0x11c840a0`) | C | `0x0` when off; `0xF` when on | write-target of `g3d_on[]` shown read-back to prove prior state; never written by probe |
| PD_G3D status (PMU `0x11c80000`) | `+0x40A4` (`0x11c840a4`) | C | `0x0`=domain dropped; `0xF`=domain live; other = partially clocked/gated | **gate for every later row** |
| PLL_G3D CON0 (CMU `0x11400000`) | `+0x0120` (`0x11400120`) | C | bit31=enable, bit29=lock/stable; low bits div S/M/P | tells us whether the G3D PLL is providing the core clock; fail-closed if either bit is 0 |
| CLK_G3D bus divider | `+0x1800` (`0x11401800`) | C | raw divider (`cmucal-sfr.c:711`) | context only |
| G3D CMU PCLK gate | `+0x2004` (`0x11402004`) | C | raw gate word (`cmucal-sfr.c:717`) | context only; no single golden value documented |
| core clock mux `/`div (separate `CORE` CMU block) | `0x12000000+{0x101c,0x1824,0x2024}` | C | raw (`cmucal-sfr.c:245-248`) | context; not mapped by the minimal probe |
| `GPU_ID` | `0x11500000+0x00` | P | `0x60000001` (r0p1) for G71; any `0x6000xxxx` counts | proves the block is powered, clocked, reset-released and readable; resets/errs if mismatched; note panfrost treats `0x6956` as an alias for `0x0600`, not relevant here (`panfrost_gpu.c:318-324`) |
| `GPU_L2_FEATURES` | `+0x004` | P+ | nonzero, SoC-specific | L2/cache width reporting |
| `GPU_CORE_FEATURES` | `+0x008` | P+ | nonzero | shader-core option bits |
| `GPU_TILER_FEATURES` | `+0x00C` | P+ | nonzero raw word | panfrost reads it raw (`panfrost_gpu.c:262`); Mesa has **no** matching register-derived field (tilebuffer sizes for G71 are hardcoded `4096/4096`, `pan_model.c:76`) |
| `GPU_MEM_FEATURES` | `+0x010` | P+ | nonzero | memory-system widths |
| `GPU_MMU_FEATURES` | `+0x014` | P+ | nonzero; `VA_BITS = x & 0xFF`, `PA_BITS = (x>>8)&0xFF` (`panfrost_regs.h:19-20`) | tells width of Mali's own MMU; no SoC SMMU is involved |
| `GPU_AS_PRESENT` | `+0x018` | P+ | expected non-zero slot mask | address-space slots available |
| `GPU_JS_PRESENT` | `+0x01C` | P+ | expected non-zero slot mask | job slots (M2 will need one) |
| `GPU_INT_RAWSTAT` | `+0x020` | P+ | `0` typical if quiet; reset/power bits documented | pending raw interrupts (read-only; not cleared) |
| `GPU_INT_STAT` (masked) | `+0x02C` | P+ | `0` typical | `0x1` fault, `0x7` multi-fault, `0x100` reset completed, `0x200` power changed etc (`panfrost_regs.h:24-34`) |
| `GPU_STATUS` | `+0x034` | P+ | bit2 = perf-counter active | only meaningful field for probe |
| `GPU_FAULT_STATUS` | `+0x03C` | P+ | `0` expected | nonzero = a prior fault word is pending |
| `GPU_COHERENCY_FEATURES` | `+0x300` | P+ | G71 is ACE case: `BIT(COHERENCY_ACE)=0x2` (`panfrost_gpu.c:159-163`, `regs.h:11-109`) or `0`/ACE_LITE if the coherency reg is absent | selects the upstream G71 quirk |
| `GPU_THREAD_MAX_THREADS` | `+0x0A0` | P+ | nonzero (threads per core) | thread model detail |
| `GPU_THREAD_MAX_WORKGROUP_SIZE` | `+0x0A4` | P+ | nonzero | thread model detail |
| `GPU_THREAD_MAX_BARRIER_SIZE` | `+0x0A8` | P+ | nonzero | thread model detail |
| `GPU_THREAD_FEATURES` | `+0x0AC` | P+ | device-specific | read by panfrost, not decoded by name |
| `GPU_THREAD_TLS_ALLOC` | `+0x310` | P+ | nonzero | required TLS slots |
| `GPU_SHADER_PRESENT_*` | `+0x100`/`+0x104` | P+ | bitmask, typically nonzero | number of shader cores from hardware, not from Mesa |
| `GPU_TILER_PRESENT_*` | `+0x110`/`+0x114` | P+ | bitmask, typically nonzero | tiler core count from hardware |
| `GPU_L2_PRESENT_*` | `+0x120`/`+0x124` | P+ | bitmask, typically nonzero | L2/cache groups from hardware |
| `SHADER_READY_*` / `TILER_READY_*` / `L2_READY_*` | `+0x140`..`+0x164` | P+ | ready bitmask, may be `0` if G3D is powered but not internally powered | shows whether `panfrost_gpu_power_on`-style sequence (L2/shader/tiler power gates) has already happened; panfrost owns the write side (`regs.h:118-125`) |
| `GPU_STACK_PRESENT_*` (optional) | `+0xE00`/`+0xE04` | P+ | bitmask | core stack present |
| `STACK_READY_*` (optional) | `+0xE10`/`+0xE14` | P+ | bitmask | stack readiness |
| `GPU_AFBC_FEATURES` | AFBC offset `0x04C` | P+ | raw | AFBC support only |

`GPU_READY`: **no such register exists in upstream `panfrost_regs.h`**. Readiness is
not a single bit there; it is the collection of `_READY` bitmasks above plus
`GPU_ID` responding. If a "GPU_READY" name appears in a vendor tree, treat it
as Samsung-specific and name it explicitly before using it.

## Hazard notes

- **Off-domain access.** `pd_g3d` status `!= 0xF` with the G3D clock closed can
  leave the GPU SFR/QCH disabled; the bus transaction may not complete. Vendor
  keep-alive pattern: the QCH entries in `g3d_on[]` are `PMUCAL_SAVE_RESTORE`
  guarded on the four status bits (`pmucal_local_exynos7885.h:292-298`), and
  `g3d_off[]` tears the whole CMU path down and then writes control `0x0`,
  waits status `0x0` (`:310-320`). Hence equality to `0xF`, not merely nonzero.
- **Clock without domain.** The PLL word is always readable (CMU block lives in
  the always-on clock domain), but a powered domain with the PLL disabled still
  cannot answer GPU reads; fail closed on enable or stable bits.
- **No software timeout on MMIO.** `readl` cannot be cancelled. The design
  therefore gates instead of attempting and recovering. A safe implementation
  would also run the probe off the default boot path (boot arg) so a failed
  assumption cannot panic an unattended boot.
- **Reads are not free of side effects at the CPU level:** repeated probes
  should not write anything; `GPU_ID` etc are marked `(RO)` in
  `panfrost_regs.h`. Do not touch `GPU_PWR_KEY` (`0x50`, write-only unlock
  `0x2968A819`), `GPU_CMD` (`0x30`, soft/hard reset), `GPU_INT_MASK` (`0x28`),
  or any `*_PWRON/PWROFF` register. Reading `GPU_INT_RAWSTAT/INT_STAT` is
  acceptable; clearing with `GPU_INT_CLEAR` is **not** part of a read-only
  probe.
- **Upstream ordering for writing stages (reference only).** When we later need
  M1, follow `panfrost_device_init:276-314`: PM domains first, optional reset
  deassert (`reset_control_deassert`, `:24-27`), then clocks (`clk_init
  :286-288`), devfreq, optional regulators, `devm_platform_ioremap_resource`
  (`:306`), then `panfrost_gpu_init` (`:518-564`: soft reset, feature read,
  DMA mask, IRQ request, `panfrost_gpu_power_on`). This is the reason those
  reads are physically legal in upstream: all power/clock setup precedes them.
  Our read-only probe is deliberately a subset and must not implement that
  sequence on its own.
- **kernel abort reporting.** `panfrost_gpu_soft_reset` writes `GPU_CMD` and
  waits for `GPU_IRQ_RESET_COMPLETED`; that is a write and is out of scope
  here. Any "reset completed" bit already set in `GPU_INT_RAWSTAT` at probe time
  is evidence S-Boot or Android did such work, not evidence we may do it.

## What this probe cannot tell us

- Whether the **power rail** (`vdd_g3d` = S2MPU08 BUCK5) is actually energised:
  it is ACPM-only and not in the always-on PMU block; the status bit only says
  the local power gate is on. Polling `0x11c840a4` cannot distinguish "rail up"
  from "rail held up by bootloader leakage".
- Whether `cmu_g3d` root clocks are truly reaching the block beyond the PLL
  enable/stable bits; per-gate QCH entries are validated by the same status
  bits, but bus-side failure modes are not covered.
- GPU **rendering, job submission, MMU translation** or **coherency at runtime**:
  the probe reads feature/id registers only; address-space contents, page tables,
  and the G71 `ACE → ACE_LITE|ACE` JM forced coherency remain unverified.
- Whether `RESET` is released conceptually: a readable `GPU_ID` implies the
  block is responding, but it is not proof that every internal sub-block is out
  of reset or that a soft reset is unnecessary before first use.
- **Hierarchical tiling** enablement (`MODEL_QUIRKS(.no_hierarchical_tiling)` is
  per-model, and the G71 row in `pan_model.c:76` does not set it) and precise
  tilebuffer behaviour; tiler and shader *present* counts tell core count only.
- **Performance**, thermal or DVFS behaviour, and ASPM/governor state.
- Any post-probe change: this is a one-shot observation. It cannot say whether
  the domain stays up for the duration of a long run.

## Source map

- Upstream Linux: `drivers/gpu/drm/panfrost/panfrost_regs.h` (offsets,
  `GPU_*`, COHERENCY bits), `drivers/gpu/drm/panfrost/panfrost_gpu.c`
  (`panfrost_gpu_init`, `panfrost_gpu_init_features`, `panfrost_gpu_power_on`,
  `panfrost_gpu_soft_reset`, `gpu_models[]` G71 entry), `panfrost_device.c`
  (PM domains → reset → clk → regulators → ioremap → GPU).
- Upstream Mesa (funnymdzz mirror used for this fetch):
  `src/panfrost/model/pan_model.c:76` G71 row; `src/panfrost/model/pan_model.h:105-117`
  `pan_arch` (G71 → 6); Mesa docs/rst support notes.
- Samsung: `Eureka-Kernel` `master` `arch/arm64/boot/dts/exynos/dtb/exynos7885-mali_common.dtsi`
  (`mali@0x11500000`, `pd_g3d`, `gpu_pmu_status_reg_offset`, live mask `0xF`);
  `drivers/soc/samsung/cal-if/exynos7885/pmucal_local_exynos7885.h:273-320`;
  `drivers/soc/samsung/cal-if/exynos7885/cmucal-sfr.c:711-717` (`G3D_CMU` block
  and `PLL_G3D`).
- Local: `docs/jackpotlte-mali.md` §3-§6, `docs/drafts/jackpotlte-mali/0137…`,
  `docs/research/panfrost-v6-g71.md`.