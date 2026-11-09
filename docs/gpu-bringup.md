# Galaxy A8 2018 (jackpotlte) GPU / Metal bring-up

Branch `jackpotlte-gpu`, forked from `main` at `523beb5`. Independent of the
touch work on `main`.

Plan: `docs/jackpotlte-mali.md` — M0 probe -> M1 power/clock -> M2 MMU + one
job -> M3 `RedwoodMali`-shaped user client over `SweetSurface` -> M4 Metal
plug-in with Bifrost **v6** enabled (our G71 is v6; the HakiOS reference is v7).

## Status

### M0 + M1 in patch `0144` (code complete, not yet on the phone)

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

## Next

1. CI-green on this branch (`gh workflow run xnu.yml --ref jackpotlte-gpu`),
   verify the kernel really contains the strings (`strings | grep JACKPOTLTE MALI`).
2. Pack with `cpus=8 ... jtmaliprobe=1` (probe only) and flash BOOT; read the
   `g3d_config`/`g3d_status` line. That answers whether S-Boot leaves G3D up.
3. Then pack with `jtmalipwr=1`, flash, and read the power/QCH/PLL/gate lines
   plus `GPU_ID`. A Bifrost GPU_ID (arch nibble 6) with `M1 done, GPU up and
   reset` and the `*_READY` bits set is M1 done.
4. M2: MMU + one job (recipe below).

## M2 recipe (from the reference, not yet written)

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