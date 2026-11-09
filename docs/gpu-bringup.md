# Galaxy A8 2018 (jackpotlte) GPU / Metal bring-up

Branch `jackpotlte-gpu`, forked from `main` at `523beb5`. Independent of the
touch work on `main`.

Plan: `docs/jackpotlte-mali.md` — M0 probe -> M1 power/clock -> M2 MMU + one
job -> M3 `RedwoodMali`-shaped user client over `SweetSurface` -> M4 Metal
plug-in with Bifrost **v6** enabled (our G71 is v6; the HakiOS reference is v7).

## Status

### M0 + M1 in patch `0144` (code complete, not yet on the phone)

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
   plus `GPU_ID`. A Bifrost GPU_ID with a matching arch nibble is M1 done.
4. M2: MMU + one job.

## Device safety

Only `BOOT` may be flashed during bring-up. Never bootloaders/EFS (p3)/recovery
(p11)/persist/GPT. `USERDATA` holds the iOS root and needs explicit owner
approval per destructive step.