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
| 7 | launchd, then SpringBoard | **reached (screen output, 2026-11-08)** — `jt10`: launchd clears AMFI with `launchd_unsecure_cache=1`, SpringBoard execs, and the A8 panel shows the iOS Apple logo + boot progress bar (photo `IMG_20261108_170011203.jpg`) |

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
`0128`. CI `42494206360` (push of that fix) then built **successfully**.

### `jt10` image ready: flag-fixed kernel (2026-11-08, not yet on the phone)

CI `42494206360` produced `kernel.development.a5xelte` = 16,553,032 B, sha256
`ff38f990fd94b73665f3a210c25263b890807572b4eb0ee665a96a54ab6d35ad`, built with
`-DJACKPOTLTE=1`. It contains the live port symbols/strings
(`jackpotlte_pmic`, `a5xelte_usb_probe`, `a5xelte_panel_init`, `a8touchlog`,
`a5panelprobe`, the `JACKPOTLTE PMIC:` log lines) and **zero**
`A5 expand|pgalloc|tt_alloc|steal|pg|dg|de` console-spam strings.

Image **`/var/tmp/a8-xnu-jt10.img`** = 17,485,840 B, sha256
`2e3f2a568f098806079affc3e47ca71799643f68f217870114362f6942d7e55c` (jackpotlte
shim 52,048 B with the multi-bank DT probe, payload 16,547,472 B, DT 882,688 B),
cmdline
`rd=emmcroot0 emmcroot=USERDATA emmcwrite=1 unentitled_ios_sim_launch=1
cs_enforcement_disable=1 launchd_unsecure_cache=1 ignition_level=1`. This is the
image to flash next: the clean-console stage-7 trial with the S8 battery panic
fixed and the five ports compiled in. Flashing still needs the phone in fastboot.

### Stage 7 reached: iOS userland boots on the A8 (`jt10`, 2026-11-08)

The owner reports that after flashing `jt10` the A8 panel switched to **the iOS
Apple logo with the boot progress bar** — iOS userland is running on the A8, on
one core (`cpus=1`). Because that UI is drawn through IOMobileFramebuffer, the
display handover/`disp0` path is live too, not just launchd. Photo
`IMG_20261108_170011203.jpg` (SM-A530F panel): white Apple logo, portrait, with
the boot progress bar below it (roughly 40% at capture). This is the same end
state the Galaxy S8 donor reaches. Whether it finishes to the lock screen is not
yet confirmed.

### Enabling all 8 cores (`0134`, 2026-11-08)

The A8 has eight CPUs in **three** hardware clusters (Exynos 7885): 4x A53 at
MPIDR `0x100-0x103` (Aff1=1), 2x A53 at `0x200-0x201` (Aff1=2) and 2x A73 at
`0x0-0x1` (Aff1=0). `tools/mkadt_jackpotlte.py` already emits all eight ADT cpu
nodes in this order and tags them cluster 0/1/2.

But `ml_parse_cpu_topology()` maps clusters by `MPIDR_CLUSTER_ID` (the
`HAS_CLUSTER || defined(A5XELTE)` path from `0115`), and the A5 donor's board
config sets `MAX_CPU_CLUSTERS 2`. Across three physical clusters that trips
`assert(cpu->cluster_id < MAX_CPU_CLUSTERS)` on the third cluster — so `cpus=8`
would panic in topology parsing.

`0134-jackpotlte-cpu-clusters.patch` gates `MAX_CPU_CLUSTERS` to 3 for
`JACKPOTLTE` in `pexpert/pexpert/arm64/board_config.h` (the A5 keeps 2);
`MAX_CPU_CLUSTER_PHY_ID` then defaults to `MAX_CPU_CLUSTERS - 1 = 2`, the
highest Aff1, and both `cluster_phys_to_logical[]` / `cluster_max_cpu_phys_id[]`
grow to size 3. `SWEET_SMP_MAX_CPUS` and `MAX_CPUS` are already 8 (`0115`), and
`SweetCPU::start()`'s jackpotlte MPIDR map (`0126`) already covers `0x0-0x201`,
so the only change is the cluster count. Boot with `cpus=8`.

Risks (not yet seen on the phone): `SweetCPU::startAll()` is all-or-nothing — if
any secondary (especially an A73) fails to start via PSCI within 10 s it panics.
Verified only that `0001`-`0134` applies on a pristine tree and builds in CI; the
core bring-up itself is untested.

CI `42497660560` built it green (`kernel.development.a5xelte`, sha256
`dc60d32faa0a8710be2635e0b2af94a81636ea304f17b05f7a04f37af473a414`). Image
**`/var/tmp/a8-xnu-jt11.img`** = 17,485,840 B, sha256
`e3a379317d70632d52d0ca88678e24c09e69e135e0468bef591c3bc356685323`, same cmdline
as `jt10` but `cpus=8`. Not yet flashed: `jt10` is still creeping through the iOS
boot on one core, and swapping images drops that run.

**Flashed and panicked (photo `IMG_20261108_171522433.jpg`).** `0134` did its job —
the console shows `A5XELTE CPU: requested=8 selected=8 (limit=8)` with no
`unsupported topology` assert, so all eight CPUs were enumerated and the three
clusters were accepted. The secondaries then fail to start:

```
cpu_signal failed in DebuggerXCallEnter        (repeated)
panic(cpu 0 caller 0xfffffff007a66300): Ticket Lock 0xfffffff0083c0680 is
  unexpectedly owned by thread 0xfffffff6f053b430 @lock_ticket.c:143
```

`lock_ticket.c:143` is the "unexpectedly owned" detector: the boot CPU xcalls the
other cores, they never answer (`cpu_signal failed`), and a ticket lock they half
took is never released. So the panic is a **symptom of secondaries that are not
running**, not a lock bug. `SWEET FB: AppleCLCD 1080x2220 stride 4320 at
0xec000000, default surface 1, presented as 1080x2220` confirms display is up.

Conclusion: the cluster-count change is correct, but the A8 secondary bring-up
(`SweetCPU::startAll()` / the donor PSCI path) does not actually power the other
cores on this SoC. Next: step the count down (`cpus=2`, then 4) to see whether
*any* secondary comes up — no CI needed, the `0134` kernel is reused and only the
cmdline changes.

### `cpus=2` also panics: the core is never started (`0135`, 2026-11-08)

`jt11-cpus2` (same `0134` kernel, `cpus=2`) was flashed and panicked the same way
(photo `IMG_20261108_171958252.jpg`):

```
A5XELTE CPU: cpu=1 CPU_ON target=0x101 entry_pa=0x881417000 context=1
A5XELTE CPU: cpu=1 PSCI CPU_ON rs=0
A5XELTE CPU: cpu=1 mpidr=0x101 NOT alive after 10s
cpu_signal failed in DebuggerXCallEnter
panic ... A5XELTE CPU: cpu=1 startup timeout (reset/MMU/GIC/SGI) @SweetCPU.cpp:216
```

The secondary is in the **same** A53 cluster as the boot core (MPIDR `0x101`),
so this is not a cluster-count or A73 problem: the core-start path itself never
gets the core running. `rs=0` means firmware accepted the call, i.e. it thinks
the core started.

Two defects, both of which the `DREAMLTE` (S8) port already handles and the
jackpotlte build did not:

1. **Truncated entry.** `sweet_secondary_entry` is in the high bank —
   `entry_pa=0x881417000` — and firmware keeps the CPU_ON entry in a 32-bit
   register. The core therefore starts at `0x81417000`, i.e. low-bank garbage.
   The S8 code already avoids this with an `ldr x1, #8; br x1; .quad entry`
   trampoline in the low bank. `0x8f000000` is inside the jackpotlte low bank
   (`0x80000000+0x3da00000`) and clear of everything we know about: S-Boot puts
   the boot image at `0x800xxxxx` and the initrd at `0x84000000`, and the
   loader's own probe scratch is `0x8ff00000`.
2. **Core power is not sequenced.** SAM-K `drivers/soc/samsung/exynos-pmu.c`
   `pmu_cpu_ctrl()` powers a core by writing `CPU_LOCAL_PWR_CFG` (`0xf`) at
   `PMU_CPU_CONFIG_BASE` (`0x2000`) + offset and polling
   `PMU_CPU_STATUS_BASE` (`0x2004`) + offset, step `0x80`. Exynos 7885 overrides
   `pmu_cpu_offset()` with its own numbering — Aff1=1 (`0x100-0x103`) → 4+Aff0,
   Aff1=2 (`0x200-0x201`) → 2+Aff0, Aff1=0 (A73 `0x0-0x1`) → Aff0 — *not* the
   generic `(Aff1<<2)|Aff0` the S8 block uses (that would place the `0x200` pair
   at 8-9). PMU base `0x11c80000` (`exynos7885.dtsi system-controller`).

`0135-jackpotlte-secondary-core-start.patch` adds the jackpotlte branch:
trampoline at `0x8f000000`, PMU power-down/up on the 7885 core numbering, then
CPU_ON; a first `rc=0` counts as success so the second call is skipped. Verified
with `git apply -R`/forward on the `0001`-`0134` tree and
`tools/validate-repo.py`; **not yet run on the phone**.

The owner asked for the next image to use **all 8 cores**, so `jt12` boots
`cpus=8` straight away. If a secondary still fails to come alive the wrapper logs
name the CPU and both `rc` values, and a `cpus=2` image (same kernel, cmdline
only) is kept on disk to tell "no secondary can start" apart from "only the other
clusters fail".

CI `42501037411` built `0135` green in 11m40s. Kernel
`kernel.development.a5xelte` sha256
`32734fbead24e42e8b1f143131822dda0a8d8cf9f23632f6f71e268a97832ba7`; the binary
carries the new strings (`via low trampoline`, `PMU core=%u cfg=0x%x
status=0x%x`), so the jackpotlte branch really compiled in. Images:

- **`/var/tmp/a8-xnu-jt12.img`** — 17,485,840 B, sha256
  `fb77c1a162ea893264a30982df990466760297c498eb3b4bd095780d9a99500b`,
  cmdline `... cpus=8 ...` (verified in the image).
- `/var/tmp/a8-xnu-jt12-cpus2.img` — sha256
  `6c419ccd8bc8f6d9173cfc58436cededafcfe7a693949f886cc498b7f12aea5f`,
  `cpus=2`, diagnostic only.

Not yet flashed. `jt10` (working single-core boot) is untouched on disk.

### `cpus=4` boots: the secondary-start fix works (2026-11-08)

`jt12` was flashed with `cpus=8` and **failed**, then the same `0135` kernel with
`cpus=4` (`/var/tmp/a8-xnu-jt12-cpus4.img`, sha256
`34afd97b7f62439ca49e06bf9b776cf1fce676c2a5d75e3a9ed66e6aed998e8b`, cmdline
`cpus=4`) **boots** — reported by the owner as working. So the trampoline +
PMU power-up sequence is correct for the first A53 cluster (MPIDR `0x100-0x103`,
PMU core slots 4-7), which is the one place where the 7885 `pmu_cpu_offset()`
and the generic `(Aff1<<2)|Aff0` formula agree.

That isolates the remaining failure to the other two clusters: 2x A53 at MPIDR
`0x200-0x201` (PMU slots 2-3) and 2x A73 at `0x0-0x1` (PMU slots 0-1). The
`cpus=8` panic was not photographed, so `0136` makes the next run answer it
instead of guessing:

- Log, per secondary, `CPU_CONFIG`, `CPU_STATUS`, `CPU_RESET` (`0x200c + off`,
  the `DISABLE_*_RESET` bits), plus the cluster registers `CPUSEQ_OPTION`
  (`0x2488 + cluster*0x20`), non-CPU power status (`0x2404 + ...`) and L2 status
  (`0x2604 + ...`), before and after the power-up.
- Do what SAM-K `exynos_cluster_up()` does on the first core of each cluster:
  clear bit 0 of `CPUSEQ_OPTION`, which re-enables the PMU's power-up sequence
  for that cluster (`exynos_cluster_down()` sets it). Only the boot cluster is
  known to have it enabled; the other two are not.

If the sequence enable was the missing piece, `cpus=8` comes up. If it does not,
the logged `status`, `reset`, `noncpu` and `l2` values say whether the core is
unpowered, held in reset, or powered with the cluster still off.

The owner reports the `cpus=4` boot reaches **50% of the progress bar** promptly
(after the single-core run needed a long crawl to get there), i.e. the slowdown
really was lack of SMP and not a boot fault. Outcome beyond 50% not yet
recorded.

CI `42502641208` built `0136` green. Kernel `kernel.development.a5xelte` sha256
`9c727c27b0d47fdbd5ddccff482db4dc67848563856cd2422fca96de92cc2e8f`; the binary
carries the new strings (`pmu before`, `sequence enabled`,
`noncpu=0x%x l2=0x%x`). Two images built from it (cmdline verified in each):

- **`/var/tmp/a8-xnu-jt13.img`** — sha256
  `0977e441939c76b3e7e045fad5eb0a90467879486fe0b70556d367914f0d5387`,
  cmdline `... cpus=8 ...` (all three clusters).
- `/var/tmp/a8-xnu-jt13-cpus6.img` — sha256
  `7f5036e489688fb9d90873dada97a77e25ee67a6ab9fe4c98dd95c7e50e377e4`,
  `cpus=6` (both A53 clusters, no A73), one-flash bisect.

Not yet flashed.

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


### 3D acceleration: scoping (2026-11-08)

Scoping only — nothing applied and no boot-path change. Full write-up:
`docs/jackpotlte-mali.md`; draft-only skeletons in
`docs/drafts/jackpotlte-mali/` (not part of the numbered series).

- Reference is HakiOS's Mali stack (Mali-G52 MC2 on MT6768). Public, commit-
  pinned sources: `oosexgang/hakios-xnu-kernel` @ `620c775` branch `mtk`,
  `oosexgang/hakios-mt6768` @ `84b0252`, `oosexgang/hakios-metal` @ `d36f329`,
  Mesa 25.0.7. The Metal plug-in, the AIR→NIR frontend and the Mesa panfrost
  layer are all public source; only prebuilt binaries and Apple's
  `default.metallib` are not.
- **The A8 GPU is Mali-G71 = Bifrost v6**, not the G52 = v7 the reference is
  tuned for (`pan_props.c:61`; arch = `gpu_id >> 12`, `pan_props.h:113-128`).
  The reference's Darwin panfrost build compiles **v7 only**
  (`tools/mesa/panfrost-darwin.patch:199`) and `pan_screen.c` returns NULL
  unless `arch == 7` (`:222`) — so v6 must be re-enabled in the Mesa build and
  screen-init dispatch before any port can render.
- Exynos 7885 GPU facts (Samsung Eureka 4.4.302 tree; not yet read on the
  phone): `mali@0x11500000` size `0x5000`; IRQ names JOB/MMU/GPU = GIC INTID
  **191/192/190**; power `pd_g3d@11c840a0`, status `0x11c840a4` mask `0xf`;
  clocks in CMU_G3D `@0x11400000` (`PLL_G3D` con0 `@0x120`, div `@0x1800`,
  gate `@0x2004`) plus `CORE_G3D` in `@0x12000000`; **no** `samsung,sysmmu`
  node for the GPU. Citations in `docs/jackpotlte-mali.md` §3.
- The jackpotlte tree has no GPU or accelerator code: no `IOAcceleratorES`,
  no Redwood/Mali driver. The display path is the SWEET one —
  `SweetSurface`/`SweetFramebuffer` (`0046`), pixels at `s->memory + s->offset`,
  and `AppleCLCD::swap()` is a CPU memcpy to panel memory (`0046:1217`).
- Plan shape: M0 read-only GPU probe → M1 pd_g3d/PLL_G3D/CMU_G3D power+clock →
  M2 MMU + one job → M3 `RedwoodMali`-shaped user client over `SweetSurface` →
  M4 Metal plug-in with v6 enabled. The draft M0 probe reads only always-on
  registers and reads GPU registers only if the PMU status says G3D is already
  up (an access to an unpowered block can stall the bus).
- **Single biggest unknown:** whether S-Boot leaves G3D powered and clocked. If
  not, no GPU register read works until the power/clock sequence runs. Source
  cannot answer this; one readback can. That is what M0 is for.

### All eight cores in one logical cluster (`0137`) + the iOS-root gap (2026-11-08)

`0137-jackpotlte-single-logical-cluster.patch` (31 insertions, 2 hunks) takes
`cpu->cluster_id` from the ADT `cluster-id` and registers
`cluster_phys_to_logical[]` for **every** CPU, not only the first per physical
cluster. `tools/mkadt_jackpotlte.py` publishes `cluster-id = 0` for all eight
CPU nodes, so one `pset` is built under `pset_node0` and the half-owned ticket
lock at `lock_ticket.c:143` is not reached. The ADT byte-diff showed exactly
`1->0, 1->0, 2->0, 2->0`; the remaining differing bytes are `random-seed`.

CI `42510117442` (xnu) built `0137` green in 9m29s; `loader` `42510117397`
regenerated the ADT green. Kernel `kernel.development.a5xelte`
16,553,232 B, sha256
`d47087a240a4334d6fcb357850839037d6ff3b4ac973865efdd79cbfb7c4b32f`.

- **`/var/tmp/a8-xnu-jt14-cpus8.img`** — 17,473,552 B, sha256
  `aeb3a9791fbbe94fa966fa0dd90426e9c162002ce06f5417260fbf7e98e427a9`,
  built from the `0137` kernel with `tools/jackpotlte/make-boot.sh`; cmdline
  `... cpus=8 ...` (verified in the image) and the built ADT shows
  `cluster-id = 0` for all eight CPUs. **Not yet flashed.** `jt13` (from
  `0136`) was also never flashed, so if `jt14` boots the result is the
  single-cluster change on top of `0136`; a `jt13` control is on disk
  (`a8-xnu-jt13.img`).

### jt14 panicked: the ADT lost its platform-match entry (`jt16`, 2026-11-08)

`jt14` (8 cores) was flashed together with the patched iOS root and panicked
(photo `IMG_20261108_204818781.jpg`):

    panic(cpu 0 ...): Unable to find driver for this platform: "samsung,jackpotlte"
    @IOPlatformExpert.cpp:2338

That is `IOPanicPlatform::start()`: no `IOPlatformExpert` personality matched,
so the fallback driver ran instead of `SweetPlatformExpert`. It is not the
ticket lock and not the core count.

Dumping the ADT out of each boot image's `SWEETXNU` payload settles it:

| image | root `compatible` | ADT |
| --- | --- | --- |
| jt10 | `samsung,jackpotlte\|samsung,exynos7885\|AppleARM\|samsung,a5xelte` | 9796 B |
| jt13 | same as jt10 | 9796 B |
| jt14 | `samsung,jackpotlte\|samsung,exynos7885\|AppleARM` | 9780 B |

The A5XELTE kernel's `SweetPlatformExpert` matches `('samsung,a5xelte',
'samsung,dreamlte')` (`iokit/KernelConfigTables.cpp`, from `0086` + `0124`),
and IODT matching considers every compatible entry. `jt10`/`jt13` bound
because they still carried the trailing `samsung,a5xelte`; `jt14` lost it.

Root cause is a build-path mismatch, not the kernel: `mkadt_jackpotlte.py`
never emitted that entry. The old local scripts
(`/var/tmp/xnu-build/mkimg-jt-*.sh`) appended it as a post-processing step
("keep the real A8 names first and append the donor name for the platform
match"), but `jt14` was built with the in-tree
`tools/jackpotlte/make-boot.sh`, which does not. The append was an
uncommitted assumption of those /var/tmp scripts.

Fixed in `5dedd44`: `mkadt_jackpotlte.py` appends `samsung,a5xelte` itself, so
every build path gets it; the regenerated ADT is 9796 B, the size of the
jt10/jt13 ADTs. Kernel and iOS root are untouched.

New BOOT images from the same verified `0137` kernel (`d47087a2...`), ADT
9796 B with the 4th entry verified in-image:

| image | cmdline | sha256 |
| --- | --- | --- |
| `/var/tmp/a8-xnu-jt16-cpus4.img` | `... cpus=4 ...` | `8ca8fac83c7ce5076c4582a26efd989fc3b0e07faa7b8573de5c4849765b4532` |
| `/var/tmp/a8-xnu-jt16-cpus8.img` | `... cpus=8 ...` | `73963022efb00ef751648689ad98ba67ab47e1479e40888525c4f01f48399cda` |

`cpus=4` first (owner's request): the platform panic is core-independent, so a
`cpus=4` image would have hit it too. Four cores isolates the DataMigration
test from the still-unproven `0137` cluster change; `cpus=8` is the next trial
if `0137` needs verifying. The patched root was already on USERDATA.

`jt16-cpus8` was flashed (photo `IMG_20261108_210452786.jpg`, 21:04). It
clears both earlier failures and then stops one step further:

    A5XELTE: platform expert registered                 <- ADT fix
    A5XELTE CPU: requested=8 selected=8 (limit=8)
    [0]ml_processor_register>pset_find(cluster_id=0) returned pset 0   x8
    ... cpu_number 0..5 ...
    A5XELTE CPU: cpu=6 NOT registered (IOKit matching timeout)
    A5XELTE CPU: cpu=7 NOT registered (IOKit matching timeout)
    panic: A5XELTE CPU: CPU registration timeout @SweetCPU.cpp:199

So the platform panic is gone and `0137` holds (one pset, no ticket lock),
but the two A73 big cores never register. `SweetCPU.cpp:192` prints "NOT
registered" when `sweet_cpus[i]` is still `NULL` -- i.e. the nub was never
*created*, an IOKit match failure, not a PSCI/power failure. The cause:
`iokit/KernelConfigTables.cpp` gave SweetCPU `IONameMatch = 'arm,cortex-a53'`
only, while `mkadt_jackpotlte.py` builds cpu6/cpu7 (MPIDR 0x000/0x001, Aff1 0)
with `compatible = 'arm,cortex-a73'`. Extending the match to the tuple
`('arm,cortex-a53','arm,cortex-a73')` (the same trick `0124` used for the
platform expert) fixes it. Kernel-only.

That hunk is parked as `docs/drafts/jackpotlte-cpus8/0146-...patch.draft`:
`tools/validate-repo.py` needs `xnu/patches/` contiguous (0001-0139) and
0140-0145 are held by the six subsystem tracks still on their own branches,
so a main-repo 0146 left a 0139 -> 0146 gap. A branch `jackpotlte-cpus8`
carries it as 0146 purely for a compile verdict; renumber on merge. The
owner has parked 8 cores for now.

**`0146` compile-verified and the kernel is built** (2026-11-08): branch run
`42519017042` (`a6a47f6`) **succeeded** in 9m39s. Artifact
`/tmp/opencode/art0146/kernel.development.a5xelte`, 16,553,800 B, sha256
`8782aa4e06cf50e43a5332dfd4aba6de93a0301ce3fb8a70667b9760c5f4e664`. The
personality table inside that binary reads
`'IONameMatch' = ('arm,cortex-a53', 'arm,cortex-a73')` for `SweetCPU`, so
the fix is linked in, not just applied to source. Image
`/var/tmp/a8-xnu-jt20-cpus8.img` (jt17 cmdline with `cpus=8`), 17,485,840 B,
sha256 `129bd8eabae57464a8bf870c1491669c789cba31187592a92258625edcba0e8e`.
Not flashed.

**No renumbering is needed after all.** The six subsystem tracks each add
exactly one patch and together occupy 0140-0145 with nothing left over:

| track | branch | patch |
| --- | --- | --- |
| BT | `jackpotlte-bt` `abd8999` | `0140-jackpotlte-scsc-bt-transport` |
| audio | `jackpotlte-audio` `52e8c61` | `0141-jackpotlte-audio-probe` |
| camera | `jackpotlte-camera` `77704d1` | `0142-jackpotlte-camera-probe` |
| sensors | `jackpotlte-sensors` `0fed091` | `0143-jackpotlte-sensors-probe` |
| flashlight | `jackpotlte-flashlight` `702718d` | `0144-jackpotlte-flashlight-probe` |
| mic | `jackpotlte-mic` `6b4e5bb` | `0145-jackpotlte-mic-probe` |

So merging the six in that order makes `xnu/patches/` run 0001-0145
contiguous and 0146 can then be moved out of `docs/drafts/` unchanged. Every
track's own `loader` CI red is only the gap error
(`expected 0140, found 014N`), which that merge clears everywhere.

Interim 6-core image (A53s only, skips the A73 pair): **flashed 2026-11-08
~21:15** after the owner asked for 6 cores for now, and the owner
reports it **boots properly**. `/var/tmp/a8-xnu-jt17-cpus6.img`, 17,485,840 B,
sha256 `fd3ab067e97131079c30881cc6336156bf61e432fbe22396fd8bb8c4f5e8408a`.

Evidence level: **owner statement, no photo yet**. That covers "no panic,
kernel comes up with 6 CPUs"; it does **not** yet cover how far userland got.
The open question for this image is unchanged: does the progress bar run past
the old stall (the `com.apple.migration.plist` / `DMLastMigrationResults`
tweaks, verified in the image but never booted) and does SpringBoard appear.
`jt10` already reached logo + bar, so "boots properly" is progress on
stability, not yet proof of stage 8.

**Why the screen shows no logo/bar on `jt17-cpus6`:** its cmdline carries
`-noprogress`, which exists only to stop the graphical boot indicator from
corrupting console text (`NOTES.md:245`). With it set, the owner sees the
kernel/userland log stream and the progress UI never
draws, so this image **cannot** answer the DataMigration question. Built
`/var/tmp/a8-xnu-jt18-cpus6-ui.img` (same `art0137` kernel, 17,485,840 B,
sha256 `77cee4472e44a48a80c39d4993aeb740df0880ccb0ecb25989336ef953031d29`)
with the identical cmdline **minus** `-noprogress` and
`-enable_kprintf_spam`, so the Apple logo + bar draw. Flashed; it **panicked
with "Assertion failed"** (text not captured) and jt17 was flashed back as a
bisect.

Two findings from that round:

- `-noprogress` is **inert on iOS**. `pexpert/arm/pe_init.c:284-289` gates the
  `-noprogress` branch under `XNU_TARGET_OS_OSX`; the iOS build sets
  `show_progress = FALSE` and only honours `-progress`. The boot UI on this
  kernel is userland (`BootProgress`), not pe_init. So removing `-noprogress`
  cannot have caused the panic; the only real change was dropping
  `-enable_kprintf_spam` (`pe_kprintf.c:48-51`), or the panic was independent
  of the cmdline (USERDATA state written by the previous boot with
  `emmcwrite=1`).
- The right knob for "see the UI and keep the logs" already exists:
  `sweetfb_noconsole=1` muting is compiled for this board
  (`osfmk/console/serial_console.c:275` includes
  `ARM64_BOARD_CONFIG_A5XELTE`; `0109` wires it), console text still goes to
  the persistent log, and panics still draw (`sweet_console_muted &&
  !panic_active()`).

Because kprintf spam paints over the framebuffer console, jt17 can show logs
*or* the GUI but not both, which is why no logo was visible on it.
`/var/tmp/a8-xnu-jt19-cpus6-ui2.img` (17,485,840 B, sha256
`a8fbf0073aa424fb51038cb1605f8882bdfdbf30c8696872e4f5b8161967865a`) is the
jt17 cmdline **plus** `sweetfb_noconsole=1`, nothing else changed. Not
flashed yet.

**Correction (owner, 2026-11-08 ~21:25): the extra boot args were not needed
and `jt19` should be ignored.** Plain `jt17-cpus6` **does draw the Apple logo
properly**. The earlier raw log output before the Apple logo was simply the
early-boot console phase before userland's `BootProgress` took the panel, not
a suppressed UI. So `-noprogress` is confirmed inert on this kernel by
observation, matching `pe_init.c:287`, and `jt19`/`jt20`'s UI experiments are
unnecessary for this question. The jt18 "Assertion failed" therefore was
**not** a cmdline effect and remains unexplained; it did not recur on jt17.

### SCSC/Maxwell WiFi port start (`0138`, `0139`, 2026-11-08)

`0138-jackpotlte-scsc-mx-transport.patch` adds `bsd/dev/a5xelte_scsc.{c,h}`:
PMU/mailbox/MIF mapping, mailbox bit ops, a bump MIF-DRAM allocator, the
`mx140.bin` read and "smxf" header parse, the MBOX0-3 startup contract and the
PMU reset-release handshake. `0139-jackpotlte-apple80211-glue.patch` adds
`bsd/dev/a5xelte_wifi.c`: an `en0` ifnet with `SIOCS/GA80211` handling for
POWER/STATE, `EOPNOTSUPP` for the rest, and an IEEE-80211 power `kev_msg`.
Both are wired through `bsd/conf/files` + `iokit/bsddev/IOKitBSDInit.cpp` and
merged to `main` (`c55c7bb`).

**Gated by `a8wifi=1`.** A default boot maps PMU + MIF DRAM only and reads
four always-on PMU registers; the mailbox is mapped only when `a8wifi=1`.
CI `42512239378` built `0138`+`0139` **green** (Build, stub and LSE gates all
passed), so the port compiles; it has **not** been booted. DT values: R4 `0x120c0000`/`0x180`, M4 `0x120e0000`/`0x180`,
IRQs `<0 57 4>`/`<0 21 4>`/`<0 286 4>`, PMU `0x11c80000`/`0x10000`, MIF
`0xE9000000`/`0x400000` (`exynos7885.dtsi:1798-1803`, `:1291`,
`exynos7885-rmem.dtsi:55-58`). Firmware: 54 files / 7.2 MB in
`/var/tmp/opencode-keep/jackpotlte-wifi-fw/` from `VDavid003/samsung_jackpotlte_dump`
(`A530FXXS8CSK2`), `mx140.bin` sha256
`004e5982d658099a412f4f388f3aac7554a2e1fd63e7f988c3918ac6a6c88d07`. Details:
`docs/drafts/jackpotlte-wifi/PORT-START.md`. The earlier draft
`PORT-ATTEMPT.md` (untracked until now) assumed an `IO80211Controller`
subclass and a `redwoodwifi=1` gate, both wrong; it is now bannered
superseded and points at `PORT-START.md` (commit `17730ef`).

### The flashed iOS root has none of the donor tweaks (2026-11-08)

A `7z l` listing of `/var/tmp/iosroot/iOSRoot.img` (the image flashed to
USERDATA) shows the plain `tools/mkiosroot.sh` output: **no**
`/private/var/mobile/Library/Preferences/com.apple.migration.plist`, no
purplebuddy `SetupDone`, no `BKNoWatchdogs`, no mobileactivationd data ark, no
`private/var/containers`, VirtualAudio still installed. `mkiosroot.sh` never
calls `tools/sweet-root-tweaks.py`; that is the donor's pre-v12 state, which
is the endless-DataMigration boot (bar runs, SpringBoard held in `_checkIn`).

**APPLIED to the host image on 2026-11-08, verified in the image (not booted).**
`tools/sweet-root-tweaks.py` also writes `DMLastMigrationResults`
`{success:true, buildVersion:20H392}` (the key HakiOS / danstronics /
`lechaukha12/xnu-xzs` ship; one line to drop).
`pkexec tools/jackpotlte/apply-root-tweaks.sh /var/tmp/IOSRoot.img` ran to
completion: `fsck_hfs` reported the case-sensitive volume `iOSRoot` **OK**
before and after, it mounted `loop,rw,force`, and it rewrote the image. Read
back out of the finished image with `7z e -so`:

- `com.apple.migration.plist` — `LastSystemVersion=20H392`,
  `DMUserDataDisposition=SyntheticDidUpgrade`,
  `DMLastMigrationResults={buildVersion:20H392, success:true}`
- `com.apple.purplebuddy.plist` — `SetupDone`, `SetupFinishedAllSteps`,
  `SetupVersion`, `daemonizeNeedsToRun`
- `com.apple.springboard.plist` — `BKNoWatchdogs`,
  `FBLaunchWatchdogExceptions`, `MCForceShowCloudConfigurationUI`
- `private/var/containers/{Bundle,Data/System,Shared/SystemGroup,Temp}` created
- `launchd.plist`: 7 `_PanicOnCrash` renames, 2 CommCenter jobs removed;
  VirtualAudio plug-in disabled, `StartRecorderServer=false` (3 plists)
- `mobileactivationd` data ark **skipped**: the image has no such container
  (expected — `mkiosroot.sh` never created one)

The live image is now **`/var/tmp/IOSRoot.img`** (16 GiB apparent, 11.31 GiB
allocated), rebuilt from the flashable sparse because of the hazard below.
The flashable derivative is re-sparsed from it with `img2simg`:

| file | bytes | sha256 |
| --- | --- | --- |
| `/var/tmp/IOSRoot.img` (master) | 17,179,869,184 | — |
| `/var/tmp/IOSRoot.sparse.img` (flash this) | 12,145,451,332 | `0d5b7a3bff914e363e61c976a4cfaa0e7ebee7b2bab462ec9257bcb62b478b63` |

`img2simg` produced it in 2m44s; a full `simg2img` parse of every chunk exits
0, and the sparse magic is `0xED26FF3A`. Flashing USERDATA is destructive and
still needs the owner's confirmation. Nothing here has been booted.

**Host filesystem hazard (2026-11-08) — worked around, cause not fixed.**
The ext4 directory index for `/var/tmp/iosroot` is damaged. A fresh *lookup*
of a name in that directory intermittently returns `ENOENT` while `readdir`
still lists the entry (`ls -la` shows it, `stat`/`open`/`rm` fail in the same
window). It flips within seconds, for user and root alike, and it hit a
**newly created** entry too, so it is the directory, not one inode.
`debugfs -R 'stat ...' /dev/sda2` prints `Inode bitmap checksum does not match
bitmap` and refuses to open without `-c`; the kernel logs no ext4 errors and
the fs reports `clean`. No data is lost — inode 548316 was intact (regular,
`Links 1`, 16 GiB, full block list).

Done instead of fighting it: the raw was **rebuilt from the flashable sparse**
into a healthy directory (`/var/tmp`), so no lookup goes through the damaged
index. Do not put the image back in `/var/tmp/iosroot`.

- **`simg2img` on this host writes the output densely.** It needs the full
  16 GiB and it *filled `/` to 100%* on 2026-11-08 before failing with
  `No space left on device`, leaving a truncated raw. Always use
  `tools/jackpotlte/unsparse-holey.py` instead: it seeks over don't-care and
  zero-fill chunks, so the output is 16 GiB apparent but only ~11.3 GiB
  allocated (4.7 GiB of holes), matching the original raw.
- `tools/jackpotlte/unsparse-holey.py --check SPARSE` prints the exact bytes
  needed before writing anything.
- `apply-root-tweaks.sh` still retries the existence check, fsck and mount, and
  runs with a relative name from the image's directory; that stays useful for
  ordinary transient lookup failures.
- Do **not** run `fsck.ext4` on the mounted root.

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
  status `0x2004`, step `0x80`, `CPU_LOCAL_PWR_CFG 0xF`. Exynos 7885's
  `pmu_cpu_offset()` override maps MPIDR Aff1=1 (`0x100-0x103`) to core slots
  4-7, Aff1=2 (`0x200-0x201`) to 2-3 and Aff1=0 (A73 `0x0-0x1`) to 0-1.
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

Current state: stage 7 reached (`jt10`, iOS logo + progress bar; then `cpus=4`
`jt12` reaches 50% promptly and at least the migration screen). `0137` (one
logical cluster) and `0138`/`0139` (WiFi transport/glue, gated by `a8wifi=1`)
are merged to `main`. The donor root tweaks were never applied to the flashed
image (see "The flashed iOS root has none of the donor tweaks").

1. **8 cores:** flash **`/var/tmp/a8-xnu-jt14-cpus8.img`** (sha256
   `aeb3a979...e427a9`, kernel = `0137`) to `BOOT` and look for
   `requested=8 selected=8` with no ticket-lock panic. If it still panics, the
   `jt13` image on disk (`0136` kernel) is the control for the single-cluster
   change. BOOT-only flash; capture a **serial** log, not a photo, to see the
   MPIDRs attempted.
2. **WiFi (compile green, never booted):** CI `42512239378` built
   `0138`+`0139` **success** (Build, stub and LSE gates passed). Boot that
   kernel **without** `a8wifi` and confirm only the
   `A5XELTE SCSC: PMU ...` identity lines appear; then try `a8wifi=1`.
   `/var/tmp/a8-xnu-jt15-cpus8.img` (sha256 `914edae8...c5991a`) is that
   kernel at `cpus=8`.
3. **iOS root (DataMigration gate):** host side **done** — applied and
   verified in `/var/tmp/IOSRoot.img` (see "APPLIED to the host image").
   Remaining: `img2simg /var/tmp/IOSRoot.img /var/tmp/IOSRoot.sparse.img`,
   then `fastboot flash USERDATA` — destructive, needs the owner's
   confirmation. This is the host-side fix for the endless progress bar; no
   kernel change.
4. **DRAM map (for "all 4 GB"):** `shim/fdt.c`'s `fdt_memory_all()` enumerates
   every `/memory` node and `dt_probe()` prints each bank plus the total. Source
   banks (Eureka `exynos7885-jackpotlte_common.dtsi:28-41`): low
   `0x80000000+0x3DA00000` (986 MiB), mid `0xC0000000+0x40000000` (1024 MiB),
   high `0x880000000+0x40000000` (1024 MiB) ≈ 3034 MiB. XNU takes one
   contiguous window (`arm_init.c` → `gPhysBase`/`gPhysSize`; ADT
   `DRAM_BASE=0x880000000`/`DRAM_SIZE=0x20000000` today), so widening it to
   fill the largest contiguous bank is the follow-up.
5. **Ports on the phone:** the five component ports are merged onto `main`
   (`0128`–`0133`). Their drivers are log-only or read-only by default, so a
   boot should print the USB / PMIC / touch / buttons / panel state — the first
   device evidence for them.
6. **GPU:** `docs/jackpotlte-mali.md` M0 is the read-only GPU probe (the single
   biggest unknown is whether S-Boot leaves G3D powered/clocked).
7. Older housekeeping (still open): read the remaining board unknowns (ramoops
   `log_pstore` offset, gpa1 bank offset, eMMC CIU timings) from the live DT /
   Samsung source and fill `board.h`, then enable `JT_HAVE_RAMOOPS`.
