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
| 5 | Storage (eMMC) detected | **reached (screen output, 2026-11-08)** — `0127` (was `0130`) maps `dwmmc0@13500000` and reads the GPT (`SYSTEM` LBA 409600, 11,078,400 sectors) |
| 6 | Root filesystem mounts | **reached (screen output, 2026-11-08)** — `rd=emmcroot0 emmcroot=USERDATA emmcwrite=1` mounts the HFS root and execs launchd |
| 7 | launchd, then SpringBoard | **in progress** — jt7 cleared `WRONG_PLATFORM`; launchd runs, now `launchd_cache_loader` aborts `Unable to find AMFI` → fix `launchd_unsecure_cache=1` (jt8) |

### Patch renumbering (2026-11-08)

The A5 VM/pmap diagnostic patches were **removed**: their per-call `kprintf`s
(`A5 expand`, `A5 de`/`dg`/`dg2`/`dg3`, `A5 tt_alloc`, `A5 pgalloc`, `A5 pg`,
`A5 steal`, `A5 vm`, `A5 pv`, `A5 ptd`) ran on every pmap expand / page-table
allocation, so the framebuffer console only ever showed the spam and the real
boot tail (launchd / SpringBoard) could not be seen. They were scaffolding for
the stage-4 pmap hang, which the physmap fix resolved; they are not needed at
runtime and printed nothing a normal boot wants.

- **Removed** (diagnostic-only, no functional content):
  old `0125-jackpotlte-vm-trace`, `0126-jackpotlte-pmap-diag`,
  `0127-jackpotlte-pmap-layout`.
- **Renumbered down:** old `0128-jackpotlte-pmap-layout-fix` → `0125`,
  old `0129-jackpotlte-cpu-topology-mpidr` → `0126`,
  old `0130-jackpotlte-emmc-dwmmc0-base` → `0127`.

Every reference below dated **before** this note uses the **old** numbers.

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

### `0128` flashed and decoded (2026-11-08) — pmap hang is gone; new CPU-topology panic

CI run `42475507160` (`-DJACKPOTLTE=1`) built commit `8ff471f`. Artifact
`kernel.development.a5xelte` 16,553,152 B, sha256
`d95abb2e331f1dbf3d915215f9c61ba5536fef4c80b8bf0d54dd33187a4b7fe3`; the run's
compile line for `arm_vm_init.c` carries `-DARM64_BOARD_CONFIG_A5XELTE
-DJACKPOTLTE=1` and no `-DDREAMLTE`, so the three `0128` guards are active.
Image `/var/tmp/a8-xnu-jt2.img` 17,485,840 B, sha256
`6ee89ba1ad1810f79e6a64cadbc4ef4ea95987af528f2603b5ba27429824e6cb`, flashed to
`BOOT` (`fastboot flash BOOT`, RC=0).

Photo `IMG_20261108_123601918.jpg` shows the fix worked. Decoded:

- `sl=0` (physmap_slide now 0) and `n=51` L3 tables (was 12): both `0128`
  effects visible.
- The boot ran far past the old hang: `calling mpo_policy_init for vnguard`
  → `Security policy loaded; Guarded vnode policy (vnguard)` → `SWEET: trust
  caches unavailable` → `keepsyms boot arg specified` → `No POST boot-arg set.`
  → `A5XELTE: starting device-tree platform expert` → `A5XELTE: platform expert
  registered` → `SWEET RKS/WDT/SURF/HID ... registered` → `SWEET BATTERY:
  static fallback, 100% 4200mV`.

The new stop is a panic:

```
panic(cpu 0 caller 0xfffffff007a16b74): A5XELTE CPU: unsupported topology
  cpu=0 mpidr=0x100 count=1 @SweetCPU.cpp:102
```

`count=1` is the `cpus=1` boot arg. `SweetCPU::start()` computes
`expected_mpidr` for the non-`DREAMLTE` (Exynos7580) path as `cpu->cpu_id`
(cpu 0 → MPIDR 0), but the ADT `cpu0` node reports `reg = 0x100`, a mismatch
against the ADT/hardware truth (below). `ml_parse_cpu_topology()` numbers
`cpu_id` by enumeration and takes `phys_id` from the node `reg`; the `0128`
tree's `startAll()` already compares `topology->boot_cpu->phys_id` to
`A5_BOOT_MPIDR` (0x100 for jackpotlte), so only the `start()` check was stale.

### Fix `0129`: jackpotlte CPU topology MPIDR map

Hardware truth (Samsung's own `arch/arm64/boot/dts/exynos/dtb/exynos7885.dtsi`
in Eureka-Kernel): `cpu0-3` = A53 `0x100-0x103`, `cpu4-5` = A53 `0x200-0x201`,
`cpu6-7` = A73 `0x0-0x1`. `tools/mkadt_jackpotlte.py` emits the ADT nodes in
exactly this order, so the logical `cpu_id` indexes the same table.

`0129-jackpotlte-cpu-topology-mpidr.patch` changes only
`iokit/Kernel/arm/SweetCPU.cpp`: it turns the `#if defined(DREAMLTE) / #else`
pair in `SweetCPU::start()` into `DREAMLTE / JACKPOTLTE / else`, adding a
jackpotlte branch that maps `cpu_id` to the MPIDRs above. The Exynos7580
default is left unchanged for the A5. No new hardware value is invented — the
MPIDRs are the ones already in `tools/mkadt_jackpotlte.py`, sourced from the
Samsung DT. sha256 `ce6c21869133a3707e0101f07d5f034300c43118910692e82f7fcd1d2397a218`;
applies cleanly on the `0128`-applied tree and reproduces the edited file.

Note (not yet changed): `MAX_CPU_CLUSTERS` is 2, and the three 7885 clusters
(A53 ×2, A73 ×1) would need 3 if `cpus` is raised above 1. With `cpus=1` only
cluster 0 is enumerated, so this is not on the current path.

### `0129` flashed and decoded (2026-11-08) — XNU now boots to the root-mount wait

CI run `42476875488` (`-DJACKPOTLTE=1`) built commit `cb2b7c0`. Kernel
`kernel.development.a5xelte` 16,553,152 B, sha256
`5ab08dfaf313cf7d449e68748306a8f438b15fb16749fe8cbc0610c1f2488b41`; the
`xnu-buildtree` artifact shows the jackpotlte branch present in
`iokit/Kernel/arm/SweetCPU.cpp`. Image `/var/tmp/a8-xnu-jt3.img` 17,485,840 B,
sha256 `36e75126b1464a4098f64f1b969e24dd1ab0e8236295c12102629ef9c307a189`,
flashed to `BOOT` (RC=0).

Photos `IMG_20261108_125108590.jpg` and `IMG_20261108_125126851.jpg` show the
CPU-topology panic is gone and the boot now reaches BSD and the root wait:

- `A5XELTE CPU: requested=1 selected=1 (limit=8)`
- `A5XELTE CPU: cpu=0 entered initCPU mpidr=0x100 EL1 IRQs=1`
- `A5XELTE CPU: cpu=0 mpidr=0x100 came alive; SGI1 OK`
- `MAC Framework successfully initialized`; `using 2411 buffer headers`
- `A5XELTE CPU: 1/1 CPUs initialized, SGI1 verified`
- `SWEET AKS/WDT/SURF/HID ... registered`; `SWEET BATTERY: static fallback`
- `SK[0]: skmem_region_init`, `Skywalk: running self-tests` → every case
  `PASSED`
- `bsd_autoconf: calling kminit`; `ifnet_attach: ... lo0 / gif0 / stf0 ...
  Proceeding.`
- `A5XELTE KEYS: GPA2 con=0x1111 dat=0xf pud=0x3300 (read only)`

The last line is the stop:

```
Waiting on <dict ID="0"><key>IOProviderClass</key><string ID="1">IOMedia
</string><key>Content</key><string ID="2">Apple_HFS</string></dict>
```

That is the root-mount wait: XNU is up (single core), VM / security / MAC /
BSD / Skywalk / networking have all initialized, and the boot is now blocking
on a block device that publishes an HFS volume. No storage driver provides one
yet, so **stage 5 (storage) is now the blocker** (previously stage 4).

### Storage `0130`: eMMC ADT node + 7885 dwmmc0 base

Two gaps stopped `bsd/dev/a5xelte_emmc.c` from ever mapping the host:

1. `tools/mkadt_jackpotlte.py` now adds `/a5xelte-emmc`
   `reg = <0x13500000 0x2000>`. Source: Samsung `exynos7885.dtsi`, where the
   embedded eMMC is `dwmmc0@13500000` (dwmmc1 `0x13510000` = SDIO, dwmmc2
   `0x13550000` = SD card). Verified by dumping the built ADT: `reg[0] =
   0x13500000`, `reg[1] = 0x2000`.
2. `0130-jackpotlte-emmc-dwmmc0-base.patch` makes `emmc_map_host()` accept the
   board's base: `0x13500000` under `JACKPOTLTE`, `0x13540000` (the A5/
   Exynos7580 value from `tools/mkadt_a5xelte.py`) otherwise. Only
   `bsd/dev/a5xelte_emmc.c` changes.

sha256 `01d812786f32579bdfdf6a57eb273df3758ed93f2356e4b571d5ee40d15696fd`;
applies cleanly on the `0128`-applied tree (the file also has `0108`/`0110`/
`0118` on top of `0104`) and reproduces the edited file. The probe run adds
`emmcprobe=1` to the cmdline; `rd=emmcroot0` selects the eMMC root.

### `0130` flashed and decoded (2026-11-08) — eMMC host maps, GPT read (stage 5)

CI run `42478756712` (`-DJACKPOTLTE=1`, commit `fbf89d4`) built the kernel,
sha256 `71c3eea3eb3244c971d535e2152437fa03881c68ad6de847b50b25b22b3e0caa`,
16,553,152 B (matches the artifact's `SHA256SUMS`). Image
`/var/tmp/a8-xnu-jt4.img` 17,485,840 B, sha256
`01faaf7592e3205a99de4d9b9f057317590e69c3080f2e4c1312d329a5a6e54d`, flashed to
`BOOT` (`fastboot flash BOOT`, RC=0), then booted.

Photo `IMG_20261108_132650156.jpg` shows the probe pass (read directly):

```
A5XELTE KEYS: GPA2 con=0x1111 dat=0xf pud=0x3300 (read only)
A5XELTE eMMC: host ctrl=10 status=0896 clk=1 hcon=0x42541
A5XELTE eMMC: GPT SYSTEM first=409600 sectors=11078400 read-only
Waiting on <dict ID="0">... IOProviderClass=IOMedia ... Content=Apple_HFS ...</dict>
```

Reading: the driver mapped the 7885 `dwmmc0` window at `0x13500000`
(`host` non-zero, `ctrl=0x10` = FIFO reset cleared, `clk=1` = CLKENA set,
`hcon=0x42541` = 8-bit bus, IDMAC capable), then read and CRC-checked the GPT
and found `SYSTEM` at **LBA 409600, 11,078,400 sectors** (5,672,140,800 B).
`read-only` is by design: `a5xelte_emmc.c` only enables writes for `USERDATA`
with `emmcwrite=1`; the default/`SYSTEM` path is read-only. So **stage 5 is
reached** — the eMMC is detected and its partition table read on the phone.
The boot still waits on `IOMedia`/`Apple_HFS` because `emmcprobe=1` only
probes; it does not publish a root device.

Next: the iOS root **is already on `USERDATA`** — `IOSRoot.sparse.img` was
flashed on 2026-11-08 09:32 UTC (47/47 chunks, RC=0; see the top section). So
stage 6 is only the mount: boot `rd=emmcroot0 emmcroot=USERDATA` (and
`emmcwrite=1` for a writable root) with the `0130` kernel.

### Stage 6 reached, launchd killed as WRONG_PLATFORM (2026-11-08)

Image `/var/tmp/a8-xnu-jt6.img` (sha256
`18a27868887c284384ddcb9504d046f80dcaaed0feba8d7f6c0c9f848ede2805`, same
`0130` kernel) was flashed to `BOOT` with cmdline
`rd=emmcroot0 emmcroot=USERDATA emmcwrite=1` and rebooted.

The `USERDATA` root mounted and `/sbin/launchd` executed — so **stage 6 is
reached**. The phone then panicked
(`IMG_20261108_134026153.jpg`, read directly):

```
panic(cpu 0 caller 0xfffffff007acde5c): unexpected SIGKILL of launchd
(CS_KILLED) with reason -- namespace 9 code 0xe description none
handshake structure not initialized
```

Decode (from the tree, `/var/tmp/art0128/bt/xnu`):

- `namespace 9` = `OS_REASON_EXEC` (`bsd/sys/reason.h:96`),
  `code 0xe` = 14 = `EXEC_EXIT_REASON_WRONG_PLATFORM` (`reason.h:257`).
- The panic is the `psignal` guard for `initproc` (`bsd/kern/kern_sig.c:2064`).
- The trigger is `bsd/kern/kern_exec.c:7390`: this kernel is built
  `XNU_TARGET_OS_OSX`, so when the parent is `PLATFORM_IOS` the spawn must
  either carry `com.apple.private.security.no-sandbox` or the debug tunable
  `unentitled_ios_sim_launch` must be set, else it kills the child
  `EXEC_EXIT_REASON_WRONG_PLATFORM`. `unentitled_ios_sim_launch` is declared
  at `kern_exec.c:196`.
- `handshake structure not initialized` is a consequence
  (`osfmk/kdp/output_stages/out_shmem.c:499`, the panic coredump stage), not
  the cause.

The working S8/A5 cmdlines already carry the fix: `docs/upstream-README.md:115`
(S8) and the A5 boot args both use `unentitled_ios_sim_launch=1`, and the A5
adds `cs_enforcement_disable=1` (`kern_cs.c:167`). The A8 cmdline was
missing both. Corrected image `/var/tmp/a8-xnu-jt7.img` (sha256
`81dee8c317af755821bb4339c02109988f00807decf2848569381b6a5aed8d51`) adds
`unentitled_ios_sim_launch=1 cs_enforcement_disable=1`; `emmcwrite=1` keeps the
root writable so launchd's mount phase can write.

### `jt7` clears WRONG_PLATFORM; cache loader aborts on AMFI (2026-11-08)

Image `/var/tmp/a8-xnu-jt7.img` (sha256
`81dee8c317af755821bb4339c02109988f00807decf2848569381b6a5aed8d51`) added
`unentitled_ios_sim_launch=1 cs_enforcement_disable=1`. Photo
`IMG_20261108_134838366.jpg` (read directly) shows launchd running well past
the previous kill:

```
0 Found valid port: 2563 Valid: 1
Using default cache paths
Code: /System/Library/xpc/launchd.plist Sig: /System/Library/xpc/launchd.plist.sig
abort: Unable to find AMFI
pid 23 exited -- exit reason namespace 7 subcode 0x1, description Unable to find AMFI
panic(cpu 0 caller 0xfffffff007acb1bc): launchd_cache_loader[23] exited -- ... Unable to find AMFI
```

So `jt7` fixed the `EXEC_EXIT_REASON_WRONG_PLATFORM` kill. New blocker:
libignition's `launchd_cache_loader` aborts because it cannot find AMFI
(namespace 7 = `OS_REASON_LIBXPC`; the description is the userspace abort
string, present in `IOSRoot.sparse.img`). `docs/upstream-README.md:125`
documents the fix: `launchd_unsecure_cache=1` ("launchd accepts its job cache
without AMFI"). `jt8` (sha256
`81735341ac3eb7f4a4c5a63ed8a501e5f7d5f8792062aa502d8cff4f4d7aec72`) adds
`launchd_unsecure_cache=1 ignition_level=1`; boot args reach userspace through
the `kern.bootargs` sysctl (`bsd/kern/kern_sysctl.c:2177`).

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

### Display: panel/DSIM bring-up written (2026-11-08, NOT yet on the phone)

Merged onto main as `xnu/patches/0133-jackpotlte-panel-dsim-bringup.patch`
(it was `0131` on the `a8-panel` branch). It adds
`bsd/dev/a5xelte_panel.c` (registered in `bsd/conf/files`, called from
`IOKitBSDInit.cpp` next to the other `a5xelte_*` entry points). It is the
first patch that touches the A8's own display IP:

- Default is **log-only** (`a5panelprobe`, on by default, read only): it prints
  the DSIM `SWRST`/`CLK_CTRL`/`CONFIG`/`RESOL`/`PLLCTRL`/`LINK_STATUS3`, the
  DECON trigger word, and the `gpg1`/`gpg2` CON/DAT state. It writes nothing,
  so it cannot regress the working DECON handover.
- Active bring-up is opt-in (`a5panel=1`, default off) and mirrors the vendor
  driver: it re-reads `DSIM_LINK_STATUS3.PLL_STABLE` first and, if S-Boot
  already brought the link up, skips all link/panel init (`dsim_drv.c:826-832`,
  `dsim_reg.c:1355-1360`). Only when the PLL is *not* stable does it reset the
  D-PHY, program the PLL, D-PHY timings and command-mode config, pulse `gpg1-7`
  (`lcd_rst`) and send the s6e3fa7 init sequence.
- No PMIC/LDO/regulator write exists in the patch. The panel rails (`gpg2-7`
  ldo_3p0, `gpg2-6` vddr_en) are read and printed only; DECON is read-only.

`tools/mkadt_jackpotlte.py` now publishes a `disp0` node under `arm-io`
(`device_type display-subsystem`, `compatible samsung,a5xelte-display`,
`canvas 1080x2220`) so the A5XELTE `AppleCLCD` personality
(`KernelConfigTables.cpp`, patches 0109/0124) binds to the handover
framebuffer at `0xec000000`.

Not yet claimed: nothing of this has been flashed. On the merged tree it
applies as `0133` after `0001-0132`. The
`a5panel=1` path is untested and should be tried only after the log-only boot
confirms S-Boot's DSIM/rail state.

### Port merge + the `-DJACKPOTLTE=1` root cause (2026-11-08)

The five component ports were merged onto `main` (each branch was based on the
pre-renumber tree, so their patches were renumbered):

| New patch | Port | From branch (old number) |
| --- | --- | --- |
| `0128` | USB dwc3 `@0x13600000` read-only probe | `a8-usb` (0131) |
| `0129` | PMIC fuel gauge / charger read-only probe | `a8-pmic` (0131) |
| `0130` | sec_ts I2C4 id probe + `tsp_ldo_en` power log | `a8-touch` (0131) |
| `0131` | sec_ts read-only contact path (`a8touchlog`) | `a8-touch` (0132) |
| `0132` | gpa1 buttons poll | `a8-keys` (0131) |
| `0133` | DSIM/panel bring-up, log-only by default | `a8-panel` (0131) |

**The panic in photo `IMG_20261108_152844733`** (`Kernel data abort`, pc
`0xfffffff00747a070`, ESR `0x96000010`) decodes to
`dreamlte_battery_read+0x60`: the S8 donor function maps the Galaxy S8 MAX77865
fuel gauge at physical `0x10970000` (`0124-dreamlte-s8.patch:337`) and reads it.
That address is not backed on the A8, so the access external-aborts.

Why the S8 code ran at all: `pexpert/pexpert/arm64/A5XELTE.h` (patch `0124`)
picks the board variant from the **`JACKPOTLTE`** macro:

```c
#if defined(JACKPOTLTE)
    ... A8 board facts ...
#else
    #define DREAMLTE 1
    ... S8 board facts ...
#endif
```

`0124` also defines a `JACKPOTLTE` stub for `dreamlte_battery_read` that returns
`-1` without touching MMIO (`0124-dreamlte-s8.patch:409-421`). But the CI A5XELTE
job never passed `-DJACKPOTLTE=1`, so the build defined `DREAMLTE` and compiled
the S8 paths — and every `#if defined(JACKPOTLTE)` block (the battery stub and
all five new ports) was **dead code**. `xnu.yml` now sets
`EXTRA_MACHINE_FLAGS: -DJACKPOTLTE=1` for push builds (a manual dispatch can
still override it). This is the fix for the panic, not a kernel patch.

Verified on a pristine `xnu-8796.141.3` tree: run the three stage scripts
(`0019` libpthread, `0020` BearSSL, `0026` HFS), then `git apply` `0001-0133` in
order → **0 failures**. `tools/validate-repo.py` passes (133 patches, no gaps).

First-flag build failure and fix: with `-DJACKPOTLTE=1` the ports compile for
the first time, and `jackpotlte_pmic.c` called `delay()` without a declaring
header (CI `42493189603`: `bsd/dev/jackpotlte_pmic.c:118: error: call to
undeclared function 'delay'`). Fixed by adding `#include <kern/clock.h>` to
`0129` (the same header `0133` uses); the patch still applies cleanly after
`0128`.

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
- **Flashed 2026-11-08 09:32 UTC** (see the top "Flashed" section):
  `IOSRoot.sparse.img`, 47/47 chunks, RC=0. The *mount* result is still stage 6.


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

1. **Stage 5 is reached** (`0127`, was `0130`): the eMMC host maps and the GPT
   reads on the phone (`SYSTEM` LBA 409600, 11,078,400 sectors).
2. **Stage 6 is reached** (`jt6`): the `USERDATA` HFS root mounts and
   `/sbin/launchd` executes.
3. **Stage 7 (launchd):** the A5 VM/pmap diagnostic patches were removed and the
   rest renumbered, so the console no longer drowns in
   `A5 expand`/`pgalloc`/`steal` lines. CI `42486906977` built that clean kernel
   (`kernel.development.a5xelte`, 16,553,272 B, sha256
   `a0b451285c3aa34298ddfbbf5eaeceb92104ab661674070444fc7614b9764756`, **zero**
   `A5 expand|pgalloc|tt_alloc|steal|pg|dg|de` strings) and
   `/var/tmp/a8-xnu-jt9.img` (sha256
   `267262398cc113b22504aab0fa23b604ccb3b0bb9153bf7ede82e8bdc5f0d59b`) packed it
   with the multi-bank shim and the `jt8` cmdline. **But jt9 was built without
   `-DJACKPOTLTE=1`, so it is the image that panicked** in
   `dreamlte_battery_read` on physical `0x10970000` (photo
   `IMG_20261108_152844733`; see "Port merge + the `-DJACKPOTLTE=1` root cause").
   The fix is the workflow flag, not a kernel patch. Next: when CI on the merged
   `main` (with the flag) is green, download the `xnu-kernel` artifact and rebuild
   the image (`/var/tmp/xnu-build/mkimg-jt-root3.sh`) as the real stage-7 trial —
   launchd / `launchd_unsecure_cache=1` / AMFI, now with the five ports live.
4. **DRAM map (for "all 4 GB"):** `shim/fdt.c` gained `fdt_memory_all()`, which
   enumerates every `/memory` node; `dt_probe()` now prints each bank
   (`mem bank N base=.. size=.. (MiB)`) plus the total. Host-verified against a
   synthetic 3-bank DTB. On the phone this line reports the real banks in one
   flash. Source banks (Eureka `exynos7885-jackpotlte_common.dtsi:28-41`): low
   `0x80000000+0x3DA00000` (986 MiB), mid `0xC0000000+0x40000000` (1024 MiB),
   high `0x880000000+0x40000000` (1024 MiB) ≈ 3034 MiB. XNU takes one
   contiguous window (`arm_init.c` → `gPhysBase`/`gPhysSize`; ADT
   `DRAM_BASE=0x880000000`/`DRAM_SIZE=0x20000000` today), so widening it to
   fill the largest contiguous bank is the follow-up.
5. **Ports on the phone:** the five component ports are merged onto `main`
   (`0128`–`0133`, see the merge section). Their drivers are log-only or
   read-only by default, so the next boot with the flag-fixed kernel should also
   print the USB / PMIC / touch / buttons / panel state — the first device
   evidence for them.
6. Older housekeeping (still open): read the remaining board unknowns (ramoops
   `log_pstore` offset, gpa1 bank offset, eMMC CIU timings) from the live DT /
   Samsung source and fill `board.h`, then enable `JT_HAVE_RAMOOPS`.
