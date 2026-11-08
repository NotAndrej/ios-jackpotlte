# Panfrost vs Mali-G71 (Bifrost gen1, v6): upstream reality vs the HakiOS v7-only fork

Date: 2026-11-08. Author: research subagent. Status: research note (read-only,
no code changes, no commit).

## Summary

- Upstream Mesa **does carry Bifrost v6**: `src/panfrost/model/pan_model.c` lists
  `BIFROST_MODEL(PAN_PROD_ID(6, 0, 0), "G71", …)`, and `src/panfrost/model/pan_model.h`
  gives `pan_arch(0x6000) == 6`. v6 is therefore not a missing backend; it is a build/dispatch gate.
- Upstream Linux DRM **also supports Mali-G71** (`GPU_MODEL(g71, 0x6000)` in
  `drivers/gpu/drm/panfrost/panfrost_gpu.c`), but **only behind a coherency quirk**
  (G71 reports `COHERENCY_ACE`; driver forces ACE_LITE+ACE). Exynos 7885 uses a
  non-upstream `RedwoodMali` XNU driver, so this must be reproduced there.
- HakiOS's Darwin port compiled **Bifrost v7 only** (`G52`). Its `meson.build` Darwin branch
  and `pan_screen.c` dispatch are the two gates; v6 source is excluded from the Darwin
  build even though the symbols exist.
- There is **no evidence** of anyone running panfrost v6 on Darwin/macOS or iOS. HakiOS
  ran G52 (v7) only; no private or published report exercises G71/v6 through the
  `RedwoodMali`/`redwood_drm.c` transport.

### Verifier's note (2026-11-08, added after the report was checked)

- **Confirmed verbatim** in `torvalds/linux` `drivers/gpu/drm/panfrost/panfrost_gpu.c`:
  the `GPU_MODEL(g71, 0x6000, …)` entry and the G71 quirk that forces
  `ACE_LITE|ACE` when the part reports `BIT(COHERENCY_ACE)` (§3.3).
- **Confirmed** in Samsung's vendor DT (`exynos7885-mali_common.dtsi:14`): the node is
  `compatible = "arm,mali"`, so it does not match upstream's `arm,mali-bifrost` (§3.1).
- **Could not fetch first-party Mesa source.** `gitlab.freedesktop.org` and
  `git.kernel.org` are behind an Anubis bot wall and every GitHub mirror path tried
  returned 404. The v6 claims (`pan_model.c` G71 entry, `pan_arch(0x6000)==6`, the
  `0x688` identity constant, the `ctx->arch == 7` scheduler split) are **corroborated
  second-hand** — Mesa's own docs table (`G71, G72 | Bifrost (v6) | ES 3.1 | GL 3.1`)
  and a mirror quoting `PAN_PROD_ID(6, 0, 0)` — not read in the original file today.
  Treat exact expressions/line numbers as unverified until someone fetches the source.
- **Correction — invented quote (fixed inline in §5):** this note originally had HakiOS
  "stating" that the v6 path "has never driven a Mali v6 through the Darwin IOKit DRM
  shim". Both HakiOS documents (`hakios-mt6768/docs/metal-x6826b-plan.md`,
  `hakios-xnu-kernel/mtk/docs/metal.md`) contain **no occurrence** of "v6", "gen1",
  "IOKit DRM shim" or "arm,mali-bifrost". They simply never discuss v6. Absence of
  discussion is not a statement by HakiOS.
- **Broken/unverifiable file citations:** `tools/mesa/panfrost-darwin.patch` and
  `tools/mk-mesa.sh` **do not exist in this repository** (`find . -name '*panfrost*'`
  returns only this note). Every `:187/:197/:199/:222` line number in §4 traces back to
  our own `docs/jackpotlte-mali.md`, which itself cites them. That is self-reference, not
  independent confirmation — the patch was never inspected here.

## 1. Upstream Mesa Bifrost v6 support and the version gate

### 1.1 Architecture version test

`src/panfrost/model/pan_model.h` (funnymdzz/mesa mirror of upstream, current main):

```c
static inline unsigned
pan_arch(uint64_t gpu_id)
{
   switch (MIDGARD_PROD_ID(gpu_id)) {
   case 0x600: case 0x620: case 0x720: return 4;
   case 0x750: case 0x820: case 0x830: case 0x860: case 0x880: return 5;
   default: return PAN_ARCH_MAJOR(gpu_id);   /* Bifrost/Valhall: gpu_id >> 28 */
   }
}
```

For a Mali-G71 reporting GPU ID `0x6000…`, `pan_arch` returns `6`, so it is accepted into
the Bifrost (`PAN_ARCH >= 6`) compilation paths.

### 1.2 Where the gate lives

| File | Role | v6 condition |
|---|---|---|
| `src/panfrost/model/pan_model.h` | Version decoder (`pan_arch`) | returns `6` for G71 |
| `src/panfrost/model/pan_model.c` | `pan_model_list[]`, `pan_get_model()`, `pan_get_fallback_model()` | `BIFROST_MODEL(PAN_PROD_ID(6,0,0), "G71", …)`; fallback model for `case 6/7` if a specific model is missing |
| `src/gallium/drivers/panfrost/pan_screen.c` (Gallium) | Screen creation | Upstream works from the model/`pan_arch`; HakiOS fork returns NULL unless `dev->arch == 7` (`panfrost-darwin.patch:222`) |
| `src/panfrost/lib/pan_shader.c` | Compiler selection | `#if PAN_ARCH >= 6` → `bifrost_compile`; else Midgard |
| `src/panfrost/compiler/bifrost/bifrost_compile.c` | Backend IR | `b->shader->arch == 6 ? 0x688 : 0` identity, plus `ctx->arch == 7` scheduler differences (see §2) |
| `src/panfrost/meson.build` / `src/gallium/drivers/panfrost/meson.build` | Per-arch libraries | Built once per value in `pan_architectures`/`panfrost_versions`; if `6` is omitted, no `panfrost (v6)` library exists |
| `src/panfrost/vulkan/meson.build` / PanVK | JM vs CSF submission | `jm_archs = [6, 7, 9]`, `csf_archs = [10, 12, 13, 14]` (Noysz/panvk-g99-jm reconciliation) |

### 1.3 Supported? Caveats in the docs

Mesa's `docs/drivers/panfrost.rst` is inconsistent across revisions: older text lists
"Other Midgard and Bifrost chips (e.g. G71) are not yet supported," while newer tables
list `G71, G72 | Bifrost (v6)`. In practice the model entry and the v6 compiler path
exist and are used on Linux (G71 on Exynos 8895 / HiSilicon Kirin 960), but panfrost does
not claim OpenGL ES conformance on G71. Treat v6 as "buildable and runs, but non-conformant"
on a v6 part — not "unsupported code."

## 2. Bifrost backend differences (v6 gen1 vs v7 gen2)

Most of the backend is shared (`PAN_ARCH <= 8` = "Bifrost"). The v6-vs-v7 splits that actually
exist in the source are narrow and explicit:

1. **Payload identity word.** `src/panfrost/compiler/bifrost/bifrost_compile.c`:
   ```c
   unsigned identity = (b->shader->arch == 6) ? 0x688 : 0;
   ```
   (HakiOS pin cited `bifrost_compile.c:1140`; the code now sits near line 580 after refactor.)
   This is a hardware ABI difference in the shader binary header, not a perf hint.

2. **Scheduler / threads.** In `bifrost_compile.c`:
   ```c
   bool full_threads = (ctx->arch == 7 && ctx->info.work_reg_count <= 32);
   .preloads = ctx->arch == 7 ? bi_count_preload_cost(ctx) : 0,
   ```
   On v6 the compiler never assumes the 32-reg "full threads" scheduling mode and never
   emits preloads, even when the register count would allow it on v7.

3. **Varying / attribute format encoding.** In `src/panfrost/vulkan/panvk_vX_shader.c`
   (Vulkan path; OpenGL backend uses the same encodings where relevant):
   ```c
   #if PAN_ARCH <= 6
      return (MALI_R16F << 12) | panfrost_get_default_swizzle(1);  // PNTC/PSIZ
   #else
      return (MALI_R16F << 12) | MALI_RGB_COMPONENT_ORDER_R000;
   #endif
   #if PAN_ARCH <= 6
      return (MALI_SNAP_4 << 12) | panfrost_get_default_swizzle(4);  // POS
   #else
      return (MALI_SNAP_4 << 12) | MALI_RGB_COMPONENT_ORDER_RGBA;
   #endif
   ```
   v6 uses the older swizzle-based encoding for barycentric/position varyings; v7+ uses the
   newer component-order constants.

4. **Varying attribute lowering on v6 was already present in HakiOS.** The reference's
   `panfrost-redwood-perf.patch` only adds a `return true;`/`} ` at `bifrost_compile.c:5851`,
   which stock 25.0.7 already has; our own `docs/jackpotlte-mali.md` records this as
   a rebase artifact, not a v6 fix. `bi_should_idvs` is already true for vertex stages on v6.

5. **Perf knobs are arch-independent NIR passes.** `widen16`, `pushubo`, `scratchregs` are
   documented (`jackpotlte-mali.md §4`) as the only reason the perf patch is applied; they are
   not gated on `PAN_ARCH`. They must still be re-checked on G71 because tilebuffer/core
   counts differ.

6. **Tiling / hierarchical tiling.** `MODEL_QUIRKS(.no_hierarchical_tiling = true)` is set per
   model, not per arch. The G71 model entry does not carry this quirk in the current
   `pan_model.c`, but earlier Exynos 8895 reports and Chromebook ports have differed; verify
   against the real `tiler_features` on phone (M0 probe).

7. **Memory access modes / coherency.** This is principally a **kernel/driver** item (§3.1),
   not a compiler item. Mesa/panfrost relies on the backend to publish the correct tiler/fragment
   job descriptors; v6 and v7 use the same JM descriptor set on the driver side.

## 3. Upstream `panfrost` kernel driver (in-tree DRM) per version

### 3.1 Compatible strings / binding

`drivers/gpu/drm/panfrost/panfrost_drv.c` `of_match_table`:

- `arm,mali-bifrost` — the generic entry for all Bifrost (covers G71 `0x6000` and G72);
- `arm,mali-valhall-jm` — v9 JM;
- vendor wrappers (`mediatek,mt8183-mali`, `amlogic,…`, `allwinner,…`, `renesas,…`).

There is **no separate compatible string for v6 vs v7**. The driver identifies the GPU at
probe time from `GPU_ID` and looks it up in `gpu_models[]` (`panfrost_gpu.c`), where
`GPU_MODEL(g71, 0x6000, GPU_REV_EXT(g71, 0, 0, 1, _05dev0))` is present. Our A8 device
tree uses `"arm,mali"` (Samsung's own driver), which is **not** in the panfrost table;
panfrost would need a fresh compatible string or the Samsung wrapper.

### 3.2 Clocks, regulators, power domains

`panfrost_device.c`:

- `clock` = required core clock; `bus`, `bus_ace` = optional (`devm_clk_get_optional`);
  - bus clock and `bus_ace` clocks are `clk_prepare_enable`'d defensively;
- regulators: `comp->num_supplies` names from `default_supplies` (most SoCs) or
  `legacy_supplies` (MT8183/Mali-G76 path);
- power domains: single domain handled by core; multi-domain attach by name only when
  `num_domains >= 2` and matches `comp->num_pm_domains`.

No firmware load anywhere in the panfrost path (grep: no `request_firmware`).

### 3.3 MMU / address space

`drivers/gpu/drm/panfrost/panfrost_mmu.c` + `panfrost_gpu.c` `panfrost_gpu_init_quirks`:
- AS registers start at `MMU_AS(as)`, with `AS_TRANSTAB_*`, `AS_MEMATTR_*`,
  `AS_TRANSCFG_*` (the `_TRANSCFG` pair is the "Additional Bifrost AS registers");
- fault addresses, status, commands per AS;
- the Bifrost-specific quirk for G71:
  ```c
  else if (panfrost_model_eq(pfdev, 0x6000) &&
           pfdev->features.coherency_features == BIT(COHERENCY_ACE))
     quirks |= (BIT(COHERENCY_ACE_LITE) | BIT(COHERENCY_ACE)) <<
                JM_FORCE_COHERENCY_FEATURES_SHIFT;
  ```
  i.e. on v6 gen1, an ACE-coherent G71 must be forced to use ACE_LITE+ACE for job
  manager descriptors. This is the one item that is genuinely v6-vs-v7-specific in the
  kernel; everything else (JM submission, decoder) is generic Bifrost.

### 3.4 Job descriptor format

Panfrost uses the **Job Manager** format for all of Bifrost (v6/v7/v8) and Valhall v9;
v10+ move to CSF (different ABI, separate code path). There is no separate "v6 job
descriptor." Job chain setup (`panfrost_job.c`) is generic JM. IDVS and varying-shader
handling differ between Bifrost and Valhall at the compiler/userspace layer (§2).

### 3.5 Firmware

**No firmware is required for Bifrost (v6 or v7) in upstream panfrost.** The kernel
drives the core directly through MMIO and JM job chains; this matches the issue's
expectation. (Firmware is used only by the CSF front-ends on v10+ panthor.)

## 4. What a "v7-only" build must change to enable v6

Concrete, in the order a packager actually hits them:

1. `tools/mesa/panfrost-darwin.patch` (**HakiOS's** tree — not present in this repo; the
   line numbers below are inherited from `docs/jackpotlte-mali.md` and were not
   re-checked): in the Darwin branch,
   change `panfrost_versions = host_machine.system() == 'darwin' ? ['7'] : ['4','5','6','7','9','10']`
   (lines `:197`/`:199`) to include `'6'`.
2. The per-architecture library list in the same patch (line `:187`) must also include `'6'`,
   otherwise `src/panfrost` has no `PAN_ARCH==6` build and the v6 dispatch symbol is
   undefined.
3. `pan_screen.c` Darwin `#ifdef __APPLE__` dispatch (patch line `:222`): add a
   `case 6:`/v6 arm that calls the v6 init path (upstream name
   `panfrost_cmdstream_screen_init_v6`, invoked with `PAN_ARCH==6`) instead of
   `return NULL` ("Unhandled architecture major"). HakiOS's current v7-only arm is the only
   reason a v6 part fails to create a screen.
4. Varying/attribute encoding (§2.3) already compiles in once `PAN_ARCH==6` is built; no
   extra switch needed.
5. Build options: `-Dgallium-drivers=panfrost`, optionally `-Dvulkan-drivers=panfrost`
   for PanVK (`jm_archs` already lists 6). LLVM is required at build time for the v6
   compiler (`mesa-clc`/`precomp-compiler` two-stage if cross-compiling without target LLVM).
6. `tools/mk-mesa.sh` (also HakiOS-side, absent here) pins `VERSION=25.0.7` and applies the patch series in order
   (`panfrost-redwood-perf.patch` does not apply cleanly to 25.0.7 on its own because it
   re-adds a `return true;`/`}` that stock 25.0.7 already contains). Run `git apply --check`
   of the whole series before trusting any build.
7. Conformance reality: even after the build, v6 (G71) is non-conformant in upstream; a v7
   reference binary cannot be treated as a v6 correctness oracle.

## 5. Darwin / iOS evidence

There is **no** evidence of panfrost v6 on Darwin or iOS:

- HakiOS (`hakios-xnu-kernel` @ `620c775`, `hakios-mt6768` @ `84b0252`) only documents
  Mali-G52 (Bifrost v7) on the MT6768 path; its Mesa Darwin patch is v7-only by
  construction. Neither HakiOS document fetched mentions v6, G71 or
  `arm,mali-bifrost` at all — it is a gap in the material, not a written claim.
- Public upstream Mesa panfrost history/test lists v6 only on Linux (`kmsro`/`drm-shim`
  with `PAN_GPU_ID=6221`, shader-db invocations); no Darwin target in that tooling.
- Nothing in this repo (`shim/jackpotlte/`, `docs/jackpotlte-mali.md`) or in
  HakiOS's `userland/src/` claims G71/v6 running on iOS/Darwin.

(If such a report exists it is not in the sources surveyed here; treat the question as
"unproven," not "impossible.")

## 6. What we must change on Exynos 7885 (jackpotlte)

1. Apply the Mesa build v6 gate (§4.1–4.3) to our own Mesa fork, because our reference is
   v7-only.
2. Keep the **G71 coherency quirk** in the kernel driver: when G3D reports ACE
   (`coherency_features == BIT(COHERENCY_ACE)`), force
   `JM_FORCE_COHERENCY_FEATURES = ACE_LITE | ACE` (§3.3). The A8's Mali-G71 is exactly
   this case, and dropping it would be a real v6 regression.
3. Exynos 7885 register map is Samsung-specific (`mali@0x11500000`, INTIDs 190/191/192,
   `pd_g3d`, CMU_G3D/PLL_G3D, no SMMU). The upstream panfrost compatible string does not
   bind; reuse/rewrite `RedwoodMali` against our register map rather than assuming an
   open DRM driver will probe.
4. Verify tiler `tiler_features` and `no_hierarchical_tiling` on the real die before
   trusting the G71 model entry's tilebuffer sizes (4096×4096). M0 probe first.
5. Re-evaluate HakiOS perf knobs (`pushubo`, `widen16`, `scratchregs`) on v6; they are
   arch-independent passes, but tuned constants are v7-G52-specific. Start with defaults.
6. Keep all three boot-arg gates opt-in (`mali=1`, `metal=1`, `camtl=1`) and only publish
   the two `IOAcceleratorES` properties once the v6 command stream is verified
   (`jackpotlte-mali.md §2`).

## 7. Confidence / what I could not confirm

- **Source drift.** Line numbers above (especially HakiOS's `bifrost_compile.c:1140`,
  `:5851`) come from the pinned donor tree (Mesa 25.0.7) as cited by
  `jackpotlte-mali.md`; current upstream main has refactored many of them (identity now
  near line 580). I confirmed the *expressions* in current upstream but did not re-verify
  every historical line number.
- **I did not build or run anything.** No device, no Mesa build, no kernel build, no CI.
  Claims about support status come from source/docs, not from an observed rendering result.
- **Kernel binding.** I could not confirm how a Darwin XNU port would map the panfrost
  coherency quirk into `RedwoodMali`; the v6-vs-v7 job descriptor differences are stated
  as "shared JM format" by upstream, but our custom driver is not upstream's.
- **G3D power state at boot.** Source cannot say whether S-Boot leaves G3D powered/clocked;
  must be checked by the read-only PMU readback probe.
- **v6 PanVK on Darwin.** Unverified and non-conformant upstream; no claim either way on
  feasibility.

Sources: upstream Mesa (docs/drivers/panfrost.rst; src/panfrost/model/pan_model.{c,h};
src/panfrost/compiler/bifrost/bifrost_compile.c; src/panfrost/{vulkan}/panvk_vX_shader.c;
src/panfrost/meson.build); upstream Linux kernel
(drivers/gpu/drm/panfrost/{panfrost_drv.c,panfrost_gpu.c,panfrost_device.c,panfrost_mmu.c});
local `docs/jackpotlte-mali.md` (HakiOS citations and Darwin patch citations);
panvk-g99-jm `REBASE-RESOLUTIONS.md` and `MALI-GPU-ARCHITECTURES.md` for upstream
arch lists.
