# Galaxy A8 2018 (jackpotlte) GPU / Metal bring-up

On `main` since 2026-11-10: the `jackpotlte-gpu` branch was fast-forwarded into
`main` and then retired (worktree, local branch and remote branch all deleted),
so the GPU patches and the touch work are one series again (`0152` onward).

Plan: `docs/jackpotlte-mali.md` — M0 probe -> M1 power/clock -> M2 MMU + one
job -> M3 `RedwoodMali`-shaped user client over `SweetSurface` -> M4 Metal
plug-in with Bifrost **v6** enabled (our G71 is v6; the HakiOS reference is v7).

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

### M3 in patches `0153` + `0154` (building)

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

> The first build of the pair (run `42869660692`) was superseded before it
> finished: the tree builds C **and** C++ with `-Weverything -Werror`, and
> three warnings would have failed it.  `0153`'s new non-static functions had
> no prototypes; `0154`'s `externalMethod` does not use
> `dispatch`/`target`/`reference` (`-Wunused-parameter`); and `0154`'s
> `extern "C" jackpotlte_mali_service_start` had no declaration above its
> definition (the `extern "C"` block in the same file did not mention it) —
> `-Wmissing-prototypes` fires in C++ too, which is why the reference declares
> `RedwoodMaliStart` at the top of its own `.cpp`.  All three were reproduced
> locally with the system clang on a snippet of the same shape, then fixed
> (`__unused`, like the other user clients in this tree at
> `0038`/`0040`/`0046`; a prototype block for the C file; a file-scope
> `extern "C"` declaration for the C++ one).  `fc3c750` then `3210c1f`
> rebuilt `main`.

## Next

1. Flash `/var/tmp/a8-xnu-jt39-gpum2.img` to BOOT and read the two
   `job ... -- the GPU ran it` lines; both must print `word=0xc0ffee01`
   (that closes M2).
2. Build `0153`+`0154` (push to `main` triggers `xnu.yml`), verify the new
   strings (`engine up`, `JackpotlteMali:`) in the downloaded kernel, then boot
   with `jtmalimetal=1` and confirm the engine and the service come up.
3. M4: build the `redwood_drm`-shaped shim and Mesa with Bifrost **v6**
   enabled, then publish `IOAcceleratorES` properties under a boot arg.

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