# Galaxy A8 2018 (SM-A530F, jackpotlte) port — progress

Branch: this repository ports the Galaxy S8 (`dreamlte`, Exynos 8895) iOS work
to the **Galaxy A8 2018 (SM-A530F, `jackpotlte`, Exynos 7885)**. The S8 tree is
the base; we adapt it, we do not rewrite it.

Observation is kept strictly apart from plan, and build success is never a boot
claim. **A boot stage counts as reached only with a photo, serial log or screen
output from the phone.**

## Stage status

| Stage | Description | Status |
| --- | --- | --- |
| 1 | Repo skeleton, S8 port imported, builds reproducibly | **in progress (built)** |
| 2 | Loader runs on the A8 and prints the banner | **reached (screen output, 2026-11-08)** |
| 3 | Payload passes its check; jump to XNU | **reached (loader prints through `JUMP TO XNU`)** |
| 4 | XNU prints to the framebuffer (log or panic) | **reached (screen output, 2026-11-08)** |
| 5 | Storage (eMMC) detected | not started |
| 6 | Root filesystem mounts | not started |
| 7 | launchd, then SpringBoard | not started |

### Stage 2 evidence

- Image `/home/not_andrej/Downloads/boot-jackpotlte-shim.img`, 933,904 B,
  sha256 `a651f02badf6cf24f68bc22077aaf396a352c26f05d43167e5fb38c8ff4ad627`,
  shim kernel 46,144 B, empty ramdisk, packed on `boot_a530f_q.img`.
- Owner saw **`A8LOADER BEGIN  (jackpotlte Exynos XNU loader)`** on the phone
  after `fastboot flash BOOT` + reboot. This is the S8 `shim/dreamlte` loader
  with only: FB base `0xec000000`, 1080x2220, `DECON_TRIG 0x14860070 = 0x1281`,
  log zone at `0xc0000000`, and the probe calls removed.

### Stage 3/4 evidence

- Image `/tmp/opencode/a8-xnu2.img`, 17,481,744 B, sha256
  `3470dd7762d89b7e861a58003427d170d2f084d0e5cfded09612685232ebdab6`,
  shim 48,480 B, payload 16,546,816 B, DT 882,688 B. Packed on
  `boot_a530f_q.img`, written to `BOOT` from fastbootd and rebooted.
- The kernel is the A5XELTE donor build with `-DJACKPOTLTE=1`
  (CI run `42457569470`, `kernel.development.a5xelte` 16,552,752 B,
  sha256 `7a1739a09ecc469fb586c2fa8646298a276026a7a8c9d1d7153f5f0c1c4dd909`).
  The `lse.txt` "1 instruction" is a false positive: `K256` is a data
  table, not code.
- The owner photographed the screen (2026-11-08 08:37). The loader banner
  prints the whole payload, then `JUMP TO XNU`, and XNU draws its own
  `Darwin Kernel Version 22.6.0: Mon Aug 14 05:06:07 UTC 2023; xnu-8796.141.3`.
  The text is upside down in the camera frame but is XNU's kprintf.
- XNU also reports `pe_arm_init_interrupts: args: 0xfffffff007b6e730`,
  `pe_arm_map_interrupt_controller: found interrupt-controller` and
  `gPicBase: 0xfffffff03a400000`, so the GIC-400 mapping works.
- The last line printed is our own marker
  **`A5 vm: vm_page_bootstrap -> pmap_startup`**, which sits immediately
  before the call to `pmap_startup()`. No later marker appears: the next one,
  `A5 vm: npages=...`, is printed only *after*
  `pmap_steal_freeable_memory()` returns. The kernel therefore stops inside
  `pmap_startup()` before that call returns — not at the branch and not in
  the framebuffer text path. `initialize_screen: No video
  - forcing serial mode` is expected: the A8 path draws through the fbcon
  serial node, not the PE screen node.
- `fastboot flash boot` failed (`remote: 'No such file or directory'`):
  the partition is named `BOOT` uppercase in this fastbootd. The earlier
  "stuck at JUMP TO XNU" photo was the previous image, not this one.

### Flashed 2026-11-08 09:32 UTC (USERDATA root + trace kernel)

- **USERDATA**: `IOSRoot.sparse.img`, 12,145,430,824 B, sha256
  `5a8129778da7ea8d739fd1946083e0679ba1a7ce341d7f858232785d7b80e4ed`.
  Structure checked by walking every chunk: 27,811/27,811 chunks,
  4,194,304/4,194,304 blocks, 0 errors; block 0 carries the HFSX volume
  header. Flashed with `fastboot flash USERDATA` as a **sparse** image:
  47/47 chunks, `Finished. Total time: 499.6s`, RC=0.
- **BOOT**: `/var/tmp/a8-xnu3.img`, 17,481,744 B, sha256
  `0936f33fa5edaafe97a27fd856c2b9ba7dfec44d52b2be1866eecd67ef5e298e`
  (shim 48,480 B, ramdisk 16,546,816 B, DT 882,688 B). Kernel from CI run
  `42466539728` (`kernel.development.a5xelte`, 16,552,752 B, sha256
  `2c591d90e3386c393c8cacf434edd0663c1497a1b537a7e6400b3888ac5a352d`)
  with the `0125` markers verified in the binary. Flashed to `BOOT`, OK,
  rebooted 09:32 UTC.
- The flash writes do **not** by themselves make stage 5 or 6: eMMC
  detection and the root mount still need evidence from the phone.

Why the image had to be sparse (recorded so nobody repeats it): fastboot
loads a raw image whole — with `IOSRoot.img` it sat at
`total-vm: 16785176 kB` and was OOM-killed three times on this 7.8 GB host,
taking the desktop session with it. `-S` does not change this. A sparse
image streams in ~256 MiB chunks with a few hundred MB of RSS; the 512 MiB
test image flashed fine under a 400 MB `ulimit -v`. The final flash ran
under a 1 GB cap.

Note: after the OOM reboots the ext4 dentry for `IOSRoot.img` became
unresolvable (`ls` shows it, `stat`/`open` return ENOENT) while the sparse
file stayed intact. The sparse image is therefore the surviving copy of
the root; `simg2img` restores the raw image when there is 16 GiB free
(the host has 3.2 GiB free now). The IPSW DMGs were removed earlier with
owner approval, so a rebuild would start from a fresh IPSW download.

### Next step for the stage-4 hang (patch `0125`, built and flashed)

The last line is printed by `vm_resident.c` immediately *before* it calls
`pmap_startup()`. Nothing after it appears, and the framebuffer console
scrolls (row wraps) rather than stopping at the bottom, so the kernel really
stops printing. `pmap_startup()` has three later markers of ours
(`span`, `npages`, `page 0 ppn`) and none appear. Patch
`0125-jackpotlte-vm-trace.patch` adds, all gated on
`ARM64_BOARD_CONFIG_A5XELTE`:

- `A5 vm: enter vs=… ve=…` at `pmap_startup()` entry (after the LP64 align);
- `A5 vm: span=… mem_sz=… npages=…` after the page-count arithmetic;
- `A5 steal: size=… align=… vs=…` at `pmap_steal_memory_internal()` entry
  (the only caller on this path is `pmap_steal_freeable_memory()`);
- `A5 steal: addr=… new_vs=…` once the VA is carved out of
  `virtual_space_start`;
- `A5 steal: map=… phys=…` once per 1024 mapped pages.

These bisect the window: the existing marker prints, the next one that
appears names the failing call. One candidate is patch `0124`'s
`#if defined(DREAMLTE)` layout workarounds (deterministic `physmap_slide = 0`,
`ROUND_TWIG`, and the `va_l1_end` framebuffer reservation), which
`-DJACKPOTLTE=1` turns **off** — the working A5XELTE-on-A8 image had them on
because `DREAMLTE` was defined there. That is a hypothesis, not a finding:
the trace is what decides.

### Diagnostic build `0126` (built, then fixed; not yet on the phone)

The flashed trace was still ambiguous: `A5 steal: map=` fires only once per
4 MiB **before** `pmap_enter`, so the hang is bounded to the single window
`[0x69800000, 0x69a18d10)` (~2 MiB, ~522 pages) but not to a page, and
nothing says whether `pmap_enter()` reached `pmap_expand()` (the L3-table
shortage the S8 worked around) or stalled inside its own body. Per the
"diagnostics before speculative fixes" rule, `0126-jackpotlte-pmap-diag.patch`
adds, all gated on `ARM64_BOARD_CONFIG_A5XELTE`, no behaviour change:

- `A5 pg: va=… pa=…` *before* and `A5 pg: ok …` *after* each `pmap_enter()`
  in the steal loop — the last `va` with no matching `ok` is the hanging page;
- `A5 steal: done addr=… end=…` after the steal loop returns;
- `A5 avm: …` budget values (`physmap_base`, `slide`, `static_memory_end`,
  `dynamic_memory_begin`, `mem_size`) and the L3 prebuild range/count;
- `A5 expand`, `A5 tt_alloc`, `A5 ptd: feed`, `A5 pv: feed`, `A5 pgalloc` at
  the L3/PV/PTD allocation sites.

The `A5 avm:` markers run in `arm_vm_init` *before* `serial_init()`, so they
are expected to be dropped; the pmap/steal markers run inside `pmap_startup`
and will print.

Provenance: bt2 was stale (predates `0125`), so `0126` was generated against
the exact tree of the last good run (`42466539728`, downloaded as
`xnu-buildtree` and verified to contain the `0125` markers);
`patch -p1 --dry-run` applies cleanly.

The first CI run of `0126` (`42469107466`) failed to compile on one line:
`pmap_data.c:43: fatal error: 'pexpert/pexpert/pexpert.h' file not found`.
The TU is compiled with `-I<obj>/EXPORT_HDRS/pexpert`, so the header is
reached as `<pexpert/pexpert.h>` (the spelling `vm_resident.c` already uses);
fixed in commit `29c6175`. Every other TU in `0126` compiled with zero
errors in that run.

Re-dispatch `42470560244` is **green**:

- kernel `kernel.development.a5xelte` = 16,552,752 B,
  sha256 `1ebf8835465feeb4bc418f233c729a1ae32d854fc12e251b026d226c8bed4aa2`;
- all nine marker families present in the binary (`A5 pg: va=`, `A5 pg: ok`,
  `A5 steal: done`, `A5 avm:`, `A5 expand:`, `A5 tt_alloc:`, `A5 ptd: feed`,
  `A5 pv: feed`, `A5 pgalloc:`), and `0125`'s `A5 steal: map=` correctly gone;
- BOOT image `/var/tmp/a8-xnu4.img` = 17,481,744 B,
  sha256 `fc0779607d4a70a09f228b3eae07cdd33d4727940b6b87a04956aa837a4da5e6`
  (shim 48,480 B, payload 16,546,816 B, DT 882,688 B).

**Flashed and the decisive trace is captured (2026-11-08).** `/var/tmp/a8-xnu4.img`
was flashed to `BOOT` (product `jackpotlte`, `is-userspace: yes`,
`fastboot flash BOOT` RC=0) and the phone rebooted. The screen tail was:

```
A5 pg: va=0xfffffff034fff000  ... A5 pg: ok ... kr=0   <- last page of a 2 MiB region
A5 pg: va=0xfffffff035000000                            <- first page of the next
A5 expand: v=0xfffffff035000000 lv=3
A5 tt_alloc: lv=3 opt=0
A5 pgalloc: size=4096 opt=0
<then nothing>
```

Every `kr=` on screen is 0, and there is no `A5 pg: ok` after `A5 pgalloc`.
So the hang is **not inside `pmap_enter`'s body**: at a 2 MiB boundary a new L3
table is needed, `pmap_expand` → `pmap_tt_allocate` → `pmap_pages_alloc_zeroed`
runs, and `pmap_pages_alloc_zeroed` never returns — it blocks in
`vm_page_grab()`/`VM_PAGE_WAIT()` because the VM free list does not exist yet
during `pmap_startup`. The `A5 avm:` budget markers are absent, as predicted
(`arm_vm_init` runs before `serial_init()`).

Digit caveat: the `pa=` values are blurry on the photo; they read as a clean
+1-per-page sequence. Trust the structure (2 MiB boundary, +0x1000 VA, +1 PA),
not the individual digits.

### Diagnostic build `0127` (built; not yet on the phone)

The trace localises the hang to `pmap_pages_alloc_zeroed`, but does not yet say
*why* the L3 table for the next 2 MiB region was not prebuilt. Two candidate
causes need opposite fixes: either the L3 prebuild range
`[dynamic_memory_begin, va_l1_end)` is too short and genuinely does not cover
the stealing VA (fix the range), or it ran over that VA but the L2 TTE never
landed where `pmap_enter` looks (physmap overlap / layout), or the prebuilt L1
table is not the one the kernel pmap uses.

`0127-jackpotlte-pmap-layout.patch` (on top of `0126`), all gated on
`ARM64_BOARD_CONFIG_A5XELTE`, no behaviour change, adds:

- file-scope `a5_diag_*` globals filled in `arm_vm_init` (`physmap_base/end`,
  `static_memory_end`, `dynamic_memory_begin`, `va_l1_end`, `mem_size`,
  `real_phys_size`, `physmap_slide`, `topOfKernelData`, video size) plus an
  L3-table counter in the dynamic prebuild loop;
- in `pmap_expand` at level 3 it prints the failing VA, the L1 index and raw L1
  TTE, the resolved L2 table PA, the L2 index and raw L2 TTE, all the
  `a5_diag_*` values, and `virtual_space_start/end`.

These print late from `pmap_expand` (after `serial_init`) because `arm_vm_init`'s
own `kprintf`s are dropped. Provenance: generated as a diff of the `0126`-applied
files to the `0127` edits; `patch -p1 --dry-run` applies cleanly on a fresh
`bt3 + 0126` tree and the applied result is byte-identical to the edited files.
Patch sha256 `2116627c92e840c8c626434080b338b67c1afd2082bc79ac39eceede7ae89ad6`.

### `0127` flashed and decoded (2026-11-08) — the range is the bug

The full image (`shim/jackpotlte` + `0127`) was flashed to `BOOT` and the screen
photographed. The newest lines were:

```
A5 pg: va=0xfffffff02d7ff000 pa=0x882e4b
A5 pg: ok va=0xfffffff02d7ff000 kr=0
A5 pg: va=0xfffffff02d800000 pa=0x882e4c
A5 expand: v=0xfffffff02d800000 lv=3
A5 de: v=0xfffffff02d800000 l1i=448 l1=0x200000008828be003 l2i=364 l2=0 tab=0x8828be000
A5 dg: pb=0xfffffff00af84000 pe=0xfffffff02bf84000 sme=0xfffffff02bf84000 dmb=0xfffffff02c00000
A5 dg2: vle=0xfffffff02d800000 ms=0x20000000 rp=0x20000000 sl=0x26c8000
A5 dg3: tk=0x8828bc000 vid=0 n=12 vstart=0xfffffff02da18d10 vend=0xffffffff...
A5 tt_alloc: lv=3 opt=0
A5 pgalloc: size=4096 opt=0
```

Every value lines up with the code in `osfmk/arm64/arm_vm_init.c`:

- `mem_segments = (mem_size + 0x0FFFFFFF) >> 28 = 2`, so
  `va_l1_end = dmb + ((2 + 2*10) << 20) = dmb + 22 MiB`, `+ round_page(Video)`
  (`vid=0` on this handoff), then 8 MiB rounded → **`dmb + 24 MiB` =
  `0xfffffff02d800000` = exactly the hanging VA (`vle`)**.
- `n = 12` L3 tables = the 24 MiB / 2 MiB the prebuild loop built.
- `l2 = 0` at `l2i=364`: the L2 entry for the hanging 2 MiB region is empty, so
  the prebuild never got there and `pmap_expand` must allocate an L3 table.
- `sl = 0x26c8000` non-zero and `pb` (physmap_base) not TWIG aligned
  (low bits `0x84000`): the `#if defined(DREAMLTE)` layout workarounds in
  `0124` are **off** for `-DJACKPOTLTE=1`.

The `pmap_steal_memory()` that blocks is the `vm_pages` array:
`npages = mem_sz / (PAGE_SIZE + sizeof(vm_page))`; for 512 MiB that is ~16 MiB,
and with the earlier bucket steals the boot-time VA reaches `vstart` ≈
`dmb + 26 MiB`, ~2 MiB past the prebuild's 24 MiB. So this is a **range
deficiency**, not an L2 landing / overlap problem. The `pa=` values increment by
one per page (they are PFNs; the trailing `000` is not printed) — trust the
structure, and note the photo OCR moved digits (e.g. `l1` prints `8828be003`).

### Fix `0128`: enable the S8 physmap layout fixes for jackpotlte

`0128-jackpotlte-pmap-layout-fix.patch` changes the three `0124` guards from
`#if defined(DREAMLTE)` to `#if defined(DREAMLTE) || defined(JACKPOTLTE)`, so
the A8 gets the same deterministic layout the S8 (hardware-verified) runs:

- `physmap_slide = 0` (was random, `sl=0x26c8000`);
- `physmap_base = ROUND_TWIG(physmap_base)` (was not 2 MiB aligned);
- `va_l1_end += round_page(A5_FB_STRIDE * A5_FB_HEIGHT) + (64ULL << 20)`
  (was `dmb + 24 MiB`, one 2 MiB region short of the boot-time steal VA).

Only `osfmk/arm64/arm_vm_init.c` changes; no new values are invented — the
`64 MiB` and the framebuffer term are the S8's, with the A8's framebuffer size
(`1080*4*2220`). Applies cleanly on a `bt3 + 0001..0127` tree and reproduces the
edited file byte-for-byte. Built only on CI (`MACHINE_CONFIG=A5XELTE
EXTRA_MACHINE_FLAGS=-DJACKPOTLTE=1`); a local XNU build is not possible.

### Display: DECON trigger ported into the jackpotlte shim (2026-11-08)

Stage 2/4 were reached with the **S8 `shim/dreamlte`** loader, which is what
actually poked DECON. The jackpotlte shim carried `JT_DECON_TRIG 0` ("do not
poke DECON before XNU"), so an image built from it would clear the handover
framebuffer but never make DECON scan it out — serial only. The trigger is now
in `shim/jackpotlte` itself.

- `shim/jackpotlte/board.h`: `JT_DECON_TRIG` = `0x14860070`
  (`JT_DECON_BASE + 0x0070`), plus `JT_DECON_TRIG_UNMASK 0x1281`.
- `0x1281` is the 7885 definition, not a copy of an 8895 magic number:
  `drivers/video/fbdev/exynos/dpu_7885/regs-decon.h` defines
  `HW_TRIG_EN (1 << 0)`, `HW_TRIG_EDGE_POLARITY (1 << 7)`,
  `TRIG_AUTO_MASK_EN (1 << 12)` and warns `SW_TRIG_EN (1 << 8)`. `0x1281` sets
  the three HW-trigger bits and **not** `SW_TRIG_EN`, so it re-arms the TE
  (hardware) trigger; the display keeps scanning the handover buffer on every
  TE instead of holding the S-Boot frame. This matches the board note and the
  verified S8 value.
- `shim/jackpotlte/loader.c` `jt_main`: with `JT_HAVE_FB`, always write the
  trigger (when non-zero), then clear the framebuffer and set the foreground
  colour. The old `JT_DECON_TRIG == 0` clear-only branch is gone.

Local build (not a device claim):

- `shim/jackpotlte/out/Image` = **51,984 B**, 0 ABS64 relocations, `mov
  w1,#0x1281` / `str w1,[x0]` with `x0=0x14860070` confirmed by objdump.
- Probe image `/var/tmp/a8-probe-jt.img` = **940,048 B**,
  sha256 `2f6424a43c8bbf37affa007126f205ed430a80eb41b8b47c5a45d5ba0837b5be`
  (kernel 51,984 B, empty ramdisk 43 B, DT 882,688 B) on `boot_a530f_q.img`.
  Expected on the phone: the `JACKPOTLTE LOADER BEGIN` banner on the A8 panel,
  then the DT probe, then the 8 s canary reset to recovery.
- Full image `/var/tmp/a8-xnu-jt.img` = **17,485,840 B**,
  sha256 `69755a6fc096eedd12ac540b478fa38eeb7440e1d844e17afdcc91f8344cc86e`
  (jackpotlte shim 51,984 B, payload 16,547,216 B, DT 882,688 B). Kernel is the
  `0127` build from run `42472946452` (`kernel.development.a5xelte` 16,553,152 B,
  sha256 `22c8d8cd3f75a39c9b88425a3e4b1b4bb3950b0249aef830a3b7cab5781884a6`),
  with `A5 de:`, `A5 dg:`, `A5 dg2:`, `A5 dg3:` verified present. Built with
  `/var/tmp/xnu-build/mkimg-jt.sh` (jackpotlte shim + `tools/jackpotlte_mkboot.py`;
  it still appends `samsung,a5xelte` to the ADT root compatible because the
  kernel's `IONameMatch` is the donor board name).

Flashed and seen (2026-11-08): both `/var/tmp/a8-probe-jt.img` and the full
`/var/tmp/a8-xnu-jt.img` were written to `BOOT` (`fastboot flash BOOT`,
product `jackpotlte`, `is-userspace: yes`, RC=0). The owner confirmed the A8
panel showed the `JACKPOTLTE LOADER BEGIN` banner and the device-tree dump —
stage 2 re-confirmed with the device's own shim. The full image then reached
XNU and the `0127` layout probe (see the `0127` decode above).

### iOS root image (host build, not a device result)

- Built with `tools/mkiosroot.sh OS.dmg SYSTEMOS.dmg APPOS.dmg
  /var/tmp/iosroot/iOSRoot.img 16G` from the iPhone10,3 16.7.16 (20H392)
  DMGs `087-86698-044` / `087-86593-047` / `087-86715-047`.
- The image is 17,179,869,184 B, 12 GB used, case-sensitive journaled HFS+,
  label `iOSRoot`. Mounted read-only on the host and spot-checked:
  `/sbin/launchd` (541,600 B, root:wheel 0755), `/usr/lib/dyld` (922,912 B),
  `/private/preboot/Cryptexes/{OS,App}`, the `System/Library/Caches/com.apple.dyld`
  shared-cache hard links (link count 2), `System/Cryptexes/{OS,App}` symlinks,
  and the root-only `/private/var/keybags/systembag.kb`.
- The IPSW DMGs are LZBITMAP-compressed, so the distro `apfs-fuse` 0.33
  returns `EIO` on every compressed file; the image was built with upstream
  master `apfs-fuse` built from source at `/tmp/opencode/apfs-fuse/build`
  (`-DCMAKE_CXX_FLAGS="-include cstdint"`, needs `fuse3-devel`).
- `mkiosroot.sh`'s final check is fixed: Linux `fsck.hfsplus -n` reports
  "Volume header needs minor repair" and exits 8 on **any** Linux-made
  journaled volume (a fresh, never-mounted `mkfs.hfsplus -s -J` image does
  the same), so it cannot be used as a pass/fail check. The script now
  verifies with a second repair pass and requires "appears to be OK".
- Not flashed: the iOS root goes to USERDATA only after approval. This is a
  host build result, not stage 6.


## Hardware facts (from Samsung's own 4.4 source and the mainline DT; NOT yet confirmed on the phone)

Source now includes the **Eureka-Kernel** Samsung 4.4 tree for this device
(`eurekadevelopment/Eureka-Kernel`, tag "Release R24U sources",
`arch/arm64/boot/dts/exynos/dtb/exynos7885*.dts[i]`, `drivers/`). See
`shim/jackpotlte/board.h` for the full list with a citation per value.

- RAM: low `0x80000000 + 0x3da00000`, mid `0xc0000000 + 0x40000000`, high
  `0x880000000 + 0x40000000`. S-Boot's own `/chosen` sets
  `linux,initrd = 0x84000000..0x840fffff`.
- CPU: 6x Cortex-A53 (MPIDR `0x100..0x103`, `0x200..0x201`), 2x Cortex-A73
  (`0x0..0x1`). Boot core expected at MPIDR `0x100` (unconfirmed).
- GIC-400: GICD `0x12301000`, GICC `0x12302000`, GICH `0x12304000`,
  GICV `0x12306000`.
- Timer: arch timer + 26 MHz; non-secure phys timer PPI 14 (INTID 30).
- PMU: `0x11c80000`. Secondary-core power: `PMU_CPU_CONFIG_BASE 0x2000`,
  status `0x2004`, step `0x80`, `CPU_LOCAL_PWR_CFG 0xF`; clusters 0/1/2 map to
  core slots 0/4/6 (Exynos7885-specific numbering in `exynos-pmu.c`).
- Storage: **eMMC** `samsung,exynos-dw-mshc` `dwmmc0@13500000` (+ cmdq
  `0x13508000`); the S8 uses UFS, so storage adapts from the A5XELTE eMMC
  driver, not the S8's UFS.
- Display: bootloader framebuffer **`0xec000000`**, 20 MiB
  (`fb_handover`/`exynos,dpu_fb_handover`). Panel **s6e3fa7, 1080x2220**, MIPI
  command mode. **DECON_F @ `0x14860000`**, `HW_SW_TRIG_CONTROL` at `+0x70`.
  **Confirmed on the phone (2026-11-08):** the loader must write `0x1281` to
  `0x14860070` before touching the framebuffer. Without it DECON never latches
  a shadow update, no frames are scanned, and the panel keeps showing the
  S-Boot logo out of its own GRAM (the "stuck on Samsung Galaxy" symptom).
  The loader's earlier shadow-update pokes (`+0x60`) are what broke scanout;
  the `+0x70` trigger is the one that works.
- Buttons: `gpa1` (pinctrl alive `0x11cb0000`) — vol-down p6, vol-up p5,
  power p7 (active low). Bank offset still to read from the driver.

## Unknowns (do NOT guess; read from the named source)

| Item | Read from |
| --- | --- |
| ramoops/pstore zone (exact offset of the "log_pstore" item) | Eureka `drivers/trace/exynos-ss.c` ESS layout, or live DT / TWRP `/proc/iomem` |
| gpa1 bank offset inside pinctrl_0 | Eureka pinctrl bank table, or live DT |
| eMMC CIU timing / card setup | Eureka `samsung,exynos-dw-mshc` driver |
| Stock boot.img header (kaddr/raddr/tags/os_version, DTBH) | the SM-A530F's own BOOT dump |
| EL at handoff, x0 register, load addresses | loader banner + a probe image on the phone |

## Build results (local, not device)

- `shim/jackpotlte/build.sh` → `out/Image` = **51,984 bytes** with the DECON
  trigger (48,384 B before it), 0 ABS64 relocations, above the 16 KiB stub
  gate. Reproducible on this machine.
- `shim/jackpotlte/build.sh --probe STOCK OUT` → shim-only flash image
  (51,984-byte loader + a minimal newc cpio.gz ramdisk). This is the stage-2
  bring-up/display image; see the DECON section for the current artifact.
- `tools/mkadt_jackpotlte.py` → ADT = 8,920 bytes (without the iPhone X
  `/product` node, which is an Apple artifact kept out of git).
- `tools/jackpotlte/selftest.py` → the full pack chain (loader → ADT → payload
  → Samsung pack → bootimg_info) passes on a synthetic Mach-O; the packed
  header keeps the stock load addresses and the SEANDROIDENFORCE trailer.
  This proves the pipeline is internally consistent, not that a kernel boots.
- Packer round-trip tested with a synthetic stock image only, not a real
  SM-A530F BOOT dump. Real header values still required (see above).

## CI status

- `loader` (Linux): build + gates + ADT + pipeline self-test + repo validation.
  The ABS64 gate caught a real defect the local GCC hid: a `const char *names[]`
  string-literal table in `dt_probe` became 12 absolute relocations under CI's
  GCC (locally it folded to PC-relative). Replaced with a `node_line()` helper;
  run 42413636860 is green.
- `xnu` (macOS 15): builds `MACHINE_CONFIG=A5XELTE` with the A5 patch set +
  `0124-dreamlte-s8`, newest installed Xcode (patch `0083` supplies the
  `IORPC.kernelContent` field that Xcode 16+ `iig` emits), and now fails on a
  stub kernel. First runs: macos-14 was rejected by the runner; fixed to
  macos-15. Run 42412221459 (success) produced `out/kernel.development.a5xelte`
  = 16,553,272 bytes, retrievable as the `xnu-kernel` artifact (66 MB, not
  expired) and kept as the `xnu-buildtree` artifact.

## Device confirmation

The owner confirmed the device is **SM-A530F (Exynos 7885)**, not the
Snapdragon A530W/U. The mainline `exynos7885*` values above therefore apply.

## Next concrete action

1. Get `backup/BOOT.img` (the SM-A530F's own BOOT dump) from the phone:
   `adb shell "dd if=/dev/block/by-name/BOOT of=/sdcard/BOOT.img"` then
   `adb pull /sdcard/BOOT.img backup/BOOT.img`.
2. Build and flash the shim-only probe image and photograph the screen:
   `bash shim/jackpotlte/build.sh --probe backup/BOOT.img a8-probe.img`.
   That reaches stage 2 and returns the live bootargs, initrd range, /memory
   and the 7885 node presence — record them in `docs/jackpotlte-bringup.md`.
3. Read the remaining unknowns (ramoops `log_pstore` offset, gpa1 bank offset,
   eMMC timings) from the live DT / Samsung source and fill `board.h`; then
   enable `JT_HAVE_RAMOOPS`.
