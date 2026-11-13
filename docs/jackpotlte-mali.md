# Mali GPU / Metal 3D acceleration on jackpotlte — scoping

Status: **scoping only**. This document is a plan and an inventory; it changes
no boot path, applies no patch, and makes no claim about the A8's GPU beyond
what a named source says. The only code it proposes is a read-only register
probe (draft in `docs/drafts/jackpotlte-mali/`).

Read this with `JACKPOTLTE_PROGRESS.md` and `docs/jackpotlte-bringup.md`.
Nothing already established there (display/DSIM, secondary cores, storage,
panel) is in question here.

The reference implementation is HakiOS's Mali stack, which drives a
Mali-G52 MC2 on a MediaTek MT6768 (Infinix Hot 20) with panfrost + Mesa + a
Metal plug-in. The A8's GPU is **not** a G52; the sections below say why that
matters and where it is cheap to handle.

---

## 0. Verdict in four lines

- **The userland half is reusable almost verbatim.** The Metal plug-in, the
  AIR→NIR frontend, and Mesa's panfrost/native layer are device-neutral; they
  talk to the kernel only through a DRM shim and an IOKit user client named
  `RedwoodMali`. Reproduce that contract and they port.
- **The kernel half must be rewritten for Exynos 7885.** Power, clocks, IRQs
  and registers are Samsung-specific; the MTK driver in the reference is a
  different SoC's block with the same Mali core.
- **One hard build gap:** the Darwin panfrost patch compiles **Bifrost v7
  only** (G52). The A8 is **G71 = Bifrost v6**, so the Mesa build gate and the
  screen-init dispatch must gain v6. (Inference: v6 exists upstream and is
  re-enabled by a config edit; nothing has tested Mali v6 on Darwin.)
- **Biggest unknown:** whether S-Boot leaves G3D powered and clocked. If it
  does not, no GPU register read works until the CMU_G3D/PLL_G3D/pd_g3d
  sequence runs. That is decided by a device readback, not by source.

---

## 1. Reference inventory (commit-pinned)

| Repo | Branch / commit | What it contributes |
|---|---|---|
| `github.com/oosexgang/hakios-xnu-kernel` | `mtk` @ `620c775` | Kernel Mali driver (`xnu/files/...`), `userland/` Metal stack, Mesa patches |
| `github.com/oosexgang/hakios-mt6768` | @ `84b0252` | MT6768 bring-up notes, `mtkmali.h` register map, `redwood_mali.c` |
| `github.com/oosexgang/hakios-metal` | @ `d36f329` | Earlier home of the Metal driver; header says "moved into the kernel" |
| Mesa | `25.0.7` (upstream) | panfrost, the Darwin patch's base, `pan_props.c`/`pan_props.h` |

All three repos are **public** (`gh repo list oosexgang`). The 203 MB kernel
repo was not cloned; a blobs-off sparse clone
(`git clone --filter=blob:none --sparse -b mtk`) into `/tmp/opencode/hakios-xnu`
was used for the reads below and has since been deleted.

> **Correction to the working assumption.** The Mali driver layer on branch
> `mtk` @ `620c775` is **not** under a `mtk/mt6768/` path — there is no such
> path in `hakios-xnu-kernel`. It lives as XNU source under `xnu/files/iokit/`
> and `xnu/files/bsd/dev/` (applied by the repo's `xnu/apply.sh`) plus
> userland source under `userland/src/`. The MT6768-specific android driver
> (`kernel/bsd/dev/redwood_mali.c`, `mtkloader/mtkmali.h`) is in the separate
> `hakios-mt6768` repo.

### What is actually inside each half

Kernel side (`hakios-xnu-kernel`, branch `mtk` @ `620c775`):

- `xnu/files/bsd/dev/redwood_mali.c` (1146 lines) — power-up, MMU, job slots,
  worker thread. `mali=1` gate, see `RedwoodMali.cpp:4`.
- `xnu/files/iokit/Kernel/RedwoodMali.cpp` (633) — the IOKit user client /
  ioctl surface the userland speaks to.
- `xnu/files/iokit/Kernel/RedwoodGPU.cpp` (1207), `MiniIOSurface.cpp` (1872),
  `MiniIOMobileFramebuffer.cpp` (2071) — the framebuffer/accelerator plumbing.
- `tools/mesa/redwood-drm/redwood_drm.c` (767) — libdrm stand-in that turns
  DRM ioctls into `IOServiceOpen("RedwoodMali")` calls.
- `userland/src/redwoodmetal.m` (5938, 14 `@implementation`s, 553 methods) —
  the private `_MTLDevice` subclass.
- `userland/src/metal/*.cpp` (21 files) — the Metal→panfrost driver body.
- `tools/mesa/air/*` (`air_to_nir.c` 4644, `air_metal.c` 7749, `air_metal.h`
  1364) — the AIR bitcode reader.
- `tools/mesa/*.patch` — the Mesa changes, applied by `tools/mk-mesa.sh`.

### Availability: what is rebuildable from public source, and what is not

| Piece | Where | Public / rebuildable? |
|---|---|---|
| Metal plug-in (bundle) | `userland/src/redwoodmetal.m`, `metal_plugin.plist` | **Yes** |
| Metal properties helper | `userland/src/metal_props.c` | **Yes** |
| AIR→NIR frontend | `tools/mesa/air/*` | **Yes** |
| Mesa patches (incl. Darwin) | `tools/mesa/*.patch` | **Yes** |
| DRM stand-in | `tools/mesa/redwood-drm/redwood_drm.c` | **Yes** |
| Kernel GPU driver | `xnu/files/bsd/dev/redwood_mali.c`, `.../RedwoodMali.cpp` | **Yes** |
| `IOAcceleratorES`, iomfb | `xnu/files/iokit/Kernel/MiniIOMobileFramebuffer.cpp` | **Yes** |
| Mesa itself | upstream `mesa-25.0.7` | **Yes** |
| Prebuilt userland binaries | `prebuilt/userland/*` | **No** (not in repo) |
| UEFI / loader | `prebuilt/uefi/*`, `prebuilt/loader.efi` | **No** |
| QuartzCore shaders | Apple's `default.metallib` | **No** (Apple; never in repo) |
| `origin/air`, `friend/metal` history | private branches (a 6333-line `hkmetal.m`) | **No** |

The Metal stack's *source* is all public. The only non-public artifacts are
prebuilt binaries, Apple's own metallib, and dead private branches — none of
which the plan below needs to re-derive.

---

## 2. The layer architecture (verified, with citations)

Metal on iOS finds a renderer purely through IOKit. `docs/metal.md` §1 (the
reference's own reverse-engineering, verified against the iOS 16.5 dyld cache)
lays out the five steps, and the port's own `MiniIOMobileFramebuffer.cpp`
implements the two that are the port's responsibility:

```
app ── MTLCreateSystemDefaultDevice()
      └─ MTLRegisterDevices: IOServiceMatching("IOAcceleratorES")       [metal.md:29]
         └─ accelerator service with
              MetalPluginName      -> "RedwoodMetal"                    [MiniIOMobileFramebuffer.cpp:1900]
              MetalPluginClassName -> "RedwoodMetalDevice"              [MiniIOMobileFramebuffer.cpp:1901]
            └─ /System/Library/Extensions/RedwoodMetal.bundle
               NSPrincipalClass = RedwoodMetalDevice                    [metal_plugin.plist]
               ├─ class RedwoodMetalDevice : private _MTLDevice         [redwoodmetal.m]
               └─ dlopen /redwood/lib/libredwoodair.dylib
                     on a Mali = symlink libOSMesa.8.dylib              [userland/build.sh:334]
                     └─ air_metal.c: drmOpenWithType()
                        └─ redwood_drm.c: IOServiceOpen("RedwoodMali")  [redwood_drm.c:26]
                           └─ RedwoodMali.cpp user client
                                 ioctls 0x40..0x48 (kPanfrost*) and
                                 0x7c..0x7f (kRedwood*)                 [RedwoodMali.cpp:62-72]
                              └─ redwood_mali.c: power, MMU, job slots  [redwood_mali.c]
                                 └─ Mali GPU
```

Everything above `redwood_drm.c` is device-neutral in shape. The kernel side
is where the SoC appears.

### Boot-arg gates (all off by default in the reference)

| Gate | Meaning | Citation |
|---|---|---|
| `mali=1` | kernel brings the GPU up / exposes the `RedwoodMali` service | `RedwoodMali.cpp:4` |
| `metal=1` | `IOAcceleratorES::setProperties` accepts the two plug-in keys | `MiniIOMobileFramebuffer.cpp:1903`, `:1958` |
| `camtl=1` | CoreAnimation's Metal renderer is given QuartzCore's shaders | `inject.c:1106` |
| `pushubo=0` / `widen16=0` / `scratchregs=0` | perf knobs, default on | `tools/mesa/panfrost-redwood-perf.patch` |
| `rwm_async` / `rwm_finish` | job-submission policy knobs in the plug-in | `userland/src/redwoodmetal.m` |

The `metal=1` gate is deliberately opt-in: `MiniIOMobileFramebuffer.cpp:1925-1935`
explains that a half-made Metal device makes an app *assert*, and launchd
restarts an aborting daemon in a loop — so the properties are only published
for a boot made on purpose.

---

## 3. Exynos 7885 GPU hardware (from Samsung's own source)

All values below are from the on-phone Eureka 4.4.302 kernel tree
(`/var/tmp/opencode-keep/Eureka-Kernel`), whose DTB the A8 boots. They are
**not yet confirmed on the phone** — that is what a probe would do.

| Item | Value | Source |
|---|---|---|
| GPU register block | `mali@0x11500000`, size `0x5000`, `compatible "arm,mali"` | `exynos7885-mali_common.dtsi:15-17` |
| Interrupts (DT order) | `<0 159 0> JOB`, `<0 160 0> MMU`, `<0 158 0> GPU` | `exynos7885-mali_common.dtsi:18-20` |
| Interrupts (GIC INTID) | **191 JOB / 192 MMU / 190 GPU** (SPI INTID = DT irq + 32) | `irq-gic.c` translate; inference from the DT above |
| Power domain | `pd_g3d@11c840a0`, `cal_id 0xB1380004`, `reg` size `0x20` | `exynos7885-pm-domains.dtsi:49-54` |
| Power status register | `0x11C80000 + 0x40A4`, live mask `0xF` | `exynos7885-mali_common.dtsi:30-31`; `pmucal_local_exynos7885.h:275` |
| Local power-on sequence | WRITE `0x11C80000+0x40A0 = 0xF`, then WAIT `0x40A4 == 0xF` | `pmucal_local_exynos7885.h:274-275` |
| G3D clock block (CMU) | base `0x11400000`, size `0x8000` | `cmucal-sfr.c:17` |
| `PLL_G3D` con0 | `0x11400000 + 0x120`; enable = bit 31, lock = bit 29, gate = bit 4 | `cmucal-sfr.c:712`; `pmucal_local_exynos7885.h:288-290` |
| GPU bus divider | `CLK_CON_DIV_DIV_CLK_G3D_BUSP` @ `0x11400000 + 0x1800` | `cmucal-sfr.c:711` |
| G3D CMU gate | `..._G3D_CMU_G3D_..._PCLK` @ `0x11400000 + 0x2004` | `cmucal-sfr.c:717` |
| `CORE_G3D` clock | mux `0x12000000+0x101c`, div `+0x1824`, gate `+0x2024` | `cmucal-sfr.c:245-248` |
| Default GPU clocks | min `343 MHz`, max `1300 MHz` (`gpu_min_clock`, `eureka_gpu_max_clock`) | `exynos7885-mali_common.dtsi:29,61` |
| G3D rail | `vdd_g3d` = S2MPU08 `BUCK5`, 500–1300 mV, always-on | `exynos7885-jackpotlte_common.dtsi:196,261-267`; **not on I2C** — `s2mpu08` is ACPM-only (`0129` notes, `0129-...pmic...patch:36-42`) |
| GPU SMMU | **none** — no `samsung,sysmmu` node for the GPU; Mali uses its own MMU / AS0 | no such node in `exynos7885.dtsi`; inference |

Notes that matter for a driver:

- **The G3D power gate is local.** The `pd_g3d` control register is in the
  always-on PMU block and is written directly (`pmucal_local`), not through
  ACPM IPC. The `s2mpu08` *rail* is ACPM-only, but the reference's driver
  observes that the GPU rails are usually left on by the bootloader
  (`hakios-mt6768` `redwood_mali.c:19`, and the same is likely here) — so a
  first bring-up can reasonably try the gate + clocks and only add the rail if
  a readback says otherwise. **Labeled inference.**
- **The local G3D sequence already disables `PLL_G3D` on power-down**
  (`pmucal_local_exynos7885.h:288-290`: bit 31→0, wait bit 29, bit 4→0).
  That is the mirror of the power-up a driver needs, and it is the honest
  starting point for the ordering.

### Contrast with the MTK reference

`hakios-mt6768/mtkloader/mtkmali.h` (lines 1-21) documents the G52's wake
order: rails → `mfg_sel` mux → four SPM power domains → one gate (`BG3D` at
`0x13000000`). The Exynos 7885 has the same *shape* (rail → mux/gate → power
domain → internal CMU gate) but different register blocks: no SPM, no
`infra_toaxi` protection register, a PLL-based CMU_G3D, and a local PMU gate.
The MTK file is a **template for the order**, not a value table to copy.

---

## 4. The Bifrost version gap (the concrete risk)

- The A8's GPU is **Mali-G71** (Exynos 7885, Bifrost gen 1). panfrost models it
  by `gpu_id 0x6000` → `MODEL(0x6000, 0, "G71", ...)` `pan_props.c:61`.
- panfrost's arch is `gpu_id >> 12` for Bifrost/Valhall `pan_props.h:113-128`,
  so G71 is **arch 6**.
- The reference's Darwin patch compiles **only v7**:
  `panfrost_versions = host_machine.system() == 'darwin' ? ['7'] : [...]`
  (`tools/mesa/panfrost-darwin.patch:199`, `:187` for the per-arch library),
  and `pan_screen.c` **returns NULL unless `dev->arch == 7`**
  (`panfrost-darwin.patch:222`).

So porting the reference as-is to the A8 yields a screen that fails to
initialise with "Unhandled architecture major". The fix is small and has two
parts:

1. Add `'6'` back to both `panfrost_versions` lists in the Darwin branch of
   `meson.build` (the unpatched Mesa default is
   `['4','5','6','7','9','10']`, `panfrost-darwin.patch:197`).
2. Add a v6 arm to the `#ifdef __APPLE__` dispatch in `pan_screen.c` calling
   `panfrost_cmdstream_screen_init_v6` (the v6 function exists upstream; it is
   only excluded from the Darwin build).

The v6 backend is upstream and maintained, so step 1-2 is a build-configuration
change, not a missing backend: Mesa 25.0.7 carries the v6 identity constant
(`bifrost_compile.c:1140`, `b->shader->arch == 6 ? 0x688 : 0`) and
`bi_should_idvs` already returns true for vertex stages
(`bifrost_compile.c:5851-5853`). That also means the perf patch's `widen16` /
`scratchregs` / `pushubo` passes (all NIR-level, arch-independent) are the only
reason it is applied — not any v6 support.

**Inference (the actual risk):** nothing in HakiOS has driven a Mali v6
through the Darwin IOKit DRM shim. The gap above is about *compiling* v6; that
v6 then *works* on this transport is unproven and is only retired by building
and running. **Caveat on the patch series:** `panfrost-redwood-perf.patch` adds
a `return true;` / `}` at `bifrost_compile.c:5851` that stock 25.0.7 already
has, so it is not a clean apply against 25.0.7 in isolation.
`tools/mk-mesa.sh:24` pins `VERSION=25.0.7` and applies the series in order, so
another patch (or a fuzz-tolerant apply) is expected to reconcile this; the
exact order must be confirmed by a `git apply --check` before any build.

> A second, smaller unknown: the G71 has a different tiler/shader-core count
> from the G52, so the perf knobs tuned on the reference may be wrong for the
> A8. That is tuning, not correctness, and is out of scope for the first
> milestone.

---

## 5. Gaps in the jackpotlte tree

Our tree is the SWEET-derived jackpotlte port. It has **no GPU work at all**:

- `shim/jackpotlte/board.h` has no GPU entries (no `JT_GPU_*`), by design —
  it only lists what has been read from a source (`board.h:1-17`).
- No `redwood_*`, no `panfrost`, no `IOAcceleratorES` anywhere in the tree
  (only `NOTES.md`, the SWEET donor history, mentions `IOAcceleratorES`).
- The display path is the SWEET one: `SweetSurface` + `SweetFramebuffer`
  (`AppleCLCD`), added by `xnu/patches/0046-sweet-iosurface-iomfb.patch`,
  **not** the reference's `MiniIOSurface` / `MiniIOMobileFramebuffer`.
- Where a BO's memory lives, in our tree:
  `s->memory = IOMemoryDescriptor::withPhysicalAddress(...)`
  (`0046:361`), pixels at `s->offset` (`0046:411`, `:689`), and client
  surfaces are mapped `kIOMapDefaultCache` (`0046:806-807`) — **not**
  write-combine, so a GPU driver would have to flush/invalidate.
- `AppleCLCD::swap(const uint8_t *rec)` (`0046:1217`) copies layer surfaces to
  panel memory by **CPU memcpy** (`0046:1287`) — the present path a GPU
  drawable would eventually have to replace or feed.

What a jackpotlte port therefore needs that the reference had for free:

1. An `IOAcceleratorES` service and its two properties, published where our
   display/boot stack lives.
2. A `RedwoodMali`-equivalent user client over our surface model
   (`SweetSurface` id/offset) rather than `MiniIOSurface`.
3. Register/power/clock/IRQ sequencing for Exynos 7885 (§3).
4. The v6 Mesa build fix (§4).

Items 1-2 are the reference's *structure* adapted to our *surface type*;
item 3 is new code; item 4 is a config edit.

---

## 6. Milestones (lowest-risk first)

Each milestone is independently useful and independently abandonable. None
touches the boot path except where noted.

- **M0 — read-only GPU probe (this document's draft).** Map the always-on PMU
  and CMU_G3D windows; read `pmu_g3d` status (`0x11c840a4`); read `GPU_ID` at
  `0x11500000` **only if the status says the domain is up**, to avoid stalling
  a bus on a live phone. Prints a report; changes nothing. Proves
  addressability, yields the real product id, and answers the S-Boot-state
  question. Gate off by default.
- **M1 — power and clocks.** `pd_g3d` local gate (`0x40A0`) + `PLL_G3D` +
  `CMU_G3D` gates, in the order the local sequence implies. Re-read `GPU_ID` and
  the presence/ready registers (`SHADER`/`TILER`/`L2` `_PRESENT` and `_READY`).
  Still no jobs.
- **M2 — MMU and one no-op job.** Port the AS0 setup and a single job through
  the reference's structure (`redwood_mali.c`), with INTIDs 191/192/190.
  Success = an IRQ and a job-completion counter, no rendering.
- **M3 — user client + surfaces.** A `RedwoodMali`-shaped user client whose
  BOs are `SweetSurface`s, reachable through a `redwood_drm.c`-shaped shim.
  Proves the userland contract before any Metal.
- **M4 — GPU rendering in the compositor (route decided 2026-11-13: GLES, not
  Metal).** Get CoreAnimation drawing through Mesa: build panfrost v6 for iOS
  (written, `tools/mesa-jackpotlte/`) and back `EAGLContext` with it, so
  QuartzCore's EAGL renderer — the one it falls back to when there is no Metal —
  and CoreImage's GL backend run on the Mali. Draw a triangle first, then the
  wallpaper. The reason this is GLES and not Metal is the reference's own Metal
  analysis (`hakios-xnu-kernel` branch `mtk`, `docs/metal.md` §3/§4/§7): Metal is
  handed shaders as Apple's AIR bitcode, which has no reader anywhere, while the
  compositor's CPU rendering — the problem here — is on the GLES path and does
  not need Metal. The Metal plug-in (`0155`'s bundle plus the reference's AIR
  frontend) is a later, separate milestone; that is the part that would reuse the
  reference's Mali userland, and its licence question is parked with it.

Estimated shape: M0-M1 are small (one driver file + board.h facts); M2-M3 are
medium (the port of a 1146-line driver to a new register map + a new surface
binding); M4 is mostly integration and the v6 build, plus the EAGL shim and
whatever Darwin surprises the v6 command stream holds.

---

## 7. Reuse vs. write — summary

| Reuse as-is (structure) | Adapt | Write new |
|---|---|---|
| Metal plug-in `redwoodmetal.m` | `redwood_drm.c` shim target (`SweetSurface`) | Exynos 7885 reg/power/clock/IRQ driver |
| AIR frontend `tools/mesa/air/*` | Mesa Darwin build gate (add v6) | `IOAcceleratorES` glue in our display stack |
| panfrost native layer + patches | Surface/BO model (`SweetSurface` offset/cache) | — |
| `RedwoodMali.cpp` user-client shape | ioctl/SweetSurface binding | — |

The licence side of this table is checked, file by file and commit by commit, in
`docs/mali-userland-provenance.md`: the AIR frontend and the SoC-side driver have
a grant that travels with them, the Metal driver and the plug-in do not. With the
GLES route chosen (§6, M4), none of those are needed yet — Mesa is upstream and
the drm face is ours — so the decision on the Metal files is deferred with the
Metal plug-in.

---

## 8. The single biggest unknown

**Whether S-Boot leaves G3D powered and clocked.** If it does not — and the
reference's own driver expects bootloader-left-on rails but says nothing about
Exynos — then no read of `0x11500000` succeeds until the CMU_G3D/PLL_G3D and
`pd_g3d` sequence has run. Source cannot answer this; the PMU status register
(`0x11c840a4`, in the always-on block) and the GPU id register can, with one
readback. M0 is designed to make that readback safe (status first, id only if
powered). Until it is done, every later milestone's feasibility is assumed.

---

## 9. The EAGL shim (GLES route, M4b)

The route chosen for M4 (§6) puts CoreAnimation on Mesa. The library M4 builds
(`tools/mesa-jackpotlte/`, `libOSMesa.8.dylib`) is the bottom of it: Mesa's
panfrost v6 behind an OpenGL ES entry point. The piece above it is an
`EAGLContext` implementation of our own, loaded into the window server
(`backboardd`), because this port's CoreAnimation currently draws in software
(`docs/upstream-README.md`).

How the reference does it, from `userland/src/eagl.c`'s own header (HakiOS,
branch `mtk` @`620c775`) — this is a description of *their* design, not a plan
we have to copy:

- `libredwood-gl.dylib` is "an OpenGL ES implementation behind iOS's
  EAGLContext, inside the window server (backboardd; libredwood.dylib loads this
  when the image has it: build.sh GL=1)".
- CoreAnimation "asks six things" of an `EAGLContext`, and all six are replaced:
  `-initWithAPI:properties:` (make a context of the library behind us), `-API`
  (3), `-setParameter:to:`, `-getMacroContextPrivate` (a table of GL functions at
  iOS's own `OpenGLES` entry offsets), `-texImageIOSurface:…` and
  `-attachImage:toCoreSurface:…` (the IOSurface's memory becomes the texture;
  where it cannot, the texture is copied in and out).
- The table's offsets are per iOS build: the reference generates
  `src/eagl_slots.txt` (409 entries, 62 used by QuartzCore) with `mkeagl.py` from
  *its* shared cache. **Ours must be generated from this port's cache** (iOS
  16.7.16, 20H392), not copied — the reference's is iOS 16.5 (20F66).
- The reference's CoreImage failure was `-[EAGLContext sharegroup]`, which it had
  not replaced; CoreAnimation and CoreImage ask for different methods, so a shim
  that satisfies the first need not satisfy the second.

Pieces for this port, none written yet:

1. an `EAGLContext` replacement that calls Mesa (OSMesa), in `userspace/` beside
   `libjackpotlte`;
2. the GL entry-point table, generated from the phone's own shared cache by a
   `tools/` script (the reference's `mkeagl.py` is the shape, not the source);
3. a loader that puts the replacement inside `backboardd`. This port has no
   `libredwood.dylib`, so the mechanism — a launchd environment insert, a dyld
   interpose, or a replacement `OpenGLES` binary — is an open question to settle
   against this root;
4. the verification rule stands: a library that loads is not a frame. The first
   claim that CoreAnimation draws on the Mali needs a photo.

Deployment goes into the iOS root (the existing `userdata` partition), which
`AGENTS.md` says needs the owner's confirmation before writing; it is not a BOOT
change.

Licence: this is our own code (the chosen route). The reference's `eagl.c` is
under the same README-only MIT statement as the rest of its Mali userland
(`docs/mali-userland-provenance.md` §2), so nothing from it is needed here — and
its slot table is for a different iOS build anyway.

---

## Sources

- HakiOS reference: `github.com/oosexgang/hakios-xnu-kernel` @ `620c775`
  (branch `mtk`); `github.com/oosexgang/hakios-mt6768` @ `84b0252`;
  `github.com/oosexgang/hakios-metal` @ `d36f329`.
- Mesa 25.0.7: `src/panfrost/lib/pan_props.c`, `.../pan_props.h`,
  `.../compiler/bifrost_compile.c`.
- A8 hardware: Samsung Eureka 4.4.302 tree
  (`arch/arm64/boot/dts/exynos/dtb/exynos7885-{mali_common,pm-domains}.dtsi`;
  `drivers/soc/samsung/cal-if/exynos7885/{cmucal-sfr.c,pmucal_local_exynos7885.h,pmucal_system_exynos7885.h}`).
- This repo: `shim/jackpotlte/board.h`,
  `xnu/patches/0046-sweet-iosurface-iomfb.patch`,
  `xnu/patches/0129-jackpotlte-pmic-readonly-probe.patch`.
