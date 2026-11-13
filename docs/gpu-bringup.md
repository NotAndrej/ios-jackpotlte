# Galaxy A8 2018 (jackpotlte) GPU / Metal bring-up

On `main` since 2026-11-10: the `jackpotlte-gpu` branch was fast-forwarded into
`main` and then retired (worktree, local branch and remote branch all deleted),
so the GPU patches and the touch work are one series again (`0152` onward).

Plan: `docs/jackpotlte-mali.md` — M0 probe -> M1 power/clock -> M2 MMU + one
job -> M3 `RedwoodMali`-shaped user client over `SweetSurface` -> M4 GLES: Mesa
panfrost **v6** (our G71 is v6; the HakiOS reference is v7) behind an
`EAGLContext` shim.  M4 was first sketched as a Metal plug-in; that route is
deferred and the GLES one settled instead (`docs/jackpotlte-mali.md` §6,
2026-11-13).

## Status

### M0 + M1 in patch `0144` (confirmed on the phone 2026-11-09)

> **Verify before flashing:** a green CI run is not proof the driver is in the
> kernel. The first "green" build of this patch silently skipped it — blacktop's
> `build.sh` only warns on `git apply` failure — so always confirm
> `strings <kernel> | grep 'JACKPOTLTE MALI'` before packing (see
> `pack-gpu.sh`, which does this). The workflow now also fails if any
> `xnu/patches/*.patch` is skipped.

`xnu/patches/0144-jackpotlte-mali-probe.patch`, entry point
`jackpotlte_mali_probe()`, called from `IOKitBSDInit` next to the touch probe.

- **`jtmaliprobe=1`** — prints `g3d_config` (PMU `0x40A0`) and `g3d_status`
  (PMU `0x40A4`), which are always-on, then the `PLL_G3D` state and the Mali
  `GPU_ID`/`*_PRESENT` registers **only if the domain reports up**. The earlier
  draft read `CMU_G3D`/`PLL_G3D` before checking status; those registers live in
  the G3D domain (the vendor's `g3d_save[]` treats them as lost when it is off,
  `pmucal_local_exynos7885.h:279-306`), so that read could stall the bus. Fixed.
- **`jtmalipwr=1`** — M1: writes PMU `0x40A0=0xF` and waits for `0x40A4&0xF==0xF`
  (`g3d_on[]`, `pmucal_local_exynos7885.h:273-276`), then enables the seven
  `CMU_G3D` QCHs (`en` bit0, `cmucal-sfr.c:2140`, `ra.c:440`), sets and waits
  for `PLL_G3D` `ENABLE` (bit31) / `STABLE` (bit29) (`ra.h:37-40`), and opens
  the eleven per-IP clock gates in manual mode (`MANUAL` bit20 + `CG_VAL`
  bit21, `ra.c:391-410`, offsets `cmucal-sfr.c:717-729`). It logs the raw
  values before/after each step.
- **M1 also brings the GPU itself up** (`jt_mali_up()`): once the register
  reads answer, read `SHADER`/`TILER`/`L2_PRESENT`, soft-reset via `GPU_CMD`
  and wait `GPU_INT_RAWSTAT` reset-complete, then `L2`/`TILER`/`SHADER_PWRON`
  and wait the matching `*_READY` bits — the reference's `mali_up()` step 4
  (`redwood_mali.c:544-567`). A Bifrost needs no firmware, so `M1 done, GPU up
  and reset` plus a Bifrost `GPU_ID` (arch nibble 6 for a G71) is M1 complete.

Deliberately **not** touched yet: the G3D clock dividers (`CLK_CON_DIV_DIV_CLK_G3D_BUSP`
`0x1800`, `CLK_CON_MUX_MUX_CLK_G3D_BUSD` `0x1004`) and the PLL dividers. The
patch logs `PLL_CON0/1/2` before enabling so the next run can see whether the
dividers survived the domain being off before anything is reprogrammed.

Nothing from this branch has been flashed, so M0/M1 have **no device result**.
(Update 2026-11-09: the `0144` merged into `main` was booted; its console
printed `g3d_config=0xf status=0xf`, `pll_g3d_con0=0xa1570d11 enable=1
stable=1`, `GPU_ID=0x60000002 id=0x6000 arch=6 rev=0x2`, and `reset done,
GPU up and cores powered` with `shaders=0x3 tiller=0x1 l2=0x1`, so **M0 and
M1 are done on hardware**. See `JACKPOTLTE_PROGRESS.md` on `main`.)

### M2 in patch `0152` (green build, image packed, not yet on the phone)

CI `42692773680` (branch `jackpotlte-gpu` @ `d226b5d`, `0152` on top of
main's `0151`; the branch was rebased onto main and the patch renumbered
`0149` -> `0152` so the two branches no longer collide at `0149`) was
**success**.  After the branch became `main` (`0c48a82`, a fast-forward), run
`42866306317` rebuilt the same tree, also **success**; its `xnu-kernel`
(16,571,352 B, sha256 `bca16b89...434cf3e3`) carries `mmu_features=...`,
`job 1, identity mode`, `job 2, GPU page tables`, `M2 done as0_status=...
fault=...` and `-- the GPU ran it`, **and** the `0150`/`0151` touch strings
(`A8 TOUCH: event backlog left=... draining`, `A8 TOUCH: lift watchdog`) with
`sweethidclients` present.

> A watcher first reported the two `job ...` strings missing. It had grepped
> `kernel.development.a5xelte.lse.txt` (a 30-byte sidecar) instead of
> `kernel.development.a5xelte`, because its `find -regex` matched the sidecar
> first. Always point `strings`/`grep` at the real kernel file.

Image packed from that verified kernel:
`/var/tmp/a8-xnu-jt39-gpum2.img` (sha256
`ae6a99e9d055d6fe48ed8fc139396b16621cd45b6d2a89119acdb44bc2379fde`), base
cmdline plus `jtmaliprobe=1 jtmalipwr=1 jtmalijob=1` (`cpus=8`, no
`sweethidtest`).  M2 is done only when the phone prints `word=0xc0ffee01` for
**both** jobs; nothing has been flashed from this series yet.

### M3 in patches `0153` + `0154` (green, image packed, not yet on the phone)

- `0153-jackpotlte-mali-engine.patch` turns the M2 helpers into a persistent
  engine (gate `jtmalimetal=1`, after `jtmaliprobe=1 jtmalipwr=1`): dynamic
  three-level page tables (`jt_pt_below`/`jt_map_page`/`jt_unmap_page`), the
  GPU-address allocator, `jackpotlte_mali_parameter/address/map/unmap/mapped/
  submit`, and a 64-entry job queue run by a worker thread
  (`jackpotlte_mali_enqueue/wait`). Ported from `redwood_mali.c:583-1146`.
- `0154-jackpotlte-mali-user-client.patch` adds
  `iokit/Kernel/arm/JackpotlteMali.cpp` (registered in
  `iokit/conf/files.arm64`, started from `IOKitBSDInit` after the probe). It
  speaks the panfrost ioctls `0x40-0x48`, plus `0x7d` (wait for a job by
  number) and `0x7e` (a `SweetSurface`'s pixels as a buffer). Buffers are
  kernel-user-shared memory; a surface is mapped from its pixel `offset` so the
  buffer's base is pixel 0. The service is created only once
  `jackpotlte_mali_ready()` is true.

Success for M3 on the phone: `JACKPOTLTE MALI: engine up, addresses from
0x1000000` and `JackpotlteMali: the GPU is there for programs to draw with`.

Run `42871632907` (`main` @ `ae9582f` + the docs) is **success**.  Its
`xnu-kernel`, the real `kernel.development.a5xelte` (16,598,424 B, sha256
`a34d2968...93297f3a`), carries `engine up, addresses from`,
`JackpotlteMali: request`, `the GPU is there for programs to draw with`,
`the GPU did not come up`, both `job ...` lines, `M2 done as0_status=...` and
the `A8 TOUCH: event backlog left=...` / `lift watchdog` strings — so the two
patches really are in the kernel.  Image packed:
`/var/tmp/a8-xnu-jt42-gpum3.img` (17,518,608 B, sha256
`536ab7edcb8229a2c822d6232c7fbed83d5066388414d5fab0d2b74c4a120901`), base
cmdline plus `jtmaliprobe=1 jtmalipwr=1 jtmalijob=1 jtmalimetal=1` (`cpus=8`,
no `sweethidtest`; the cmdline was read back out of the image to check).

> **Checked here, without CI or a device** (this host has no Apple SDK, but it
> can still do a lot).  Download a green run's `xnu-buildtree` artifact (it
> holds `xnu/` — the real post-`0152` sources — plus `sdk-kernel-platform/`
> and `build/xnu.obj/`), unpack it, then
> `git apply --check xnu/patches/0153*.patch xnu/patches/0154*.patch` from its
> `xnu/` dir: both apply (the only offset is one line in `IOKitBSDInit.cpp`).
> `tools/xnu-cc.py` re-runs the CI compile commands locally
> (`python3 tools/xnu-cc.py LOG BT bsd/dev/jackpotlte_mali.c
> iokit/Kernel/arm/JackpotlteMali.cpp iokit/bsddev/IOKitBSDInit.cpp
> --per-file`, with `CLANG` pointed at the system clang); all three pass
> (`0 of 3 files failed`).
> With `XNU_CC_OBJDIR` set it emits real arm64 objects, and `llvm-nm` then
> shows `JackpotlteMali.o`'s only non-Apple undefined symbols are the
> `jackpotlte_mali_*` ones, all defined in `jackpotlte_mali.o`, plus
> `sweet_surface_copy`, and that `jackpotlte_mali_service_start` comes out
> unmangled as `_jackpotlte_mali_service_start`, which is what
> `IOKitBSDInit.cpp` calls.

> The first build of the pair (run `42869660692`) was superseded before it
> finished, for three warnings only: `0153`'s new non-static functions had no
> prototypes (`-Wmissing-prototypes`); `0154`'s `externalMethod` does not use
> `dispatch`/`target`/`reference` (`-Wunused-parameter`); and `0154`'s
> `extern "C" jackpotlte_mali_service_start` had no declaration above its
> definition — `-Wmissing-prototypes` fires in C++ too, which is why the
> reference declares `RedwoodMaliStart` at the top of its own `.cpp`.  All
> three were reproduced locally with the system clang on a snippet of the same
> shape and fixed (`fc3c750`, `ae9582f`; `__unused`, as the other user clients
> in this tree at `0038`/`0040`/`0046`).
>
> **They were warnings, not build failures.** The kernel is built with
> `-Weverything` but **not** `-Werror`: the compile command in the CI log has
> no bare `-Werror`, and green run `42866306317` contains two
> `-Wmissing-noreturn` warnings (`a5xelte_keys.c`, `a5xelte_touch.c`) among
> 553,341 warnings in the whole log.  A red build is only ever a patch that
> would not apply (`Skipping patch`) or a compile/link error, so check for
> `-Werror` in the logged compile command before treating a warning as fatal.
> (The supersede cost two CI cycles.)  `jt_mali_worker`'s own `for (;;)`
> still draws `-Wmissing-noreturn`; it is harmless.

## Next

1. Flash `/var/tmp/a8-xnu-jt39-gpum2.img` to BOOT and read the two
   `job ... -- the GPU ran it` lines; both must print `word=0xc0ffee01`
   (that closes M2).
2. ~~Build `0153`+`0154`~~ Done: run `42871632907`, image
   `/var/tmp/a8-xnu-jt42-gpum3.img`.  Boot it (BOOT only) and read
   `JACKPOTLTE MALI: engine up, addresses from 0x1000000` plus
   `JackpotlteMali: the GPU is there for programs to draw with`; that closes
   M3.  Both images are packed and waiting — the phone is the blocker.
3. M4: **the GLES route, settled 2026-11-13** (`docs/jackpotlte-mali.md` §6).
   The `redwood_drm`-shaped shim (`userspace/libjackpotlte`) and Mesa 25.0.7
   panfrost v6 are written and the Mesa build is green (CI `43491401891` on
   `60aba9d`); the `EAGLContext` shim (`userspace/jackpotlte-gl/`) is written
   and its userspace CI green (`43489433732` on `069825f`).  `0155` and the
   `JackpotlteMetal` bundle below are deferred with the Metal route.  What is
   left: the loader (`0157`) plus a `userdata` write for the two dylibs, then a
   BOOT image and a photo (§9 of that document).

### Metal route, step 1 in `0155` (written, build-green, not yet on the phone; deferred)

`0155-jackpotlte-metal-accelerator.patch` publishes an `IOAcceleratorES`
service from `AppleCLCD::start`, the class the reference adds in
`iokit/Kernel/MiniIOMobileFramebuffer.cpp:1934-1971`.  It exists unconditionally
and carries **no** Metal properties by default: CoreAnimation waits for a
service of that class before it looks for a GPU at all, and one that does not
name its Metal driver is passed over by Metal, which is the same software
renderer as before.  With boot-arg `jtmetal=1` it also sets
`MetalPluginName` = `JackpotlteMetal` and `MetalPluginClassName` =
`JackpotlteMetalDevice` — the names the bundle in M4's next step must have.

Why the gate is not optional: with those properties Metal loads the bundle and
treats the service as a real GPU; a device that cannot draw yet makes a program
that asks for Metal assert, and CoreAnimation would then pick Metal for the
screen (the reference hit exactly this: `docs/metal-plan.md`, step 1's "the next
wall").

Checked here, without CI or a device: the patch applies to a green run's
post-`0154` `xnu-buildtree` (`git apply --check`, clean) and `tools/xnu-cc.py`
compiles `iokit/Kernel/arm/SweetFramebuffer.cpp` with that run's own compile
command (`0 of 1 files failed`).  Boot check when the phone is free: with
`jtmetal=1` the log must show `the accelerator names JackpotlteMetal as its
Metal plug-in`; without it, no such line and no change.

The earlier sketch of this step (`docs/drafts/jackpotlte-mali/
0138-jackpotlte-ioacceleratores.draft`) is superseded by `0155` and says so.


## M4 groundwork: what the reference's userland needs from us

Reconnaissance on the reference's shim (`redwood_drm.c`, the HakiOS tree cited
in `docs/jackpotlte-mali.md` §Sources), so M4 starts from facts.  None of this
is built or run yet.

- Every request is `IOConnectCallStructMethod(connection, number, data, size,
  data, &returned)` — the same structure in and out, which is exactly what
  `JackpotlteMaliUserClient::externalMethod` implements, so the shim's `call()`
  needs no change but the service name (`RedwoodMali` -> `JackpotlteMali`).
- Numbers: Linux's `0x40`-`0x48` as they are, `0x7d` for "wait for job N" and
  `0x7e` for "a surface as a buffer" — the ones `0154` already answers, and
  `0xffffffff` in `handle` meaning "describe only".
- `mmap()` is `IOConnectMapMemory64(connection, handle, ...)`; our
  `clientMemoryForType(handle)` returns that buffer's memory, so a buffer's
  handle is its map type, unchanged.
- The surface request is field-for-field the reference's `struct
  redwood_surface` (checked against its `RedwoodMali.cpp`), so the ABI needs no
  work.  The one real difference is the state page: a MiniIOSurface has one at
  the front of its memory, a `SweetSurface` keeps `state` outside `memory`, so
  `0154` maps from the pixels' `offset` and reports that `offset` and the size
  from it back to the shim.
- Fences are job numbers, so the reference's `syncobj`/`syncfile` bookkeeping
  carries over; its `drmPrime*` return `ENOSYS` and would here too.
- Mesa still has to be built with Bifrost **v6** on: our G71 is arch 6 and the
  reference's G52 is v7, so the arch gate (`pan_screen.c`'s dispatch, and the
  version checks in `pan_props.c`/`bifrost_compile.c`) must accept 6 or the
  driver rejects the GPU — see `docs/jackpotlte-mali.md` §4.
- Missing on our side and harmless until something needs them: a separate
  address space per client (everyone shares AS0), interrupts (submission polls
  `JOB_INT_RAWSTAT`), and buffers shared between programs.
- Open: where the shim belongs in this repo.  It is userspace for the iOS root
  and this repo has no userspace tree yet (`shim/` is the boot shim); the
  reference keeps it under `tools/mesa/redwood-drm/`.

## M2 recipe (from the reference, written as `0152`)

`xnu/patches/0152-jackpotlte-mali-mmu-job.patch` extends
`jackpotlte_mali.c`; gate on `jtmalijob=1` and only run after `jt_mali_up()`
returns true.

Port from `hakios-xnu xnu/files/bsd/dev/redwood_mali.c`; the same Linux
panfrost register map applies to a Bifrost G71. Gate on `jtmalijob=1` and only
run after `jt_mali_up()` returns true.

- Job slot 1 at `JS1 = 0x1880`; per-slot: `JS_STATUS 0x24`,
  `JS_HEAD_NEXT_LO 0x40`, `JS_HEAD_NEXT_HI 0x44`, `JS_AFFINITY_NEXT_LO 0x50`,
  `JS_CONFIG_NEXT 0x58`, `JS_COMMAND_NEXT 0x60` (write 1 to start);
  `JS_CONFIG = (3<<8)|(3<<12)|(8<<16)` (AS 0, flush caches both ends).
- Interrupts: `JOB_INT_RAWSTAT 0x1000`, `JOB_INT_CLEAR 0x1004`,
  `JOB_INT_MASK 0x1008`; completion is `(1<<1)|(1<<17)` with `1<<1` wanted.
- Address space 0 at `AS0 = 0x2400`: `AS_TRANSTAB_LO 0x00`/`_HI 0x04`,
  `AS_COMMAND 0x18` (1 = update), `AS_FAULTSTATUS 0x1c`, `AS_STATUS 0x28`
  (wait for 0), `AS_TRANSCFG_LO 0x30`, `AS_MEMATTR_LO 0x08 = 0x88888888`
  (all write-back).
- Two jobs, the reference's `first_jobs()`: first **identity** (`AS_TRANSCFG`
  mode 2, transtab 0 — needs no page tables, so it proves the job manager
  works); then the same job through **AArch64 4 KB page tables**
  (`AS_TRANSCFG` mode 6 with the walk flags). The job is the reference's
  `run_write_job`: a header at page+0 saying "write value" (`word[4] =
  (2<<1)|1|(1<<16)`), a 64-bit target at +0x800, immediate 6, value
  `0xc0ffee01`; success = `*target == 0xc0ffee01`. Flush the page from the
  processor caches (GPU here does not snoop them) before and after.
- Page tables: AArch64 stage-1, 4 KB, three levels for 35 address bits
  (`top[(gpu>>30)&31]`, `mid[(gpu>>21)&511]`, `bot[(gpu>>12)&511]`); PTE =
  `phys | AF | SH_INNER | AP_RW | 3`.
- Success = the word is written and `JOB_INT_RAWSTAT` shows the completion
  bit, with `AS_FAULTSTATUS` clear.

## Device safety

Only `BOOT` may be flashed during bring-up. Never bootloaders/EFS (p3)/recovery
(p11)/persist/GPT. `USERDATA` holds the iOS root and needs explicit owner
approval per destructive step.