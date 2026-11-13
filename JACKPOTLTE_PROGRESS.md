# Galaxy A8 2018 (SM-A530F, jackpotlte) port — progress

Branch: this repository ports the Galaxy S8 (`dreamlte`, Exynos 8895) iOS work
to the **Galaxy A8 2018 (SM-A530F, `jackpotlte`, Exynos 7885)**. The S8 tree is
the base; we adapt it, we do not rewrite it.

Observation is kept strictly apart from plan, and build success is never a boot
claim. **A boot stage counts as reached only with a photo, serial log or screen
output from the phone.**

### History rewrite (2026-11-09)

At the owner's request not to be quoted in a public repository, the public
history was rewritten with `git filter-repo` to remove every verbatim owner
quote from `JACKPOTLTE_PROGRESS.md`, `NOTES.md` and from commit messages. Only
those quoted phrases changed (replaced with neutral attribution such as "the
owner approved/asked"); no technical fact, image hash, patch or file was
altered. Because this rewrites every commit from the first affected one on,
**all SHAs after that point are new** and `main` was force-pushed (new tip
`1ed6265`, old tip `29c50b6`); `jackpotlte-{bt,audio,camera,mic}` were
force-pushed too. Short SHAs referenced elsewhere in this document are
pre-rewrite and no longer resolve; any existing clone or fork must re-clone.

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
| 8 | DataMigration completes; SpringBoard lock screen | **reached (owner-observed screen output, 2026-11-09)** — `jt25-cpus8` (8 cores, `0140` A73 match + `0141`-`0143`): after ~20+ minutes the phone shows the iOS **lock screen**, i.e. DataMigration ran to completion and SpringBoard's UI is up. Owner report; photo still wanted |

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
| `0130` | sec_ts I2C4 id probe + `tsp_ldo_en` power log; opt-in `a8touchpwr` power-up (LDO gpg3-0 + I2C4 QCH gate) | `a8-touch` (0131) |
| `0131` | sec_ts contact path (`a8touchlog`); full raw+decoded event log, always-starts worker | `a8-touch` (0132) |
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

#### GPU port (2026-11-09): M0+M1 in patch 0144, merged to main

The port runs on its **own branch `jackpotlte-gpu`** (worktree
`/home/not_andrej/ios-jackpotlte-gpu`), forked from `main` at `523beb5`, so it
never touches the touch work on `main`. Its reference is HakiOS's Mali-G52
stack (Bifrost **v7**); our G71 is **v6**, so v6 must be re-enabled in the Mesa
build and in `pan_screen.c`'s arch dispatch (`arch == 7` today) before anything
can render.

State when the worker stopped: patch
`xnu/patches/0144-jackpotlte-mali-probe.patch` (M0, the read-only PMU/CMU/GPU-id
probe on boot arg `jtmaliprobe=1`) is committed on the branch and **CI-green**
(`gh workflow run xnu.yml --ref jackpotlte-gpu`, run `42632879797`); nothing has
been flashed from that branch, so there is no device result for M0 yet.
Next steps: pack/flash the `jtmaliprobe=1` build and read the raw
`JACKPOTLTE MALI:` lines (that answers the biggest unknown above), then the
`jtmalipwr=1` build for M1; then M2 MMU + one job (INTIDs JOB/MMU/GPU =
191/192/190), M3 a `RedwoodMali`-shaped user client, M4 Mesa panfrost with
**arch 6** enabled plus the Metal plug-in.

The worker was an `ollama-cloud/beam` subagent; it stopped on the provider's
monthly usage limit (not a technical failure). The owner then said to take it
over, so **the main assistant owns the GPU port** from here (no subagent).

Latest state (commit `0d49191` on `jackpotlte-gpu`): patch
`xnu/patches/0144-jackpotlte-mali-probe.patch` now carries **M0 + M1**.
M0 (`jtmaliprobe=1`) logs PMU `0x40A0`/`0x40A4` and, only once the domain reads
up, `PLL_G3D` and the Mali `GPU_ID`; the earlier draft read `CMU_G3D`/`PLL_G3D`
before checking status, which can stall a bus on a powered-off block (those
registers are in the G3D domain, `pmucal_local_exynos7885.h:279-306`) — fixed.
M1 (`jtmalipwr=1`) powers `pd_g3d` and its clocks (PMU `0x40A0=0xF`/wait
`0x40A4`; seven `CMU_G3D` QCHs; `PLL_G3D` ENABLE bit31/STABLE bit29; eleven
manual gates bit20+bit21) and then brings the GPU itself up: soft reset
(`GPU_CMD`) + `L2`/`TILER`/`SHADER_PWRON` + `*_READY`, the reference's
`mali_up()` step 4 (`redwood_mali.c:544-567`). A Bifrost needs no firmware, so
`M1 done, GPU up and reset` with a Bifrost `GPU_ID` is M1 complete.
Build was re-dispatched as run `42640666505` (the previous queued run was
cancelled to free the runner); pack/flash with `docs/gpu-bringup.md`. **Nothing
from this branch has been flashed, so M0/M1 still have no device result.**
Note the branch is based on `523beb5`, so its image predates the jt28 touch
fix; that is fine for GPU testing but the branch will need a merge onto `main`
(conflict expected only in this progress file) before it lands.

**Correction (important).** `42640666505` came back green but the kernel it
produced **did not contain the driver**: blacktop's `build.sh` only warns when
`git apply --check` fails, so 0144 was silently skipped
(`error: patch failed: bsd/conf/files:109`) — the earlier "CI-green" M0 result
was hollow for the same reason. The anchors were wrong: `bsd/conf/files` pads
with **three** tabs, and after `a5xelte_panel_init();` comes
`#if defined(JACKPOTLTE)/jackpotlte_pmic_probe()`, not a brace. Fixed in
`87e4d92`; a dry-run against the run's own `xnu-buildtree` now applies 0144
with no fuzz. To stop this class of bug, the workflow now **fails the run if
any `xnu/patches/*.patch` appears in a "Skipping patch" line** (`main` commit
`9115b42`, and the same guard on the branch). Verify every future build with
`strings` for the expected `JACKPOTLTE MALI:` / `A8 TOUCH` lines — a green run
is not enough.

**Merged to `main`** as `9876e87` (2026-11-09): the GPU branch is merged, so
`xnu/patches/` is `0001-0144` contiguous on main and the CI guard is shared.
The `jackpotlte-gpu` worktree/branch can be retired once its build is no longer
needed. Main now rebuilds with 0144 (run `42644071418`); GPU work continues on
`main` from here.

**M0 panic on the phone (2026-11-09).** The first device boot with
`jtmaliprobe=1` (photo `IMG_20261109_164934683.jpg`) panicked inside the new
probe: `pc 0xfffffff00747bf30` = `_jackpotlte_mali_probe+0x60`
(`ldr w8,[x0,#0x40a0]`), `lr +0x50` = the `cbz x0` after
`ml_io_map(0x11c80000, 0x1000)`, `esr 0x96000007` (data abort, translation
fault level 3), `far = x0 + 0x40a0`. The map covered one 4 KiB page but the
probe reads PMU `+0x40A0` (the fifth page), so the L3 PTE is absent. No
`JACKPOTLTE MALI:` line printed, i.e. it died building the first one.
The PMU syscon is a `0x10000` window (`exynos7885.dtsi:298-301`,
`reg = <0x11c80000 0x10000>`; docs/research/exynos7885-mainline.md:127,293),
the same size `0128`/`0135` map, so `JT_PMU_WINDOW` became `0x10000`
(commit `1d038c0`, 0144).

**That build failed, and the reason matters.** CI `42651805295` ended in
`jackpotlte_mali.c:13:2: error: unterminated conditional directive`: the
window fix added four comment lines to the new-file hunk but left
`@@ -0,0 +1,300 @@`, while the file is now 304 lines. The build materialised
only the first 300 lines, dropping the closing `#endif`. Fixed the header to
`+1,304` and re-checked every hunk's `+`/`-` counts against its header
(commit `7c3e2ab`); rebuild is run `42667447712`. This is the same class of
trap as the silent-skip above: a green local `validate-repo.py` did not catch
a hunk whose declared length disagreed with its body, so a patch's line
counts must be verified before pushing.

Note the host is Linux with no Apple SDK/KDK, so **kernel builds only come
from the `macos-15` CI runner**; push triggers only `main`/`s8-dreamlte`, so the
branch must be built with `gh workflow run xnu.yml --ref jackpotlte-gpu`.

**Merged to `main` and the branch retired (2026-11-10).** The GPU series is no
longer a side branch: `jackpotlte-gpu` (`0c48a82`, M2 in `0152` on top of the
touch fixes `0148`-`0151`) was fast-forwarded into `main` and pushed; the
worktree, the local branch and `origin/jackpotlte-gpu` were then deleted. From
here GPU patches go to `main` like every other patch, so `xnu.yml` builds them
on push (no more manual `--ref`, and the note above no longer applies).

M2's kernel was verified after the merge (run `42866306317`, success): the real
`kernel.development.a5xelte` (16,571,352 B, sha256 `bca16b89...434cf3e3`)
carries `job 1, identity mode`, `job 2, GPU page tables`, `-- the GPU ran it`,
`M2 done as0_status=...` and the `0150`/`0151` touch strings. Image:
`/var/tmp/a8-xnu-jt39-gpum2.img` (sha256 `ae6a99e9...2379fde`, base cmdline +
`jtmaliprobe=1 jtmalipwr=1 jtmalijob=1`). A watcher had wrongly reported the
two job strings missing: it grepped a 30-byte `.lse.txt` sidecar instead of the
kernel binary.

M3 is **green and packed**, not yet on the phone: `0153` (persistent engine —
dynamic three-level page tables, GPU address allocator, job queue and worker
thread) and `0154` (`iokit/Kernel/arm/JackpotlteMali.cpp`, the panfrost user
client over `SweetSurface`, started from `IOKitBSDInit`), both gated by
`jtmalimetal=1`.  Run `42871632907` is success; its real
`kernel.development.a5xelte` (16,598,424 B, sha256 `a34d2968...93297f3a`)
carries `engine up, addresses from`, `JackpotlteMali: request`,
`the GPU is there for programs to draw with`, `the GPU did not come up` and
both `job ...` lines, so the patches are in the kernel and not skipped.  Image:
`/var/tmp/a8-xnu-jt42-gpum3.img` (sha256 `536ab7ed...4a120901`), base cmdline +
`jtmaliprobe=1 jtmalipwr=1 jtmalijob=1 jtmalimetal=1`, `cpus=8`.

Runs `42869660692` and `42870910337` were superseded by `fc3c750` + `ae9582f`,
which silence three warnings — `-Wmissing-prototypes` on `0153`'s new
non-static functions and on `0154`'s `extern "C" jackpotlte_mali_service_start`,
and `-Wunused-parameter` on `0154`'s `externalMethod`.  They were warnings, not
failures: this kernel builds with `-Weverything` but **no** `-Werror` (a green
run's log holds 553,341 warnings), so only a patch that will not apply or a
compile/link error turns a run red.  Superseding cost two CI cycles; check the
logged compile command for a bare `-Werror` before treating a warning as fatal.

Checked here while that build ran, without CI or a device: against a green
run's `xnu-buildtree` artifact (which holds the real post-`0152` `xnu/` tree),
`git apply --check` accepts both patches (one one-line offset in
`IOKitBSDInit.cpp`), and `tools/xnu-cc.py` compiles both sources with that
run's own compile commands — `0 of 2 files failed`.  With `XNU_CC_OBJDIR`
set, `llvm-nm` on the emitted arm64 objects shows the user client's only
non-Apple undefined symbols are the `jackpotlte_mali_*` ones (all defined in
the driver object) and `sweet_surface_copy`, and that
`jackpotlte_mali_service_start` is exported unmangled.  So M3's remaining
unknowns are runtime ones, not build ones.  Recipe in `docs/gpu-bringup.md`.

**M4 step 1 written as `0155` (2026-11-10, build-green, not yet on the phone).**
`0155-jackpotlte-metal-accelerator.patch` adds the `IOAcceleratorES` service
Metal discovers, published from `AppleCLCD::start` in
`iokit/Kernel/arm/SweetFramebuffer.cpp` — the service the reference adds in
`iokit/Kernel/MiniIOMobileFramebuffer.cpp:1934-1971`.  It is always published
and carries no Metal properties; with boot-arg `jtmetal=1` it names
`MetalPluginName` = `JackpotlteMetal` and `MetalPluginClassName` =
`JackpotlteMetalDevice`, the two keys Metal reads before it loads a plug-in.
The gate stays off by default because a device that cannot draw yet makes an
asking program assert and CoreAnimation would pick Metal for the screen.
Checked without a device: the patch applies clean to a green run's
post-`0154` `xnu-buildtree` and `tools/xnu-cc.py` compiles
`SweetFramebuffer.cpp` with that run's own command (`0 of 1 files failed`).
Boot check: with `jtmetal=1`, `the accelerator names JackpotlteMetal as its
Metal plug-in`; without it, nothing.  The draft
`docs/drafts/jackpotlte-mali/0138-jackpotlte-ioacceleratores.draft` is
superseded and now says so.

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

**MERGED TO `main` AS `0140`** (owner instruction, 2026-11-08 21:26). The
draft lived at `docs/drafts/jackpotlte-cpus8/0146-...patch.draft` because
`tools/validate-repo.py` demands `xnu/patches/` be contiguous from 0001 and
main had nothing at 0140, so a 0146-only patch would have left a gap. The
owner asked for the 8-core fix on main directly, so it was promoted as
`xnu/patches/0140-jackpotlte-a73-cpu-personality.patch` (identical hunk,
`patch -p1 --dry-run` clean against main's 0139 tree, validator reports
140 patches 0001-0140 contiguous). The draft file was deleted.

Consequence for the six subsystem tracks, which previously sat at
0140-0145: they now have to renumber to **0141-0146** when they merge
(mechanical `git mv` plus doc references), because 0140 is taken.

Branch history: for its CI verdict it lived on branch `jackpotlte-cpus8` as
0146 (`a6a47f6`), run `42519017042` **succeeded** in 9m39s. Artifact
`/tmp/opencode/art0146/kernel.development.a5xelte`, 16,553,800 B, sha256
`8782aa4e06cf50e43a5332dfd4aba6de93a0301ce3fb8a70667b9760c5f4e664`. The
personality table inside that binary reads
`'IONameMatch' = ('arm,cortex-a53', 'arm,cortex-a73')` for `SweetCPU`, so
the fix is linked in, not just applied to source. Image
`/var/tmp/a8-xnu-jt20-cpus8.img` (jt17 cmdline with `cpus=8`), 17,485,840 B,
sha256 `129bd8eabae57464a8bf870c1491669c789cba31187592a92258625edcba0e8e`.
Built from the branch kernel; not flashed. A main-built kernel with 0140
should be rebuilt from the main CI run before flashing.

**`0140` main build is green** (`42520231344`, `b2b8f0f`): the main-built
kernel `/tmp/opencode/art0140/kernel.development.a5xelte`, 16,553,800 B,
sha256 `930af8d2ee9342dbd809a4188e8df1e98a22025db93eca02067c3151bc435157`,
contains `'IONameMatch' = ('arm,cortex-a53', 'arm,cortex-a73')` for
`SweetCPU`, so the merged patch is really compiled in. From it:
`/var/tmp/a8-xnu-jt21-cpus8.img`, 17,485,840 B, sha256
`17dd819c03dbf261d5f5b6406dd22c06906f60d858bf3e8d89d6359b6a75c946` -- 8
cores, same quiet-log cmdline as jt19 (`sweetfb_noconsole=1`). Not flashed;
jt19 (6 cores) is what is on the phone.

**No progress bar is expected on this port.** No patch in `xnu/patches/`
draws a boot progress bar or reports boot progress; the Apple logo is drawn
by iOS userland through the port's framebuffer. Logo-without-bar therefore
means launchd is not reporting progress increments -- normal when migration
is skipped or there is nothing to migrate -- and is neither a fault nor
evidence of a completed boot. The DataMigration tweaks only make
`com.apple.migration.plist` look already-migrated (`LastSystemVersion`
20H392, `DMUserDataDisposition=SyntheticDidUpgrade`,
`DMLastMigrationResults.success=true`); they do not make SpringBoard launch.

**`0141` AKS log quieting is green** (`42521342616`, `6a953e7`): main-built
kernel sha256
`127fb6ed1decf60879834b092ef64ce4d7f03455d56ae7efa07baf037c2049c9`, and the
gate string `per-selector logging enabled` is present in it. From it:
`/var/tmp/a8-xnu-jt22-cpus6.img`, 17,485,840 B, sha256
`5d232b76dbd6ca3f281687bc9b25a4c7d67fca9c63d007d0a11e43563c62b5bd` --
6 cores, jt19's cmdline (quiet display), AKS per-selector logging now off by
default. Not flashed; jt19 is on the phone.

Rationale: `SweetKeyStore.cpp` logged one line **per keystore selector call**
unconditionally, which floods the 4 MiB persistent console ring that patch
0047 keeps in the last 4 MiB of userdata. Failures still log always;
successful calls need `sweetakslog=1`.

**Observed boot state at 21:4x (owner, screen only):** Apple logo, no bar,
no lock screen, no SpringBoard. After everything else quiesces the only
repeating `exec` lines (patch 0042 prints `p->p_name`) are **`xpcproxy`,
`bluetoothd` and `fairplayd.H2`**; `driverkitd` crashes but is non-fatal
(`_PanicOnCrasX`). Note `exec xpcproxy` names the *trampoline*, not the XPC
service it is starting, so that loop is unattributed in the log -- patch 0042
would need to print the exec'd argv / the reaped child's exit status to name
it. `bluetoothd` has no working controller because the BT transport is only
the opt-in probe on branch `jackpotlte-bt`, not on main. Investigation
delegated to a subagent (branch `jackpotlte-xpcproxy`, doc
`docs/research/jackpotlte-xpcproxy-loop.md`), including how sibling ports
(SWEET/Sandcastle, georgev-6974/s9plus-ios, lechaukha12/xnu-xzs, HakiOS)
handled the same loops and what their root tweak lists contain.

Subsystem track numbering, for whoever merges them next:

| track | branch | patch on branch | merged as |
| --- | --- | --- | --- |
| flashlight | `jackpotlte-flashlight` `702718d` | 0144 | **0142 (merged `052a4b9`)** |
| sensors | `jackpotlte-sensors` `0fed091` | 0143 | **0143 (merged)** |
| BT | `jackpotlte-bt` `abd8999` | 0140 | 0144 |
| audio | `jackpotlte-audio` `52e8c61` | 0141 | 0145 |
| camera | `jackpotlte-camera` `77704d1` | 0142 | 0146 |
| mic | `jackpotlte-mic` `6b4e5bb` | 0145 | 0147 |

Flashlight was merged first (owner instruction) because it is the smallest and
most isolated of the six: one new file plus the two shared wiring files. Its
patch needed no regeneration, only renumbering, because nothing on main since
0139 touches `bsd/conf/files` or `iokit/bsddev/IOKitBSDInit.cpp`.

Sensors merged next as `0143` (its own branch number: main was at 0142). Unlike
flashlight it *did* need its three wiring hunks re-anchored, because `0142` put
the flashlight entry between the anchors sensors was cut against (`wifi` was no
longer adjacent to `pmic` / `#endif`). The new-file hunk was untouched; the
probe still sits between `scsc` and `wifi`, and the flashlight lines survive.
Verified clean (`patch --dry-run` and `git apply --check`) against a tree
reconstructed to the exact post-0142 state.

Each track's own `loader` CI red is only the gap error
(`expected 0140, found 014N`); it clears once they land in order on the new
numbering.

**But they do not stack as-is (tested 2026-11-08 21:30).** Applying all six in
order to a true `0139` tree (reconstructed by reverse-applying the BT patch to
`bt-base`; note `/tmp/opencode/sns-base` is *not* a `0139` baseline -- it
already carries the sensors probe -- and `/tmp/opencode/verify0138` is at 0138,
both giving false failures. All of `/tmp/opencode` was later wiped by a host
restart, so these trees no longer exist):

| order | patch | result |
| --- | --- | --- |
| 1 | BT 0140 | applies with fuzz 2; `IOKitBSDInit.cpp` hunk #2 FAILS |
| 2 | audio 0141 | clean |
| 3 | camera 0142 | all 5 hunks FAIL |
| 4 | sensors 0143 | clean |
| 5 | flashlight 0144 | all 5 hunks FAIL |
| 6 | mic 0145 | all 5 hunks FAIL |

Only 3 of 6 entries reach `bsd/conf/files`. Cause: all six append at the same
two spots in `bsd/conf/files` and `iokit/bsddev/IOKitBSDInit.cpp`, and each
patch was generated against the bare 0139 tree rather than against its
predecessors. `dxb` applies them with `patch -p1`, so these are genuine
failures. **The merge is not a `git merge`; each patch must be re-anchored (or
regenerated) on top of the accumulated tree and rebuilt in CI, one at a time.**
Flashlight (`0142`) needed only renumbering; sensors (`0143`) needed its three
wiring hunks re-anchored. The same holds for the remaining four (BT, audio,
camera, mic), which still have to go through the next free number in turn.

**`0142` (flashlight) is merged and green** (`42522434510` xnu and
`42522434566` loader both **success** on `052a4b9` -- the first green `loader`
run since the 0139->0146 numbering gap appeared, because the sequence is now
genuinely contiguous). Main-built kernel sha256
`8e1e00aec9e4806cab35264e1256647330433ddd38d2a42175284eadf56e7eed`; the
probe is linked in (strings `_a5xelte_flashlight_probe`, `A5XELTE FLASH:
KTD2692 s-wire on gpg1-%u ...`). From it:
`/var/tmp/a8-xnu-jt23-cpus6.img`, 17,485,840 B, sha256
`e1280e223bbaff35f3dbf819342158903da3f5c003ce51a255ea0f4539d16c8f` -- 6 cores,
quiet display + quiet AKS, flashlight probe present. Not flashed.

**`0143` (sensors) is merged and green** (`42567284722` xnu and
`42567284615` loader both **success** on `105b680`). Main-built kernel sha256
`e43ad49d869314651f09f76fd4e1388b5220360972aaebe00d063a6d49d3b00f`; the probe is
linked in (strings `A8SENSORS: probe (a8sensors=1, read only)`, `A8SENSORS:
hall gpa0-...`, `A8SENSORS: grip i2c5@...`). From it:
`/var/tmp/a8-xnu-jt24-cpus6.img`, sha256
`7342f5a3634e61307f89e8ab990315301c52170cc24edf50cbff04559e90a9fb` -- 6 cores,
quiet display + quiet AKS, flashlight + sensors probes present. Not flashed.

**`/var/tmp/a8-xnu-jt25-cpus8.img`** -- the 8-core image built on request from
the *current* main kernel (`e43ad49d`, so it carries `0140` A73 match plus
`0141`-`0143`), cmdline `... cpus=8 ...`, sha256
`d3444ad505abe55132f75b781118d16cac9ed735c96a58e96545d9acf6f385a5`. Confirmed
from the binary that `SweetCPU` matches `('arm,cortex-a53','arm,cortex-a73')`.
This supersedes `jt21-cpus8` (built from the older `930af8d2`, before
`0141`-`0143`). **Flashed 2026-11-09 to `BOOT` (`fastboot flash BOOT`, RC=0,
`Sending OKAY` / `Writing OKAY`) and rebooted.**

**RESULT: `jt25-cpus8` does NOT panic with 8 CPUs -- owner statement, 2026-11-09.**
This is the first all-8-core boot on this
unit; the `0140` A73 SweetCPU match fixed the earlier `cpus=8` panic that
`jt12`/`jt14`/`jt16` hit. Evidence level: **owner report, no photo/log yet** --
it establishes "no panic with the A73 pair enabled", not yet how far userland
gets. Worth a photo to promote it, and worth re-running the same image with
`cpus=6` vs `cpus=8` to confirm the A73 pair is actually scheduled rather than
just tolerated.

**RESULT (continued): `jt25-cpus8` reaches the SpringBoard lock screen --
owner-observed screen output, 2026-11-09.** After roughly 20+ minutes on this
image the phone shows the iOS **lock screen**: DataMigration ran to completion
(the long first-boot pass, not a hang) and SpringBoard's UI is up. This is the
first time this port has gone past the Apple logo + progress bar to real
SpringBoard UI. Evidence level: **owner report of the on-screen result, no
photo yet**; a photo would promote it to the same bar as every other stage.
Practical takeaways: (1) the long post-logo wait is DataMigration, so an image
should be left 20-40 min before it is called hung; (2) the earlier
`xpcproxy`/`bluetoothd`/`fairplayd.H2` verdict stands -- those were launchd
noise and the real gate was DataMigration/SpringBoard, not a daemon; (3)
`-noprogress` suppresses the graphical progress indicator, so a boot that looks
idle on the logo can still be migrating.

**Policy (owner, 2026-11-09): `cpus=8` is now the default for new kernel
images.** The earlier owner guidance to run 6 cores for now is superseded. All
subsequent images built here (the BT/audio/camera/mic merges and anything
after) use the `cpus=8` cmdline unless the owner says otherwise; `jt24-cpus6`
stays as the last 6-core reference.

**Baseband feasibility (subagent "Beam", 2026-11-09, branch
`jackpotlte-baseband`, local only; doc `docs/research/jackpotlte-baseband.md`).**
Modem is an integrated **Shannon 327 ("S327AP")** in Exynos 7885, driven over
**shared memory / SIPC 5.0** (`LINKDEV_SHMEM`), not PCIe/HSIC/USB; CP buffer
`0xF5400000 + 0x8800000` and CP RAM log `0xFDC00000 + 0x2000000` per the vendor
`exynos7885-rmem.dtsi`, mailbox `mcu_ipc@0x12080000` (SPI 55). Firmware is
Samsung-signed and verified in TrustZone/TIMA (`exynos_smc`, `0x82000700`), so
only the stock blob loads. Tier verdict: **T0 read-only map/readback viable
(days); T1 raw AP<->modem comms realistic (4-10 weeks); T2 iOS
CommCenter/basebandd driving the Samsung modem effectively out of reach; T3
real telephony + Apple activation not possible** (needs SEP/AKS Apple identity
and a radio-certifiable IMEI). No Android-to-iOS port anywhere has shipped
cellular. Cheapest first step is a read-only `a5basebandprobe` (GPT survey +
PMU/mailbox/CP-log reads), which also needs the T0 boot.

**Branch cleanup (owner instruction, 2026-11-09).** Deleted after verifying
their content is on `main` (renumbered/re-anchored, so not git ancestors):
`a8-keys` `aae20be`, `a8-panel` `84fdb71`, `a8-pmic` `79ff634`, `a8-touch`
`326ba70`, `a8-usb` `1539fcb` (all -> `0128`-`0133`), `jackpotlte-cpus8`
`a6a47f6` (-> `0140`), `jackpotlte-flashlight` `702718d` (-> `0142`),
`jackpotlte-sensors` `0fed091` (-> `0143`). Local and remote. Undo: the tip
SHAs above; objects remain in reflog for a while. `wifi-port` (detached
`c55c7bb`) is already an ancestor of `main` and was left in place.

**Subagent loss (server restart, 2026-11-09 06:15).** The xpcproxy
investigation subagent was killed by a host restart before it wrote anything:
worktree `/tmp/opencode/wt-xpcproxy` was clean, no doc, nothing pushed. It has
been relaunched on the same session, reset to `origin/main`, with the added
requirement to survey how sibling ports handled the same loops.

**Verdict (subagent, 2026-11-09, branch `jackpotlte-xpcproxy` `51f9689`,
doc `docs/research/jackpotlte-xpcproxy-loop.md`, 694 lines).** The three
loopers are almost certainly **not** the blocker:

- `xpcproxy` is launchd's trampoline, so one *successful* launch prints both
  `exec xpcproxy` and `exec <program>`. Most of the lines are just `bluetoothd`
  / `fairplayd.H2` restarting. A **bare** `exec xpcproxy` (target never exec'd)
  is the only interesting case and is structurally unattributable in the
  current log.
- `bluetoothd` loops because its job is `KeepAlive={SuccessfulExit=False}` and
  main has **no BT transport** (that is only on branch `jackpotlte-bt`).
- `fairplayd.H2` loops because its job is `KeepAlive=true` (unconditional) and
  it wants the IOKit user client `com_apple_driver_FairPlayIOKitUserClient`,
  which this kernel lacks.
- `driverkitd` is `RunAtLoad` with no `KeepAlive`, so it crashes once and stays
  down (non-fatal thanks to the `_PanicOnCrasX` rename).

**Most likely real blocker: SpringBoard's own launch**, not the daemons --
either its spawn never completes (which would make the bare `xpcproxy` lines
*be* SpringBoard, at its 5 s `ThrottleInterval`) or it execs once and stalls in
the backboardd system-shell check-in. Two cheap greps of the existing log
separate the cases. Siblings (`georgev-6974/s9plus-ios` via `disabled.plist`,
`lechaukha12/xnu-xzs` via a 25-job whitelist) all treated the pair as launchd
noise, never as the blocker. A draft exec-logging patch is on the branch at
`docs/drafts/jackpotlte-xpcproxy/0144-sweet-exec-argv-exit-log.patch.draft`
(kept out of `xnu/patches/` so it does not collide with BT's `0144`); it has
only had `patch -p1 --dry-run`, no compile or CI.

**Subagent model policy (owner, 2026-11-09).** All subagents now default to
`ollama-cloud/beam` via the `agents` block in
`~/.config/opencode/opencode.json` (`general` and `explore`). Environment note:
that provider resolves its key from `OLLAMA_API_KEY`.

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
for the logo.** Plain `jt17-cpus6` **does draw the Apple logo properly**. The
earlier raw log output before the Apple logo was simply the early-boot
console phase before userland's `BootProgress` took the panel, not a
suppressed UI. So `-noprogress` is confirmed inert on this kernel by
observation, matching `pe_init.c:287`. The jt18 "Assertion failed" therefore
was **not** a cmdline effect and remains unexplained; it did not recur on
jt17.

**`jt19` is now the active image** (flashed 2026-11-08 ~21:35 on the owner's
request to stop kernel logs painting over the Apple logo). `jt17`'s cmdline
plus `sweetfb_noconsole=1`, nothing else. Once iOS presents its first frame
`sweet_console_muted` is set, so console text stops being drawn over the GUI
while still reaching the persistent log, and panics still draw
(`serial_console.c:296`). `/var/tmp/a8-xnu-jt19-cpus6-ui2.img`,
17,485,840 B, sha256
`a8fbf0073aa424fb51038cb1605f8882bdfdbf30c8696872e4f5b8161967865a`.

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

### Touch debug: power-up + full event log (`0130`/`0131` regenerated, 2026-11-09)

Touch is the gate now that the lock screen is reached: the A8 panel shows the
iOS lock screen but there is **no touch input**, so Settings can't be opened.

Photo evidence (owner screenshot `Screenshot_20261109-135848.png`, 2026-11-09)
from the `jt25-cpus8` boot shows the root cause:

```
A8 TOUCH: tsp_ldo_en gpg3-0 1800000 uV dat=0 -> NOT enabled by S-Boot
A8 TOUCH: gpg3 con=0x10 dat=0x00 pud=0x111111 id(gpg3-6)=0; gpa0 dat=0x5a irq(gpa0-0)=0
A8 TOUCH: i2c4@0x13870000 con=0x6 stat=0 add=0 lc=0 (read only)
A8 TOUCH: product id read failed stat=0xf0 con=0x3e3
```

So S-Boot leaves the fixed 1.8 V touch rail `tsp_ldo_en` (gpg3-0,
`regulator-fixed`/`enable-active-high`, DTS lines 611-619) **off**, the sec_ts
chip (slave `0x48` on I2C4 `0x13870000`) is unpowered, and every I2C read NACKs
(`stat=0xf0`). That read-only probe is the last touch result on the phone.

The two touch patches were **regenerated in place** (no new patch number, so
nothing downstream renumbers) to add a power bring-up and full event logging,
both boot-arg gated so a normal boot is unchanged:

- `0130` (unchanged probe + new opt-in `jackpotlte_touch_power_up()`): with
  **`a8touchpwr=1`** it drives gpg3-0 high (`CON` func 1 + `DAT` bit0) and, if
  the I2C4 block clock is gated, sets `QCH_CON_LBLK_PERIC_QCH_I2C4` (PERI CMU
  `0x10010000 + 0x3028`, bit0 ENABLE | bit1 CLOCK_REQ; Eureka
  `drivers/soc/samsung/cal-if/exynos7885/cmucal-sfr.c:978,3753-3755`). It writes
  only that GPIO and that clock gate — no PMIC, no touch NVM. The read-only
  probe still runs and logs the rail/gate before and after.
- `0131` (contact path): with **`a8touchlog=1`** the worker now **always
  starts** (it used to stop when the controller was not in app mode) and logs
  every read raw and decoded — `A8 TOUCH: read <8 bytes> left=N`,
  `A8 TOUCH: event ... eid/tid/act`, `A8 TOUCH: press/move/release id= raw= panel=`,
  plus `gpa0-0` IRQ transitions and a ~32 s idle heartbeat. With `a8touchpwr=1`
  it also re-checks boot status and product id after the power-up.

Both patches still apply cleanly to the pristine donor `a5xelte_touch.c`
(reproduce the intended file exactly) and `tools/validate-repo.py` passes
(143 patches, contiguous).

Build + flash status (2026-11-09): CI `42624666857` (`xnu`, `main`)
**success**, kernel `kernel.development.a5xelte` 16,554,072 B sha256
`8a0cc95ce68ded7ca31a64f94751c1238eca4390004f9c8afb52c001331e9bca` (the first
build attempt `42622932947` failed: `jackpotlte_touch_power_up` was called by
`0131`'s contact path before its definition; fixed by a static prototype in
`0130`, `38c212c`). The kernel carries the new strings `A8 TOUCH PWR: ldo
before`, `A8 TOUCH PWR: i2c4 qch`, `A8 TOUCH: contact worker started (debug log
on)`, `A8 TOUCH: read ... left=`, `A8 TOUCH: %s id=... raw=(...)
panel=(...)`, and the `a8touchpwr`/`a8touchlog` boot args.

Image **`/var/tmp/a8-xnu-jt26-cpus8-touch.img`** (17,475,600 B, sha256
`1b2b78c524238fa226793d0ed44fc3574edae36d4c1248c964469c26347f96f5`), cmdline
`debug=0xc serial=3 cpus=8 keepsyms=1 rd=emmcroot0 emmcroot=USERDATA
emmcwrite=1 unentitled_ios_sim_launch=1 cs_enforcement_disable=1
launchd_unsecure_cache=1 ignition_level=1 a8touchlog=1 a8touchpwr=1
-enable_kprintf_spam -noprogress sweetfb_noconsole=1` (verified inside the
image). **Flashed to `BOOT` 2026-11-09 (`fastboot flash BOOT`, product
`jackpotlte`, `is-userspace: yes`, RC=0, `Sending OKAY` / `Writing OKAY`) and
rebooted.**

**RESULT: `jt26` brings the rail and clock gate up, but I2C4 still NACKs —
owner photo `Screenshot_20261109-143446.png`, 2026-11-09.** On-screen:

```
A8 TOUCH: tsp_ldo_en(gpg3-0) 1800000 uV dat=0 -> NOT enabled by S-Boot
A8 TOUCH PWR: ldo before con=0x10 dat=0x00
A8 TOUCH PWR: ldo on con=0x11 dat=0x01          <- rail now driven high
A8 TOUCH PWR: i2c4 qch 0x300082 -> 0x300083     <- clock gate opened
A8 TOUCH: i2c4@0x13870000 con=0x6 stat=0 add=0 lc=0
A8 TOUCH: product id read failed stat=0xf0 con=0x3e3
```

So the power-up works at the pin level, but the controller still cannot read the
sec_ts and `stat=0xf0` (MASTER_TX | START stuck) means the slave never ACKed.
**Likely cause: the I2C4 bus pads are never muxed.** `i2c_4` in
`exynos7885.dtsi:1070` has `pinctrl-0 = <&i2c4_bus>`; that group
(`exynos7885-pinctrl.dtsi:769`) is **`gpp4-1`/`gpp4-0`, function 2, pull-up**,
in `pinctrl_3 @ 0x139B0000`, bank offset **`0x0a0`** (`pinctrl-exynos.c:1737`,
`bank_type_6` CON/DAT/PUD = `0x00/0x04/0x08`). S-Boot has no reason to leave
those pads on the I2C function, and nothing in the probe muxes them, so the
wires float. The vendor driver relies on the i2c controller to apply
`i2c4_bus`.

**`jt27` (patch `0130`) now muxes the pads inside the `a8touchpwr` path**,
after the rail and clock gate and before the first I2C transfer: `gpp4` CON
pins 0,1 -> function 2 (`(con & ~0xff) | 0x22`) and PUD -> pull-up
(`(pud & ~0xff) | 0x33`), then the existing 70 ms settle. It logs
`A8 TOUCH PWR: gpp4 i2c4 pads con .. -> .. pud .. -> ..`. That is the only
new hardware write; the vendor skips its 70 ms settle only because
`regulator_boot_on` is set, so the delay stays after enabling the rail.

CI `42628632064` (attempt 2; attempt 1 died on GitHub's macOS-runner capacity,
"job was not acquired by Runner of type hosted", not on this change) built that
kernel green; the artifact was checked locally (`strings` shows
`A8 TOUCH PWR: gpp4 i2c4 pads`). **`/var/tmp/a8-xnu-jt27-cpus8-touchmux.img`**,
17,475,600 B, sha256
`23057c05dd73c0530f74f4f986b08fc640fe944c2a8e8aca713b9c02b8ca4941`, cmdline
`cpus=8 ... a8touchlog=1 a8touchpwr=1` (verified in the image), **flashed to
`BOOT` and rebooted 2026-11-09 (RC=0)**. Device result pending: look for the
`gpp4 i2c4 pads` line, then a product id, then `press/move/release`.

The remaining `read-only`/`writes nothing` boilerplate was also stripped from the
probe patches and their on-screen lines (the PMIC line is now
`JACKPOTLTE PMIC: probe (fuel gauge/charger state)`).

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

Current state: **stage 8 reached** — `jt25-cpus8` (8 cores) boots past
DataMigration to the **SpringBoard lock screen** (owner-observed 2026-11-09).
The blocker is now **touch** (dead), so the next image brings the rail up and
logs every event. `0137` (one logical cluster) and `0138`/`0139` (WiFi
transport/glue, gated by `a8wifi=1`) are merged to `main`. The donor root
tweaks were never applied to the flashed image (see "The flashed iOS root has
none of the donor tweaks").

1. **Touch (current blocker):** the IC answers (`id after power acb661`) but
   the boot-status read is flaky, so the driver was sending no
   `SET_TOUCHFUNCTION`/`SENSE_ON` and could drop movement events. `0147` adds
   the S8's sequence (sent whenever the IC answers) and 10x read retries, and
   logs `A8 TOUCH: moves=%u i2c_retries=%u lost_reads=%u`. The build is green
   (`42685239062`) and the kernel carries every new string; the image is
   **packed and ready** — `/var/tmp/a8-xnu-jt33-touchretry.img` (sha256
   `b349c7e2...446764c9d`), no `sweetfb_noconsole=1`. **Flashed to `BOOT` and
   rebooted 21:00:20** (`/tmp/opencode/touch/flash-jt33.log`); that boot
   logged **no `A8 TOUCH` line at all**, which the probe's single-attempt
   product-id read explains (see "`0148` — the jt33 boot logged nothing at
   all"). `0148` retries it, recovers the bus and starts the contact path even
   when the id never reads; build/flash it next and read the unconditional
   counters line.
2. **8 cores:** flash **`/var/tmp/a8-xnu-jt14-cpus8.img`** (sha256
   `aeb3a979...e427a9`, kernel = `0137`) to `BOOT` and look for
   `requested=8 selected=8` with no ticket-lock panic. If it still panics, the
   `jt13` image on disk (`0136` kernel) is the control for the single-cluster
   change. BOOT-only flash; capture a **serial** log, not a photo, to see the
   MPIDRs attempted.
3. **WiFi (compile green, never booted):** CI `42512239378` built
   `0138`+`0139` **success** (Build, stub and LSE gates passed). Boot that
   kernel **without** `a8wifi` and confirm only the
   `A5XELTE SCSC: PMU ...` identity lines appear; then try `a8wifi=1`.
   `/var/tmp/a8-xnu-jt15-cpus8.img` (sha256 `914edae8...c5991a`) is that
   kernel at `cpus=8`.
4. **iOS root (DataMigration gate):** host side **done** — applied and
   verified in `/var/tmp/IOSRoot.img` (see "APPLIED to the host image").
   Remaining: `img2simg /var/tmp/IOSRoot.img /var/tmp/IOSRoot.sparse.img`,
   then `fastboot flash USERDATA` — destructive, needs the owner's
   confirmation. This is the host-side fix for the endless progress bar; no
   kernel change.
5. **DRAM map (for "all 4 GB"):** `shim/fdt.c`'s `fdt_memory_all()` enumerates
   every `/memory` node and `dt_probe()` prints each bank plus the total. Source
   banks (Eureka `exynos7885-jackpotlte_common.dtsi:28-41`): low
   `0x80000000+0x3DA00000` (986 MiB), mid `0xC0000000+0x40000000` (1024 MiB),
   high `0x880000000+0x40000000` (1024 MiB) ≈ 3034 MiB. XNU takes one
   contiguous window (`arm_init.c` → `gPhysBase`/`gPhysSize`; ADT
   `DRAM_BASE=0x880000000`/`DRAM_SIZE=0x20000000` today), so widening it to
   fill the largest contiguous bank is the follow-up.
6. **Ports on the phone:** the five component ports are merged onto `main`
   (`0128`–`0133`). Their drivers are log-only or read-only by default, so a
   boot should print the USB / PMIC / touch / buttons / panel state — the first
   device evidence for them.
7. **GPU:** `docs/jackpotlte-mali.md` M0 is the read-only GPU probe (the single
   biggest unknown is whether S-Boot leaves G3D powered/clocked).
8. Older housekeeping (still open): read the remaining board unknowns (ramoops
   `log_pstore` offset, gpa1 bank offset, eMMC CIU timings) from the live DT /
   Samsung source and fill `board.h`, then enable `JT_HAVE_RAMOOPS`.

### GPU track started (2026-11-09, branch jackpotlte-gpu)

Branch `jackpotlte-gpu` from main @ 523beb5. First patch `0144-jackpotlte-mali-probe` is the M0 read-only Mali probe (PMU status + PLL_G3D con0 + GPU_ID if domain up), gated by `jtmaliprobe=1`. See `docs/gpu-bringup.md`. Not built/flashed yet.

## jt28: touch works at the bus level, then panics in our worker (2026-11-09)

Console photo `IMG_20261109_162807323.jpg` from `jt28`
(`/var/tmp/a8-xnu-jt28-cpus8-i2cinit.img`) shows the whole touch chain up:
`i2c4 init con=0xe3 stat=0 add=0x90 lc=0x7`, `before read gpp4 dat=0x3f sda=1
scl=1`, `sec_ts product id b56661`, `id after power acb661`, `status after
power=0x20 (app mode)`, then coordinates decoded and delivered —
`A8 TOUCH: release id=1 raw=(1744,104x) panel=(467,490)` and matching
press/move lines — through `sweet_hid_touch()` (patch `0058`). So the IC is
initialised and real touches reach SweetHIDTouch.

Immediately after, the console photo `IMG_20261109_163110878.jpg` shows a
`Kernel data abort`: `pc 0xfffffff00747bdac`, `lr 0xfffffff00747be08`. Both
symbolicate into **our own code**, `_jackpotlte_touch_thread` (+0x28c /
+0x2e8), at `ldurb w8,[x23,#-0x3]` — the head of the `for (i = 0; i <= left;
i++) { ... buf[i*8] }` loop. (The separate `handshake structure not
initialized` line is from the SCSC/WiFi patches `0138`/`0139`, not touch.)

**Root cause.** `left = buf[7] & 0x3f` (the number of further events the IC
still holds, `sec_ts.h:604`) was used unclamped as the loop bound. The 32-byte
stack `buf` only holds the first event plus the two the drain read can fetch
(one I2C read is capped at 16 bytes), but the loop ran to the raw `left`, so a
burst of more than two events walked the index off `buf` and off the top of
the kernel stack into unmapped memory.

**Fix `jt29` (patch `0131` regenerated).** Mirrors the native
`sec_ts_read_event()` (`sec_ts.c:937`): process only the events actually read
(1–2 here); the rest stay queued and are read one per poll when the IRQ is
next low, so none are lost. When `left > MAX_EVENT_COUNT-1` (the IC's own
corruption guard, `sec_ts.c:995`) send `CLEAR_EVENT_STACK` (`0x62`) and skip.
Adds `jtts_write_cmd()` — the controller twin of
`sec_ts_i2c_write(ts, cmd, NULL, 0)` — and sends `SENSE_ON` (`0x10`) once at
startup when the IC reports app mode and again on the watchdog status event
(`stype=INFO`, `status_id=0x00`, `data1=0x20`), releasing every finger first.
Committed to `main` as `d2ef8d5`; `tools/validate-repo.py` passes and the new
`0131` reproduces the edited tree exactly under `git apply`.

**Built green:** CI `42646809859` (main @ `d2ef8d5`). Kernel
`kernel.development.a5xelte`, 16,554,272 B, sha256
`5473605ce4020ba0cffebf1b6cbf4a4bfa558bf938a6770f6ce6b95deed43ade`; the
`xnu-kernel` artifact carries `A8 TOUCH: contact worker started`, `sense-on
sent`, `event queue overflow`, `watchdog reset, re-sense` and `IC event queue
full`, so the new worker is in the binary (not just the patch tree), and the
Mali strings are still there too. Packed to
**`/var/tmp/a8-xnu-jt29-cpus8-touchfix.img`**, 17,475,600 B, sha256
`2c3a70f862b9469c03e7e20dce04d205816f47b0f939b5968ac092a1f19473ea`, cmdline
the `jt28` base (cpus=8, `a8touchlog=1 a8touchpwr=1`, no GPU flags).
**Not flashed yet**, so the jt28 panic has not been re-run on the phone.

**RESULT: the panic is gone.** `jt29` was flashed to `BOOT`
(sha256 `2c3a70f8...`) and its console (photo
`IMG_20261109_165700853.jpg`) shows press/move decoded and delivered —
`A8 TOUCH: press id=0 raw=(1227,2689) panel=(323,1260)` then
`SWEET HID: touch 0 down at 323,1260` — with no `Kernel data abort`.
The decisive proof the drain fix works: the queue reports `left=7` and the
worker then reads `left=6,5,4,3,2,1`, one event per poll. The old code raised
the loop bound to `left` and would have walked off `buf` at `left=7`; now it
processes one event and leaves the rest queued.

Remaining touch noise (not fatal): occasionally `event read failed (1)
stat=0x30` on a tap with `gpp4 dat ... sda=0 scl=1` — the SDA line is held
low between transfers, and the existing `jtts_bus_recover()` bit-bang clears
it (`recovered ... sda=1 scl=1`) before the read succeeds. `stat=0x11` (NACK)
also appears once and is absorbed by the retry. Cost is one ~20 ms backoff +
recovery per stuck tap; the follow-up is to fix the STOP/bus-release in
`jtts_stop`/`jtts_rx` or retry faster instead of the 20 ms wait.

**iOS does not react to the delivered touches (open, 2026-11-09).** The owner
reports the lock screen is up but touch "isn't seen" even though the kernel
logs press/move/release. The drain/read path is proven above, so the question
is what happens after `sweet_hid_touch()`. The A8 compiles the
`ARM64_BOARD_CONFIG_A5XELTE` branch of `SweetHIDTouch` added for the A5 2017
(`0118`): normalise by **1080x1920** with zero offset and advertise a
**68x121 mm** surface. That matches the A5 2017 panel, not this one: the A8's
`s6e3fa7` is **1080x2220** with `size = <67 138>` (`67.176 x 138.084` mm,
`exynos7885-display-lcd.dtsi:173-177`), and `0124` leaves the A8 presenting the
**native 1080x2220** size (`SWEET FB: AppleCLCD 1080x2220 ... presented as
1080x2220`), not the donor's 1125x2436. The *normalised* coordinates are
correct (`nx = rx/4096`, `ny = ry/4096`), so taps are not simply offset; the
open suspects are (a) backboardd not opening the Sweet HID services at all and
(b) the digitizer declaring a 16:9 surface on an 18.5:9 display, plus the
missing `kIOHIDEventFieldDigitizerIsDisplayIntegrated` on the event (the
service sets a `DisplayIntegrated` property only).

Diagnostic queued without a rebuild (the jt29 kernel already has the logs):
**`/var/tmp/a8-xnu-jt29-hidtest.img`**, 17,475,600 B, sha256
`3816ca2c88b4be398ccf1afc568457b882db500b220b02a1211dea82074650e6`, same
jt29 kernel (sha256 `5473605c...`) with `sweethidtest=5` added to the cmdline
(verified: `... a8touchlog=1 a8touchpwr=1 sweethidtest=5 ...`, no GPU flags).
It makes `SweetHIDButtons` emit a fake Home press every 5 s and log the client
count: `SWEET HID: key 0x40 down (N clients)`. **`N = 0` means backboardd
never opened either Sweet HID service (a delivery/matching problem);
`N > 0` means the HID pipe works and the issue is digitizer-specific.** A
watcher (`/tmp/opencode/touch/wait-hidtest.sh`) flashes it when the phone next
appears in fastboot. Not flashed yet.

**Candidate fix prepared, not pushed (2026-11-09).**
`xnu/patches/0145-jackpotlte-touch-surface.patch` (commit `92cf41a`, local
only) gives the A8 its real `67x138 mm` digitizer surface under `JACKPOTLTE`
and appends the user-client count to the touch `IOLog`
(`... at %d,%d%s (%u clients)`), so one boot measures both the surface change
and whether backboardd opened the digitizer. Verified: applies to the
post-0144 tree (`patch -p1 --dry-run`), the result's `#if`/`#endif` balance is
0, and `validate-repo.py` reports 145 contiguous patches. It is **not pushed**
because pushing `xnu/patches/**` cancels the in-flight GPU rebuild
`42667447712`.

Note on the geometry: the driver (`0131`) emits `y = ry*1920/4096` and
`0118` divides by 1920, so `ny = ry/4096` and the full 0..1 range already
reaches the whole display. **Do not** change `fLogicalH` to 2220 on its own --
the driver still stops at 1919, so `ny = y/2220` would reach only the top 86%.
An A5XELTE that mirrors the donor (present 1125x2436, digitizer over
1125x2436 with `fOffY=108`, driver emitting 0..2219 panel pixels) needs all
three files changed together; that draft is in
`/tmp/opencode/touch/donor-like-geometry.NOTES.md` and is gated on the hidtest
client count.

**Same boot, the touch answer was inconclusive (2026-11-09).** The combined
GPU+touch image also had `sweethidtest=5`, and the console shows
`SWEET HID: key 0x40 down (0 clients)` / `key 0x40 up (0 clients)` — but those
lines are at ~5 s uptime, long before SpringBoard/backboardd start. Worse, the
test stops rescheduling after 60 events (`SweetHIDButtons::testFired`), i.e.
after ~2.5 min, which can be entirely before backboardd exists on this device
(the first DataMigration boot needed 20+ min; later boots are faster but
unmeasured). So `0` here says nothing yet.

`0146` (`xnu/patches/0146-jackpotlte-hid-client-report.patch`) makes the count
readable regardless: it adds `UInt32 clientCount()` to `IOHIDEventService` and
logs `SWEET HID: clients buttons=%u touch=%u` once per test tick, forever —
the key presses still stop at 60, but the report does not. `sweetfb_noconsole=1`
mutes the console only at the GUI's **first present**, so the last visible line
of that report is the client count at the moment backboardd began presenting.
`touch>0` there means the pipe works and the remaining fault is digitizer
geometry (go to the donor-like geometry draft above); `touch=0` means backboardd never opened
the digitizer and the fault is matching/open, not geometry.

Note the persistent console log (`0047`) records **everything**, muted or not
(`serial_console.c` calls `sweet_log_append()` before the mute check, and the
A5XELTE guard is `SWEET || A5XELTE`), so the ring at the tail of `userdata`
also has the late boot.  Reading it needs ADB in a recovery
(`tools/sweet-log-pull.sh`); the only recovery here is the stock
`~/recovery-public-v1.img`, whose ADB is not root-capable, so that route is
not yet usable on this device.

### How the other ports do sec_ts touch (research, 2026-11-09)

No public port of this exact SoC: the community list at hackiosx.dev has
Exynos in only two entries, neither an Exynos 7885 / A8 2018.  Two Exynos
ports on **our own A5XELTE base** do have sec_ts touch working, and both are
direct references:

- **Galaxy S7 `herolte` (Exynos 8890)** — `wardevsa/ios-a5xelte`, branch
  `herolte`.  It reaches the iOS setup screen and then the home screen *with
  touch*.  Touch series: `0211` (driver), `0212`/`0216` (rail via the APM M3),
  `0217` (retries).  `HEROLTE_PROGRESS.md:214-227` states the working
  condition: rails on → SDA/SCL high → **sec_ts in app mode (`0x20`),
  id `ac a5 52`, tx=15 rx=26** → touches reach iOS; then `0217` adds up to
  10 I2C retries because the IC NACKs (`int=0x400`) right after a finger
  lands and movement events were lost.
- **Galaxy S8 `dreamlte` (Exynos 8895)** — `Floppikrob/ios-galaxy-s8`.
  Reaches the lock screen; touch works via the S6SY761 on `hsi2c_23`, "a
  polled kernel driver feeds HID touch events; taps and swipes reach
  SpringBoard".

What that means for us, precisely:

- Both use the **same `sweet_hid_touch()` HID layer** we use (the S8 even
  builds as `ARM64_BOARD_CONFIG_A5XELTE`), and the S7 calls it from its own
  polled driver exactly as `0131` does.  So the HID delivery path is proven;
  `SWEET HID: touch 0 down at 323,1260` on jt29 is real.
- Our A8's controller is a **different sec_ts generation** than the S7's.
  Ours (patch `0130`/`0131`, "y661"): `READ_ONE_EVENT 0x60`,
  `READ_ALL_EVENT 0x61`, `CLEAR_EVENT_STACK 0x62`, `SENSE_ON 0x10`,
  app mode `0x20`.  The S7 S6SY761: `READ_ONE_EVENT 0x71`, `SENSE_ON 0x40`,
  app mode `0x20`.  So the S7's opcode *values* cannot be copied, only its
  structure.  **The S8 is the opposite case, and that makes it the primary
  reference:** its `0124` driver uses the *same opcode generation as ours* —
  `READ_ONE_EVENT 0x60`, `READ_ALL_EVENT 0x61`, `READ_BOOT_STATUS 0x55`,
  `READ_ID 0x52`, `SENSE_ON 0x10`, app mode `0x20` — plus the same
  `SET_TOUCHFUNCTION {0x30,0x01,0x00}` command (`sec_ts.h`), so the S8's
  working enable sequence is the one to copy, not the S7's.  The S8's copy of
  `SweetHIDTouch` (`0118`) is byte-identical to ours.
- The S7 refactored `SweetHIDTouch` to keep the native **panel** size
  (`fPanelW/H`) separate from the **logical** display size (`fLogicalW/H`,
  `fOffX/Y = (logical - panel)/2`) and to clamp against the panel
  (`0211`).  For our A8, native is `1080x2220` (`0124`) with no offset, so
  the normalised result is identical either way (both are `ry/4096`) — this
  refactor cleans up the code but is **not** the reason touch is unseen.
- The concrete difference that *is* visible on our device: the boot status
  reads fail.  The jt31 video shows `status before power=0` and
  `status read failed after power`, so `boot` never equals `APP_MODE` and
  `SENSE_ON (0x10)` is never sent (the send is gated on app mode).  jt29
  still delivered real coordinates, so the IC was scanning then, but a
  reliable app-mode read + retry (as the S7 does) is the obvious hardening.

**Where the jt31 video leaves it.** Its last frame is still kernel init
(`A8 TOUCH: contact worker started`, then the panel and `JACKPOTLTE MALI:`
probe); backboardd has not run yet, so the `SWEET HID: clients buttons=0
touch=0` lines in that video are expected and do **not** answer the
matching/open question.  That boot does sharpen the diagnosis: `A8 TOUCH:
status before power=0`, `status read failed after power`, but `id after power
acb661` — the boot-status read is what is flaky, and `SENSE_ON` (`0x10`) plus
`SET_TOUCHFUNCTION` are skipped whenever it fails.

**The client count (on-screen jt32).** `/var/tmp/a8-xnu-jt32-gputouch-onscreen.img`
(sha256 `13c88871...`) repacks the same verified jt31 kernel **without
`sweetfb_noconsole=1`**, so the periodic client report and the `SWEET HID:
touch ... (%u clients)` line keep drawing over the GUI.  Flashed to `BOOT`
and rebooted at 20:11:52 (`/tmp/opencode/gpu/flash-jt32.log`).  Reading on
that boot: `SWEET HID: clients buttons=1 touch=1` — backboardd opened **both**
Sweet HID services, so matching/open is proven and the remaining fault is the
digitizer event stream, not the service wiring.  (Owner-read; a photo is still
wanted, and the next image keeps the same on-screen logging so it is
re-confirmed.)

### `0147` — the A8 IC init, from the S8's sequence

The gap the jt31/jt32 boots expose: the boot-status read fails, so
`boot != APP_MODE` and the driver sends **nothing** to the IC (no
`SET_TOUCHFUNCTION`, no `SENSE_ON`) — the same class of miss the S7 fixed in
`0217`.  `0147` (`xnu/patches/0147-jackpotlte-touch-ic-enable-retries.patch`,
`bsd/dev/a5xelte_touch.c` only) does the two things the working references do
and our driver does not:

1. **Send `SET_TOUCHFUNCTION {0x30,0x01,0x00}` then `SENSE_ON {0x10}`** —
   the S8's sequence (`0124`) — and send them **whenever the IC answers**
   (`boot == APP_MODE || have_id`), not only on a clean app-mode read, since
   the id read (`acb661`) succeeds even when the boot-status read is lost.
   The boot-status read itself is retried 10x first.
2. **Retry a NACKing register read up to 10x** (`jtts_read_retry`, 200 us
   apart), as `sec_ts_i2c_read()` does and the S7 copied in `0217`, so a
   dropped `READ_ONE_EVENT` during a swipe does not lose finger movement.

It adds counters `moves` / `i2c_retries` / `lost_reads`, logged every ~16 s
that there is activity as `A8 TOUCH: moves=%u i2c_retries=%u lost_reads=%u`,
so one boot reports whether reads are being lost without the raw event spam;
the watchdog re-sense path sends `SET_TOUCHFUNCTION` too.  Build is `0147` on
top of the now-147-patch tree; the next image packs that kernel **without
`sweetfb_noconsole=1`** so the counters draw over the GUI.  Plan: boot it,
swipe up from the bottom of the lock screen, and read the counters — many
`moves` with low `lost_reads` and iOS still ignoring it moves the hunt to the
digitizer event/backboardd side; a failed enable write or repeated
`bus recover`/`lost_reads` points back at the bus/IC.

**Build and image (jt33).** CI `42685239062` (main @ `a2abbe7`, the `0147`
commit) completed **success** and the downloaded `xnu-kernel` artifact
(`kernel.development.a5xelte`, 16,554,512 B, sha256
`feb10b7be1459eb3a1f93bcd8fe44b19e43c6972a181819cca5215591e43c9cb`) contains
every new string — `A8 TOUCH: touch-function sent`, `sense-on sent`,
`touch-function write failed`, `i2c4 still busy on write`, `A8 TOUCH:
moves=%u i2c_retries=%u lost_reads=%u` and `SWEET HID: clients buttons=%u
touch=%u` — so `0147` applied (green is not being trusted on its own again).
The image is packed **without `sweetfb_noconsole=1`**:
`/var/tmp/a8-xnu-jt33-touchretry.img` (17,475,600 B, sha256
`b349c7e272dcefeaf12f4d414ad5aacf602ec4435c2fc9bee0fb70a446764c9d`), base
cmdline plus `sweethidtest=5 jtmaliprobe=1`.  Flash it to `BOOT`, reboot,
swipe up from the bottom of the lock screen, and read the on-screen
`A8 TOUCH: moves=... i2c_retries=... lost_reads=...` line and the
`SWEET HID: clients buttons=... touch=...` line together.

**Flashed to `BOOT` and rebooted at 21:00:20** (`/tmp/opencode/touch/flash-jt33.log`),
the phone's fastboot serial (recorded locally only, not in this file).  Result
pending: a swipe up from the lock screen
and a photo of the on-screen counters.

### `0148` — the jt33 boot logged nothing at all

The jt33 boot produced **no `A8 TOUCH` output of any kind**, not even the
periodic counters.  The log is not off: `strings` on the flashed image shows
`a8touchlog=1` in the cmdline and the kernel carries every `A8 TOUCH:`
string.  The reason is upstream of the counters: `jackpotlte_touch_probe()`
reads the product id **once** (`jtts_read_reg`, no retry) and **returns** if
that read fails, and the `jackpotlte_touch_contacts()` call that starts the
worker sits *after* it.  So one NACK on the very first boot read leaves the
digitizer with **no worker at all** — no heartbeat, no `read`/`press` lines,
no counters, exactly what was seen.  The read is the same flaky one jt31
showed (`status read failed after power`); `0147` hardened the contact path
but not this first probe read.

`0148` (`xnu/patches/0148-jackpotlte-touch-probe-resilient.patch`,
`bsd/dev/a5xelte_touch.c` only) makes that path survivable:

1. The probe's product-id read uses `jtts_read_retry` (10x, as `0147` added),
   then one `jtts_bus_recover()` + 2 ms and a second retry round (the failure
   signature is a stuck bus, `stat=0x30` with SDA held low).
2. If it still fails, the probe **no longer returns**: it starts the contact
   path anyway (`jackpotlte_touch_contacts(boot)`), so the worker, the enable
   sequence and the counters run regardless.
3. The contact path's own id read uses `jtts_read_retry` too.
4. The counters line prints **unconditionally every ~16 s** instead of only on
   change, so a boot always shows `A8 TOUCH: moves=... i2c_retries=...
   lost_reads=...` even with zero touch — the worker's liveness is never
   ambiguous again.

CI `42692421202` (main @ `10ebae2`) is **success** and the downloaded kernel
(16,554,552 B, sha256 `e16eeb1b...8c866f73`) carries the new strings
(`product id read failed ... recovering`, `no product id; starting the
contact path anyway`) plus the `0147` ones.  Image
`/var/tmp/a8-xnu-jt35-touchprobe.img` (17,475,600 B, sha256
`2e7057b6286fffe72086d5b10693993735280ab96c54f26c076948e5fd71690b`) is
packed without `sweetfb_noconsole=1`.  **Flashed to `BOOT` and rebooted at
21:33:25** (`/tmp/opencode/touch/flash-jt35.log`).  Read on that boot: the
unconditional `A8 TOUCH: moves=... i2c_retries=... lost_reads=...` line is
the worker-liveness proof, and a swipe's effect on `moves`/`lost_reads`
decides digitizer/iOS vs bus/IC.

### `0149` — the swipe never finishes: drain the whole queue

The jt35 boot did what `0148` promised: the counters print and touches reach
the HID layer.  The photo (`IMG_20261110_063951754.jpg`) shows the whole
chain up to iOS:

```
A8 TOUCH: press id=0 raw=(1893,4086) panel=(499,1915)
SWEET HID: touch 0 down at 499,1915 edge (1 clients)
A8 TOUCH: move id=0 raw=(1898,4084) panel=(500,1914)
A8 TOUCH: move id=0 raw=(1907,4078) panel=(502,1911)
A8 TOUCH: bus busy stat=0x30, recovering          (repeated)
A8 TOUCH: read 84 77 fe d7 09 09 1e 23 left=35
A8 TOUCH: event queue overflow left=35
A8 TOUCH: irq gpa0-0 -> high
```

Press and move decode correctly (`panel` matches the raw 12-bit value scaled),
and `sweet_hid_touch` receives them with `1 clients`.  **No `release` ever
appears**, so iOS sees a finger that never lifts and ignores the swipe.

The cause is the drain: the worker read **one** event per poll and only asked
for the rest when `1 <= left <= 2`, because `jtts_read_reg` rejected any
transfer over 16 bytes (two events).  A moving finger produces events faster
than that, the queue filled to `left=35` (past the IC's 32-deep queue, so the
count is garbage), and the overflow branch cleared the stack and **never
released** the finger — `jt_ts_down[0]` stayed true forever.  The two
`printf` per event to the framebuffer console added to the drain being too
slow.

`0149` (`xnu/patches/0149-jackpotlte-touch-drain-queue.patch`,
`bsd/dev/a5xelte_touch.c` only) makes the drain match the working S8 driver
(patch `0124`, `buf[8*32]` + one `READ_ALL_EVENT`):

1. `jtts_read_reg` accepts a full `JTTS_MAX_READ_BYTES` = `8*32` = 256-byte
   transfer, so the whole queue can come back in one read.
2. The worker's buffer grows to `buf[8*32]` and it reads **all** `left*8`
   further bytes in a single `READ_ALL_EVENT`, exactly like the S8, instead
   of one event per poll.
3. The overflow branch (`left >= 32`) calls
   `jackpotlte_touch_release_all()` before clearing the stack, so a lost
   stream can no longer leave iOS stuck on a phantom press; the IC's own
   `QUEUE_FULL` status event releases too.
4. Logging is cut to the two lines a swipe hinges on — `press` and
   `release` (`move` is counted in the `moves=` counter, not printed).  The
   raw per-event dump and the per-poll `read ... left=` line are behind
   `a8touchverbose=1`, off by default.

CI `42764449946` (main @ `574f6ea`) is **success**; the downloaded kernel
(16,554,584 B, sha256 `f19a6b94...ae63ddc1`) carries
`event queue overflow ... release all`, the `press`/`release` lines, the
counters line and `a8touchverbose`, so `0149` applied.  Image
`/var/tmp/a8-xnu-jt36-touchdrain.img` (17,475,600 B, sha256
`b38d91d43b95985e9fecd6a9bcf465f5f98f977bb60ed52bce74e95bead3a69a`) is
packed on the base cmdline (`a8touchlog=1 a8touchpwr=1 sweethidtest=5`, no
`a8touchverbose`, no `sweetfb_noconsole=1`) and **flashed to `BOOT`** and
rebooted at 17:16:12 (`/tmp/opencode/touch/flash-jt36.log`).  The swipe test:
`press` then a rising `moves=` with no overflow, then `release` — the last
line is the one iOS needs and the one jt35 never produced.

### jt36 result — touch works, dragging does not (`0150`)

On the phone (photos `IMG_20261110_173452181.jpg`, `IMG_20261110_173552290.jpg`)
**touch is alive**: taps register and the owner navigated all the way to
iOS's "Add New Wallpaper" screen.  That is the first time iOS has reacted to
this digitizer at all, and it confirms the whole chain below it — IC init,
event decode, `sweet_hid_touch`, `dispatchEvent`, a HID client.

Dragging still fails, and the log says why:

```
A8 TOUCH: press id=0 raw=(1481,2899) panel=(390,1358)
SWEET HID: touch 0 down at 390,1358 (1 clients)
... 8 irq low/high pairs (moves, counted not printed) ...
A8 TOUCH: bus busy stat=0x30, recovering        (x3, sda=0)
A8 TOUCH: event queue overflow left=38, release all
SWEET HID: touch 0 up at 387,1357 (1 clients)
A8 TOUCH: event queue overflow left=50, release all
A8 TOUCH: moves=340 i2c_retries=123 lost_reads=0
```

The moves *are* decoded (`moves=340`), but the drag ends ~3 px from where it
started because the `0149` overflow branch **forcibly releases the finger**
the moment the IC's `left_event` count saturates past the 32-deep FIFO.  iOS
then sees press + a few moves + release in place: a tap or a long press, not
a drag.  That is exactly the reported symptom (a long-press menu instead of
a scroll).  The saturation happens while the worker is stalled in the I2C
bus recoveries during userland spawn (`exec userlandmanagerd`, `FairPlayd`).

`0150` (`xnu/patches/0150-jackpotlte-touch-drain-backlog.patch`,
`bsd/dev/a5xelte_touch.c` only) stops throwing the drag away:

1. A saturated `left` (>= 32) no longer clears the queue and no longer
   releases.  It **clamps to the FIFO depth** (`left = 31`) and drains those
   events; the rest stay queued for the next poll.  The release event we
   need is normally *in* that backlog, which is exactly what the old branch
   deleted.
2. Added a lift watchdog: if a finger is down and **no** coordinate event
   has arrived for `JTTS_LIFT_POLLS` = 50 polls (~400 ms), release it.  A
   dropped release can no longer leave iOS holding a phantom finger, which
   was the jt35 bug.
3. The counters line gains `backlog=` (`jt_ts_overflows`) so a bad drain is
   visible at a glance: `A8 TOUCH: moves=%u i2c_retries=%u lost_reads=%u
   backlog=%u`.

The IC's own `ERR_EVENT_QUEUE_FULL` status event still releases, because
there the IC is explicitly saying it dropped data.

CI `42857529491` (main @ `2720e09`) is **success**; the downloaded kernel
(16,554,648 B, sha256 `fa3044fd...28f7dc4a`) carries
`event backlog ... draining`, `lift watchdog, releasing`, the `backlog=`
counters line, and the `press`/`release` lines, so `0150` applied.  Image
`/var/tmp/a8-xnu-jt37-touchbacklog.img` (17,475,600 B, sha256
`d8809f7c28a7034dd558dd4aada22ba2af867915d8205f82250871317fb61fe1`) is
packed on the same base cmdline, **flashed to `BOOT`** and rebooted at
17:59:23 (`/tmp/opencode/touch/flash-jt37.log`).  The test is a **drag**:
press and move without lifting
should keep `jt_ts_down` set until the real release, with `backlog=` showing
how many clamped drains happened.

### GPU 0144 is really in the shippable kernel

CI `42644071418` (main @ `9e2c729`, the GPU merge) completed **success**, and
the downloaded `xnu-kernel` artifact contains the Mali strings
(`JACKPOTLTE MALI: probe start`, `g3d_config=`, `power request -> status=`,
`pll_con0 before=`, `GPU_ID=`), so `0144` applied — CI-green is not being
trusted on its own again. Both images are packed in `/var/tmp/`:
`a8-xnu-gpu-probe.img` (`jtmaliprobe=1`) and `a8-xnu-gpu-pwr.img`
(`jtmaliprobe=1 jtmalipwr=1`), on the `jt28`-style base cmdline.

**Probe flashed 2026-11-09 16:48** (`flash-watch.log`): the phone
appeared in fastboot as the queued watcher started, `BOOT` was written and the
phone rebooted (probe sha256 `a556f11824251a67123ad9497810c8a61b76dc8e974cc8d0120cb4955cac8c7f`).
Device result pending: photograph the `JACKPOTLTE MALI:` block —
`g3d_config=.../status=...`, `pll_g3d_con0=`, then `GPU_ID=... arch=?` (arch 6
= Bifrost/G71) or `G3D domain off, GPU not read`.

**Rebuild after the hunk fix (2026-11-09).** CI `42667447712` (main @
`7c3e2ab`) is **success**, and the `xnu-kernel` artifact carries all four
strings (`JACKPOTLTE MALI: probe start`, `g3d_config=`, `GPU_ID=`, `PMU`), so
the file now materialises with its closing `#endif`. Kernel sha256
`b62f622da02e879859d43f96ac6d0d40978f9e89603353ba6ac04da3ad8f2f4b`, 16,554,272 B.

Images repacked from that kernel (same `jt28`-style base cmdline):
`a8-xnu-gpu-probe.img` sha256 `1787e5d0...` (`jtmaliprobe=1`) and
`a8-xnu-gpu-pwr.img` sha256 `28c233a1...` (`jtmaliprobe=1 jtmalipwr=1`).

Because the phone is only on when the owner plugs it in, a single boot is
worth more than two: a **combined** image is armed instead —
`/var/tmp/a8-xnu-gpuprobe-hidtest.img`, sha256 `4436ac72...`, cmdline
`... a8touchlog=1 a8touchpwr=1 sweethidtest=5 ... jtmaliprobe=1` (verified: no
`jtmalipwr`). One boot then answers both (a) whether the PMU-window fix stops
the M0 panic and what `g3d_config`/`status`/`GPU_ID` read, and (b) the touch
client count. Watcher `/tmp/opencode/gpu/wait-probe-hidtest.sh` (pid 64555)
flashes it once when the phone next appears in fastboot;
`/tmp/opencode/gpu/flash-probe-hidtest.log` records it. The earlier
`a8-xnu-jt29-hidtest.img` watcher (pid 58899) was **stopped**: `jt29` predates
the PMU-window fix, so that kernel carries the panicking `JT_PMU_WINDOW
0x1000`.

**RESULT on the phone: the panic is gone and the G71 is up (2026-11-09).**
The combined image (sha256 `4436ac72...`) was flashed at 19:07:06
(`/tmp/opencode/gpu/flash-probe-hidtest.log`) and its console, captured in
`~/Downloads/Screenshot_20261109-190908.png`, printed:

```
JACKPOTLTE MALI: probe start (jtmaliprobe=1)
JACKPOTLTE MALI: g3d_config=0xf status=0xf
JACKPOTLTE MALI: pll_g3d_con0=0xa1570d11 enable=1 stable=1
JACKPOTLTE MALI: GPU_ID=0x60000002 arch=0 rev=0
JACKPOTLTE MALI: present: shaders=0x3 tiller=0x1 l2=0x1
JACKPOTLTE MALI: reset rawstat=0x100 done=1
JACKPOTLTE MALI: ready shaders=0x3 tiller=0x1 l2=0x1
JACKPOTLTE MALI: M1 done, GPU up and reset
```

What that proves, each against a source:
- `g3d_config` **and** `status` read `0xf` with no `jtmalipwr`, so **S-Boot
  leaves `pd_g3d` powered**; the earlier translation fault is purely the old
  `0x1000` PMU window, and the `0x10000` window fixes it.
- `pll_g3d_con0=0xa1570d11`, bit31 (`PLL_ENABLE`) 1 and bit29 (`PLL_STABLE`) 1
  (`cmucal-sfr.c:712`, `ra.h:37-40`) — PLL_G3D locked.
- `GPU_ID=0x60000002`.  Linux panfrost splits this as `id = gpu_id >> 16`
  = **0x6000** and `revision = gpu_id & 0xffff` = 0x0002
  (`drivers/gpu/drm/panfrost/panfrost_gpu.c:316-327`), and its model table
  maps `0x6000` to **Mali-G71** (`panfrost_gpu.c:222
  GPU_MODEL(g71, 0x6000, ...)`), arch 6 = Bifrost v6 (`docs/jackpotlte-mali.md`
  sec. 4).  The patch's own `arch=%u` was computed from bits 12-15 of the
  *whole* value and printed 0; that is a log bug, fixed below, not a hardware
  result.
- `shaders=0x3` = **2 shader cores** (the A8's G71 is MP2), `tiller=0x1`,
  `l2=0x1`; after the soft reset all three `_READY` masks read back equal, so
  the cores really powered.
- Net: **M0 and M1 are done on hardware**, with no panic and no power
  sequence needed.  M2 (MMU + one no-op job) is next.

**Log/decoding fixes applied after that boot:** `0144` now prints
`id=`/`arch=`/`rev=`/`major=`/`minor=` decoded the panfrost way (new-file
hunk `+1,309`, every hunk re-counted before pushing) and says
`reset done, GPU up and cores powered` instead of "M1 done", because M1's
power path was skipped when the domain was already up.

**jt31 combined image flashed (2026-11-09 19:35).** One `main` build now
carries both tracks: GPU `0144` (with the decode fix) and touch
`0131`+`0145`+`0146`.  CI `42673299985` attempt 2 **succeeded**; the
downloaded kernel `/tmp/opencode/touch/art-jt31/kernel.development.a5xelte`,
16,554,272 B, sha256 `b5b41e35...`, was string-checked to contain **all four**
Mali strings (`probe start`, the new `GPU_ID=... id=... arch=...` line,
`reset done, GPU up and cores powered`, `g3d_config=%#x status=%#x`) **and all
three** Sweet HID strings (`clients buttons=%u touch=%u`, the touch log, the
buttons test log).  Packed to `/var/tmp/a8-xnu-jt31-gputouch.img`, 17,475,600 B,
sha256 `3b3da3f5aaf74c3b0e34b62795cfbd49800a410e39e3598d7c64e9e387ecc3b1`,
cmdline `... a8touchlog=1 a8touchpwr=1 sweethidtest=5 ... jtmaliprobe=1` (no
`jtmalipwr`).  Flashed to `BOOT` and rebooted at 19:35:27
(`/tmp/opencode/gpu/flash-jt31.log`).  The decisive line to read on the
console is the last `SWEET HID: clients buttons=N touch=N` before the GUI
takes over (`sweetfb_noconsole=1` mutes the console only at the first
present): `touch>0` → the digitizer pipe works and the remaining fault is
geometry; `touch=0` → backboardd never opened it.  Awaiting the owner's
photo/video of this boot.

### Quiet boot: `sweetfb_noconsole=1` back in the packed cmdline (`jt43`, 2026-11-10)

The GPU images `jt39`/`jt42` were packed **without** `sweetfb_noconsole=1`
(`strings` on the image), so every kernel `printf` keeps painting over the GUI
on the panel - the `exec` lines (`0042`/`0111`), the `SWEET HID`/touch
counters, the Mali lines. That is what the owner sees as "verbose logging".
`jt19` was the same situation on 2026-11-08 and the fix was this same boot arg.

Packed `/var/tmp/a8-xnu-jt43-gpum3-quiet.img` (17,518,608 B, sha256
`8a30023db26db6293930a8abb37574479f2bed12bd1098747044e19b137508dd`) from the
*same* `jt42` kernel (`a34d2968...93297f3a`, also the artifact sha) with only
the cmdline changed: the jt42 args plus `sweetfb_noconsole=1`. `cmp -l` against
`jt42` differs in **104 bytes**, all inside the payload cmdline field and its
checksums, so the kernel is byte-identical.

Mechanism (`0067`, `0109`): `sweetfb_noconsole=1` sets `sweet_console_muted`
in `serial_console.c` at **iOS's first frame**, not at boot - the early kernel
text still draws (that is where the Mali `word=0xc0ffee01` lines live), then the
Apple logo and GUI stay clean. Console text still goes to the persistent log,
and a panic still draws (`sweet_console_muted && !panic_active()`). The
`jtmali*` probe args are kept for now so that one photo can still close M2/M3;
once that is in, dropping them makes the boot quiet from the first line.

Not flashed: the phone was not in fastboot at pack time.

### Only ~7 home-screen icons: the 32 apps iOS 16 leaves in `staged_system_apps` (2026-11-10)

The owner sees about six apps on the home screen. Measured on the raw master
`/var/tmp/IOSRoot.img` (`7z l`, listing kept at `/tmp/opencode/cur-listing.txt`):

- `/Applications` holds **142** `.app` bundles, but almost all are XPC/UI
  services (`AAUIViewService`, `AuthKitUIService`, ...) with no home-screen
  icon. The real user apps there are only MobileSafari, MobileSMS,
  MobilePhone, MobileSlideShow (Photos), Preferences (Settings), Camera and
  AppStore - the icons that do appear.
- `/private/var/staged_system_apps` holds **32** more, no name overlap with
  `/Applications`: AppleTV, Books, Bridge, Calculator, Compass, Contacts,
  FaceTime, Files, FindMy, Fitness, Freeform, Health, Home, Magnifier, Maps,
  Measure, MobileCal, MobileMail, MobileNotes, MobileStore, MobileTimer,
  Music, News, Passbook, Podcasts, Reminders, SequoiaTranslator, Shortcuts,
  Stocks, Tips, VoiceMemos, Weather. They are **complete** bundles (each has
  its Mach-O executable: Maps 18 MB, Books 21 MB, Weather 10 MB, Notes 8 MB),
  487 MiB in total.
- `/System/Library/AppPlaceholders/<name>.app` ships a placeholder icon and
  `Info.plist` for each of the 32 - the system's "installing" icon.

Why they never install: they do not belong in `/Applications` on a device.
`BuildManifest.plist` for this IPSW identifies `Cryptex1,AppOS` as
`087-86715-047.dmg`, a **10 MB** skeleton (its tree is only
`System/Library` + `usr/libexec`, 1047 entries - it never holds an app), and
the first-boot migration (`SystemAppMigrator` / `cryptexd`) is what installs
the staged bundles onto the App cryptex at `/System/Cryptexes/App`. Our root
is one plain HFS+ volume, the App cryptex is just a directory, and this image
has the migration tweaks (`apply-root-tweaks.sh` sets `LastSystemVersion`),
so nothing ever installs them and LaunchServices never registers them.

Fix: move each staged bundle into `/Applications`, the directory
LaunchServices already scans for the icons that work. Same volume, so it is a
rename that costs no space. `tools/jackpotlte/apply-root-tweaks.sh` (the same
pkexec fsck / mount-rw / umount wrapper it already used for the donor tweaks)
moves them on an existing image, and `tools/mkiosroot.sh` does the same during
a from-scratch build. Applied to the master, the next
USERDATA reflash (destructive; needs the owner's go-ahead) should show the 32
apps. `docs/research/data-migration-prior-art.md` records that
UniXNU-droidvm presents staged apps through a union over `/Applications`
(`owner-staged-apps=1`), the same idea.

### The app fix and the probe are flashed (`jt43`, 2026-11-10, awaiting the photo)

`/var/tmp/IOSRoot.img` was prepared with `tools/jackpotlte/apply-root-tweaks.sh`
(fsck clean before and after): **32 staged system apps moved into
`/Applications`** and `maliprobe` installed at `/usr/local/bin/` with its
LaunchDaemon. Verified in the image afterwards with `7z l`: `Maps.app`,
`Music.app`, `Weather.app`, `Calculator.app`, `Files.app`, `Books.app`,
`MobileMail.app`, `MobileNotes.app`, `Podcasts.app`, `Stocks.app`, `Tips.app`,
`Compass.app`, `Magnifier.app`, `Health.app`, `Home.app` are all in
`/Applications`, `/private/var/staged_system_apps` holds nothing, and
`usr/local/bin/maliprobe` (66,984 bytes) and
`System/Library/LaunchDaemons/com.jackpotlte.maliprobe.plist` (816 bytes) are
present.

The new sparse was built and verified before flashing:
`img2simg IOSRoot.img IOSRoot.sparse.img` gave 12,145,402,264 bytes, and
`tools/jackpotlte/verify-sparse.py` (new) walked all 27,819 chunks / 4,194,304
blocks and compared every data byte against the raw image — the host has no
room for a second 16 GB copy, so it verifies in place instead of by
re-conversion. (`simg2img file -` is not a thing; it writes a file named `-`.)

Flashed with the owner's approval: BOOT = `jt43` (quiet), USERDATA = the new
sparse (47 sparse chunks, 510 s). The phone rebooted into it. What the next
photo should show: the boot text (the M2 `word=0xc0ffee01` lines and M3's
`engine up, addresses from 0x1000000` / `JackpotlteMali: the GPU is there for
programs to draw with`, plus the probe's `JACKPOTLTE MALIPROBE:` report), and
then the home screen with the 32 extra icons.

### Dropped: the status-bar fix, and the `sweetfb_noconsole` that never applied

**The status-bar change is dropped (owner, 2026-11-11).** Nothing
display-related was ever written or flashed: the kernel still reports
1080x2220 through selector 8, the ADT still carries `canvas 1080x2220` and
`display-scale 3`, and no patch was ever made for the presented mode. The
diagnosis above stands as the answer to why the bar reads `rdar:45025538`; the
change itself is not being pursued.

*(Superseded later the same day: the owner asked for it again and chose the
squeeze, `0156` - see "The rdar bar fix goes back in" at the end of this
file.)*

**`sweetfb_noconsole=1` has never done anything on this board.** The A5XELTE
build reads **`a5fb_noconsole`** — `0109` wraps the argument in
`#if defined(ARM64_BOARD_CONFIG_A5XELTE)` and the A8 takes the first branch
(`xnu/patches/0109-a5xelte-decon-iosurface-bridge.patch:83`), with the donor's
`sweetfb_noconsole` in the `#else`. So `jt31`, `jt38` and `jt43`, all packed
with `sweetfb_noconsole=1`, kept drawing the console over the GUI exactly as
before, and the "quiet boot" that `jt19` got on 2026-11-08 stopped working when
`0109` renamed the argument. Any future quiet image must pack
`a5fb_noconsole=1` (and `0146`/`0151`'s comments, which said
`sweetfb_noconsole=1`, now say "the quiet console"; the `0067`/`0109`/`0110`
comment in `serial_console.c` still names the donor argument).

### Display scale is an integer, so the mode has to be one UIKit has as well

The owner chose "present 1080x2340, squeeze 5% vertically". Working out how to
do it turned up a fact that rules it out as written: **`/chosen/display-scale`
is a `u32`** (`tools/mkadt.py:95-99`, read straight by libMobileGestalt —
`NOTES.md:89` records SpringBoard failing with "scale 0" when it was missing).
3 is 3; a scale of 2.88 (which is what makes 1080x2340 a genuine iPhone 12 mini
mode at 375x812 points) cannot be expressed. With scale 3 those pixels are
360x780 points, which is no iPhone, so the red bar would probably stay.

What is left for a screen this size is a mode UIKit knows **in pixels and in
points at 3x**, and the only one that fits a 1080x2220 panel with a modest
transform is the donor's own: **1125x2436, 375x812 points** (1170x2532 and up
are worse). So the shape of the fix is unchanged — present a real mode, keep
the panel full, keep the status bar — but the numbers are: 22 px (7 pt) of the
width cropped, exactly as the donor crops it, and the height squeezed
2436 -> 2220, which is 8.9%, not 5%. The alternative that keeps geometry exact
is to scale both axes per pixel, which is the thing to do after M4, on a GPU.

### Why the status bar reads `😳 rdar:45025538` (2026-11-10)

UIKit draws that red bar, not our root and not any app. When
`+[_UIStatusBarVisualProvider_iOS visualProviderSubclassForScreen:]` returns
nil — no status-bar layout is known for the screen's size — `_UIStatusBar`
falls back to a debug bar whose text is the literal radar number; the glyph is
the 😳 emoji inside that string. The same artifact is reported for iPhone XR/11
in VisibleIsland's known issues and fixed in Nugget by its "Rdar fix", which is
nothing but a resolution change (`canvas_width`/`canvas_height`). We had it
before on this port: `NOTES.md` records the red bar at 1080x2400 and it going
away once AppleCLCD presented 1125x2436.

Ours is back for the same reason. The panel is 1080x2220 and the ADT's
`display-scale` is 3, so `UIScreen` is 360x740 points, and no iPhone is 360x740
(X is 375x812, 8 Plus 414x736, 8/SE 375x667, 12 390x844). The log line
`SWEET FB: AppleCLCD 1080x2220 ... presented as 1080x2220` is that: `0124`
leaves `fLogicalW/H` at `fWidth/fHeight` for A5XELTE, so selector 8 answers
1080x2220. The donor's path is compiled out for this board. Nothing in the
root, the ADT identity or localisation produces the string; `grep -r 45025538`
only hits `NOTES.md` and a kernel patch comment.

The fix has to be "present a real iPhone mode", and there is a wrinkle the
earlier note did not state: **our panel is 18.5:9 (1080x2220) and every notched
iPhone mode is 19.5:9 (~1125x2436, 1080x2340, 1170x2532, 1242x2688 — all
0.462 ± 0.001, ours 0.4865)**. So a full-bleed fit cannot be exact, and the
options differ in what they cost:

1. **Donor mirror, no scaling** (what `0061` does on the donor): present
   1125x2436 and show its centred 1080x2220, i.e. crop 22 px each side and
   **108 px (36 pt) off the top and bottom**. On the donor's 1080x2400 panel
   that is 18 px and invisible; here 36 pt of a 44 pt status bar disappears, so
   it trades the red bar for a chopped status bar. `fOffY = 0` (crop the bottom
   only) keeps the status bar but loses 72 pt at the bottom.
2. **Present 1080x2340 (iPhone 12 mini, 375x812 pt at scale 2.88) and
   downscale height only** (2340 -> 2220, a per-row index change, so the
   `memcpy` per row stays): full panel, no crop, width untouched, but the
   picture is squeezed 5.1% vertically and the ADT scale must become 2.88.
3. **Scale both axes** (1125x2436 -> 1080x2220, or a uniform 0.911 with 27 px
   bars): exact geometry, no crop, but per-pixel work on a machine whose
   compositing is already the CPU. Only sensible after M4.

Until a mode is presented, everything is visible and only the bar is wrong, so
this is not urgent against the GPU work. Whichever is chosen, three files move
together — the framebuffer logical size (`0124`), the ADT canvas
(`mkadt_jackpotlte.py`), and `SweetHID`'s `fLogicalW/H` and `fOffY` (`0118`,
today hard-coded 1080x1920) — or touch and the reported screen disagree. Verify
on the phone by the red strip being gone from the lock screen and by
`presented as 1125x2436` (or the chosen mode) in the console line.

### M4 userspace starts: `userspace/jackpotlte-maliprobe` (2026-11-10)

The GPU kernel side is at parity with the reference: a Bifrost Mali needs no
firmware (the reference says so at `redwood_mali.c:6`) and the reference also
runs one AS0 and one job at a time, which is what `0152`-`0154` do. So the
remaining distance to a fast home screen is userspace, and this is its first
artifact.

`userspace/jackpotlte-maliprobe/maliprobe.c` is an arm64 iOS tool that speaks
the `0154` ABI as a Darwin client: it finds the `JackpotlteMali` service, opens
it, makes a 16 KB buffer, maps it with `IOConnectMapMemory64(handle)`, checks
the words read back, asks `get_param` for the GPU id / present+ready masks /
`nr_core_groups` / `thread_tls_alloc` / `afbc_features`, and closes the buffer.
Since there is no shell on the phone, it prints to stdout *and* to
`/dev/console` so a photo can read the result; `com.jackpotlte.maliprobe.plist`
is the LaunchDaemon that would run it once at boot.

It cannot be compiled or run on this host: it is Mach-O arm64 against the
iPhoneOS SDK. `.github/workflows/userspace.yml` builds it on `macos-15` and
uploads `userspace/out/` as an artifact. Its `--abi` mode prints the selectors,
structure sizes and offsets and touches no IOKit, so the same job runs it on the
runner and greps the values against `xnu/patches/0154` — the same "green is not
a patch that applied" rule the kernel work uses, applied to the ABI: a structure
or selector that drifts on either side turns the build red instead of failing on
the phone. The structures were read back field by field against `0154` by hand
before this was written (they match exactly), and the file also compiles
clean under `clang -Wall -Wextra -Werror -fsyntax-only` here.

Nothing about this is in the root image yet: the binary has to come out of a CI
run first, and the image only changes at the next USERDATA flash. What comes
after the probe is the shim (`redwood_drm.c` retargeted from `RedwoodMali` to
`JackpotlteMali`), Mesa with Bifrost **v6**, and then the `JackpotlteMetal`
bundle that `0155` already publishes the service for.

### M4 step 1 verified in the built kernel (`0155`, CI 42884335048, 2026-11-10)

CI `42884335048` (`main` @ `d074908`) finished **success**. The downloaded
`kernel.development.a5xelte` (16,601,960 B, sha256
`014d6ce31a27386fd38ce7b8aa3207ebd7b2c48e4273e909a8a73fbaf0001855`) carries
`SWEET FB: jtmetal=1: the accelerator names JackpotlteMetal as its Metal
plug-in` plus `IOAcceleratorES`, `JackpotlteMetal` and `JackpotlteMetalDevice`
symbols, so `0155` really compiled and linked into the shippable kernel (green
alone is not trusted). It is not yet flashed; `jtmetal=1` is the gate.

### Reached: iOS at the lockscreen, all 32 promoted apps on the home screen (2026-11-11)

The `jt43` boot (USERDATA from 2026-11-10, 32 bundles promoted into
`/Applications`) runs iOS to its lockscreen, and the home screen behind it
shows **all 32 of the promoted apps**, not the six that were there before. So
the diagnosis of the missing icons was right - iOS 16 expects to install them
onto the App cryptex, which a single plain-HFS root cannot do - and the
same-volume rename into `/Applications` is enough; no LaunchServices
`csstore` clearing was needed.

That is a screen result, seen on the phone, and it is the first time the iOS
home screen has been complete on this board.

**What is still wrong on the same screen:** the kernel console keeps drawing
its text over the GUI. `jt43` was packed with `sweetfb_noconsole=1`, which
this board ignores: `0109` reads **`a5fb_noconsole`** under
`ARM64_BOARD_CONFIG_A5XELTE` and keeps the `sweetfb_noconsole` name only in the
`#else`. So the quiet switch has never applied to an A5XELTE image. The fix is
the boot-arg name and nothing else; a repacked BOOT (same kernel, cmdline
changed) is enough, and it does not touch the root.

### M4 step 2: the libdrm face of the shim (2026-11-11)

`userspace/libjackpotlte/jackpotlte_drm.h` + `jackpotlte_drm.c` are the surface
Mesa's panfrost is built against instead of libdrm. The names libdrm owns keep
their meaning (`drmOpenWithType`, `drmIoctl`, `drmGetVersion`, the `drmSyncobj*`
family, `drmPrimeFDToHandle` for a SweetSurface by its ID, ...), so Mesa calls
them unmodified; what libdrm has no equivalent for is ours with a
`jackpotlte_` prefix (`jackpotlte_drm_map`/`_unmap` in place of `mmap` on the
device, `jackpotlte_drm_surface`, `jackpotlte_drm_last_seq`/`_wait_seq`).

The part that differs from the reference's shim is that the structures are
translated field by field instead of passed through: current Linux's
`struct drm_panfrost_submit` is 48 bytes, 0154 reads exactly its 40-byte one
and refuses a larger request, and the kernel's answer (the job's number) comes
back in `jc` where Linux puts nothing. A request's low byte is still the
kernel's selector, because 0154's selectors are Linux's ioctl numbers.
`PANFROST_MADVISE` now carries the kernel's `retained` answer back (it was
discarded), which is what panfrost reads to decide whether a buffer still has
its memory.

Checked on a build machine: `test_gpu_host` runs the whole drm face against the
same fake IOKit (submit round trip with `out_sync` -> job number, the 40-byte
size actually sent, create/get_param/madvise/mmap/close, surface describe and
describe-and-make with the field translation, the mapping kept across
unmap and dropped at close, version/cap/node/devices, sync objects and their
sync files), under ASan+UBSan with `-Wall -Wextra -Werror -Wpedantic`. The ABI
check still reports the 10 selectors, 10 structures and 2 constants identical
to `0154`. Neither says anything about the GPU; only the phone can.

### The rdar bar fix goes back in, with the quiet console (2026-11-11)

The owner asked for the status-bar fix again and picked how the geometry is
solved: present the iPhone X mode **1125x2436** (375x812 points at the ADT's
`display-scale 3`) and **squeeze its 2436 rows onto the 2220-row panel**,
rather than cropping a strip off the top or the bottom. Nothing in this
section is on the phone yet.

`xnu/patches/0156-jackpotlte-presented-iphone-x-mode.patch` is the change, in
two files:

- `SweetFramebuffer.cpp`: the A5XELTE branch of `start()` sets
  `fLogicalW/H = 1125x2436`, `fOffX = 22`, `fOffY = 0`. `present()` gained the
  row mapping: panel row `y` copies logical row `y*2436/2220` (nearest), so the
  whole canvas lands on the shorter panel, and the 45 extra columns are
  cropped 22 left / 23 right, the way the donor crops its panel.
  `sweetfb_native=1` still presents 1080x2220, so the way back to today's
  screen is a cmdline repack rather than a kernel rebuild.
- `SweetHID.cpp`: the ZT7538 reports 1080x1920 over the whole panel, so the
  touch normaliser keeps dividing rows by 1920 (which reaches the bottom of the
  display, as it did before) and only gains the column offset,
  `nx = (x + 22)/1125`.

`tools/mkadt_jackpotlte.py` writes `canvas-width/height 1125x2436` now - the
presented size, which is what IOMobileFramebuffer and MobileGestalt read - while
the `a5xelte-fbcon` node keeps the physical 1080x2220. The only bytes that
changed in the ADT blob are the three that make up those two canvas words
(the blob also carries a fresh random 64-byte `random-seed` each run, so the
rest of it differs between any two generations and cannot be compared
byte-for-byte).

The row mapping was checked on the build host (a scratch program, not
committed): with the real numbers, every one of the 2220 panel rows is written
exactly once, the source rows rise monotonically from 0 to 2434, and all 1080
columns are covered. That says nothing about the phone.

On the phone this is right only if the red strip is gone from the lock screen
**and** the console shows `SWEET FB: iPhone X mode 1125x2436 squeezed onto the
1080x2220 panel` followed by the existing `presented as 1125x2436` (the first
line is the marker to grep for in the downloaded kernel: a green build is not
a patch that applied). A squeezed picture with the strip still there would
mean the canvas did not reach backboardd; `sweetfb_native=1` puts the old
geometry back.

This and the quiet console (`a5fb_noconsole=1`) go into one new BOOT. Its
cmdline is `jt43`'s with that one word changed, but because `0156` is a kernel
change the image also carries a rebuilt kernel, so it is no longer a
cmdline-only repack.

### `jt44` built and checked, not yet flashed (2026-11-11)

CI `43033244620` (commit `e219bf1`) finished **success**. The downloaded
`kernel.development.a5xelte` (16,601,960 B, sha256
`28228568eb7c8c4d56b08643eca2178011a620207af7266767ff8a2a100f8c50`, matching
the artifact's own `SHA256SUMS`) carries the string
`SWEET FB: iPhone X mode %ux%u squeezed onto the %ux%u panel`, so `0156` is in
the kernel that was packed and not only in the patch series.

`jt44` = that kernel + the regenerated ADT + the `jt43` cmdline with
`a5fb_noconsole=1` in place of `sweetfb_noconsole=1`, packed with `jt43` as
the header/DTBH source:

```
17,522,704 B  /var/tmp/a8-xnu-jt44-quiet-iphonex.img
sha256 d86b760e674fa2411434fc254f55398cf2d3b03b8d40c37b790d48e119d61109
```

Read back out of the packed image (not out of the build inputs): the payload
magic is at `0xd800`, the cmdline is the expected 302 bytes ending
`... jtmalimetal=1 a5fb_noconsole=1`, `/arm-io/disp0` has
`canvas-width/height 1125x2436`, and `/a5xelte-fbcon` still has 1080x2220
(that node is the early console's scan-out buffer, not the presented size).
`jackpotlte_bootimg_info.py` reports kernel 0xcb50, ramdisk 0xfd3db0,
dt 0x870400 and the same addresses as before.

The phone was not attached to this machine, so nothing was flashed. To try it:
`fastboot flash BOOT /var/tmp/a8-xnu-jt44-quiet-iphonex.img`. `jt43` stays the
fallback; if `jt44` shows a squeezed
picture with the red bar still there, repack the same payload with
`sweetfb_native=1` added to the cmdline and flash that.

Also: CI `43032631715` went red on `jackpotlte_drm.h/.c` (`-Wnewline-eof`);
fixed in `46829ab`, and `43033244632` is green.

### The Mali reference's licensing, checked file by file (2026-11-11)

The open question was whether the reference's Mali userland can be reused. The
answer is now written down in `docs/mali-userland-provenance.md`, from the
public repositories at the pinned commits:

- The snapshot every Mali file comes from — `hakios-xnu-kernel` branch `mtk` @
  `620c775` — carries **no licence file at all**. (Re-checked 2026-11-13: the
  branch is not a single commit — its root is `3f31f34285` (2026-11-06,
  parentless), it has 7 commits with HEAD `a962798418fa` (2026-11-10), and no
  commit on it holds a licence file. It also shares no history with `main`, so
  `main`'s licence cannot reach it by any route.)
- The repository's `LICENSE` appeared later, on `main`, grants MIT to "the
  user-space code in `userland/src/`", and `main` **no longer contains the Mali
  userland**: the only `redwood` path in it is `docs/devices/redwood.md`.
- `tools/mesa/air/*` (the AIR frontend) is different: 16 of its 17 files carry
  `SPDX-License-Identifier: MIT` in the file itself, only `air_metal.h` does not.
- All 23 files of `userland/src/metal/` and `redwoodmetal.m` carry no SPDX line
  and no copyright line.
- The reference's own `COMPLIANCE.md`/`THIRD_PARTY_LICENSES.md` list its Metal
  plug-in as **to verify**, because it "began from vcvkk's" plug-in and that
  repository "states no license".
- `hakios-mt6768`, where the SoC driver we already cite lives, is clean: `NOTICE`
  gives `kernel/` APSL 2.0 and everything else MIT, and says no third-party code
  was copied. Our `xnu/patches/` are already APSL 2.0, so that half adds no new
  obligation.
- `oosexgang/hakios-metal` is the **Adreno** driver despite the name, not the
  Mali one.

Three options are set out in the document. The recommended one is to take the
AIR frontend and the SoC-side driver (both granted) and write our own Metal side,
because the unverified files are the ones the whole performance work would rest
on. The owner has not chosen yet; nothing was imported.

One boot of `jt44` answers more than the display question, so it is worth a
photo of the console as well as a look at the screen. Its cmdline is `jt43`'s
with that one word changed (read back out of both images to check), so it still
carries `jtmaliprobe=1 jtmalipwr=1 jtmalijob=1 jtmalimetal=1`, the gates `0144`,
`0152`, `0153` and `0154` read, and the kernel in it is the one CI
`43033244620` built. The same boot therefore also reports M2 (`word=0xc0ffee01`
for both jobs) and M3 (`engine up` plus `JackpotlteMali:`) if those stages run.
`jtmetal` is deliberately **not** set: the plug-in it names does not exist yet.

### M4 userspace: Mesa panfrost v6 for iOS, written (2026-11-13)

The M4 plan chose Mesa's panfrost as the layer below the Metal plug-in, and the
work to build it is now written: `tools/mesa-jackpotlte/` holds three patches
against Mesa 25.0.7 and `.github/workflows/mesa.yml` builds
`out/libOSMesa.8.dylib` for arm64 iOS on `macos-15`.

- `0001-darwin-drm-standin.patch` — there is no libdrm and no `mmap()` on a
  device, so `dep_libdrm` becomes a dependency on `src/jackpotlte-drm` (our
  `userspace/libjackpotlte`, installed as `xf86drm.h`); a buffer is mapped and
  unmapped by its handle; a SweetSurface's size comes from the kernel's describe
  request (`0154`'s `0x7e`), not `lseek()`.
- `0002-panfrost-v6-darwin.patch` — the A8's G71 is Bifrost **v6**, so the
  Darwin build compiles that architecture alone (the reference builds v7 for the
  Mali-G52), and `pan_raw_format_mask_midgard` is compiled for v6 too. This is
  the board-specific change; `docs/jackpotlte-mali.md` §4 has the gap analysis.
- `0003-osmesa-panfrost-screen.patch` — OSMesa asks for the GPU's screen before
  the processor's (`OSMESA_SOFTWARE=1` still forces softpipe), and turns a
  SweetSurface's pixels into a texture.

All three apply with `patch -p1` to a pristine `mesa-25.0.7.tar.xz`
(sha256 `592272df…510f8a6`), `bash -n` and the repo validator pass, and the
workflow has not run yet. **Nothing has been built green and nothing has been on
the phone**: a library that builds is not a screen.

The licensing re-check of the same day corrected the earlier note: the `mtk`
branch is 7 commits, not one, and `vcvkk/darwin-redwood` (the origin the
reference points to) is gone from GitHub with no archive, so its terms cannot be
confirmed by anyone. `docs/mali-userland-provenance.md` now carries this, the
APSL 2.0 obligations for the kernel half, and the four internal contradictions a
written reuse request would settle.

### Route decided: GLES via Mesa panfrost, not Metal (2026-11-13)

The owner chose the GLES route. The reason is the reference's own Metal analysis
(`hakios-xnu-kernel` branch `mtk`, `docs/metal.md`): Metal's shader input is
Apple's AIR bitcode, for which no reader exists anywhere (§4, "no ready AIR
reader in the world"), so a Metal driver needs an AIR→NIR compiler bigger than
everything else in that project's graphics work; and CoreAnimation's
window-server renderer (`§3`) tries Metal first and **falls back to
EAGL/OpenGL ES**, which is the renderer that draws the UI and wallpaper.
CoreImage's default backend already goes to GLES too (§7). So the CPU-rendering
problem is fixed on the GLES path — Mesa panfrost v6 (above) plus an
`EAGLContext` shim to CoreAnimation — and does not need Metal at all.

Consequences: the Metal plug-in (`0155`'s bundle and the reference's AIR
frontend) is deferred, and the licence question about reusing the reference's
Metal userland is parked with it. The GLES route takes only Mesa (upstream) and
our own shim, so nothing from the reference's Mali userland is needed for it.
If the Metal plug-in is picked up later, licence **option 1** (take the granted
frontend and driver, write our own Metal side) remains the recommendation.

`jt44` stays parked (the owner's call); it is not needed for the GLES route.

### The OpenGL ES dispatch table of this build, generated (2026-11-13)

CoreAnimation is not given GL functions by name: `-[EAGLContext
getMacroContextPrivate]` returns a table, and its caller runs `entry(table[0],
arguments...)`. The offsets belong to the build's `OpenGLES.framework` and are
written down nowhere, so they have to be read off the framework itself.

They now are. `tools/jackpotlte/eagl-slots.py` walks every `_gl*` export of the
`OpenGLES` image in the dyld shared cache and decodes its trampoline — state from
`tpidrro_el0+0xf0`, context from `+0x20`, the entry loaded from `+IMM` and
jumped to (`br`, or `blr` when the arguments need a frame) — so the table offset
is `IMM-0x20`. The generated table of the phone's build (iOS 16.7.16, 20H392,
iPhone X) is `userspace/jackpotlte-gl/eagl_slots.txt`: **409 entries, 457
names**.

Two facts worth having:

- The table was read **in place** out of the 16 GiB HFS+ root image. The cache
  files there are hard links from the SystemOS cryptex; their link records break
  a normal HFS+ walk, so the tool finds them as `[HFS+ Private Data]/iNode*`
  whose first bytes are the cache magic (`--hfs-image`, pytsk3). Nothing had to
  be extracted on a host that had 2.2 GB free.
- It is **identical to the reference's iOS 16.5 (20F66) table**: 457 names, 409
  offsets, no name at a different offset, the same names per entry. The layout
  did not change between those builds, so the per-build worry was smaller than
  assumed — but the table in the tree is still the one generated from this
  phone's build, as it should be.

The Mesa build status is unchanged in substance: run `43478062666` was
**cancelled** after 15 minutes in the queue with no runner assigned (zero steps,
`runner_id` 0 — GitHub's macOS queue, not the build). It was re-queued; nothing
has built green, and nothing has been on the phone.

### The EAGL shim is written, and the Mesa build's first real failures are fixed (2026-11-13)

The first Mesa run that actually reached the compiler stopped in the driver's
dependencies, not in the driver. `src/gallium/auxiliary/renderonly/renderonly.c`
came back with `variable has incomplete type 'struct drm_mode_destroy_dumb'` and
`use of undeclared identifier DRM_IOCTL_MODE_DESTROY_DUMB` (and the CREATE pair) —
six errors.

- The file has to be built: `src/gallium/auxiliary/meson.build` adds it whenever
  `dep_libdrm` is found, `0001` makes it found on Darwin, and panfrost itself
  calls `renderonly_scanout_for_resource` and friends from `pan_resource.c`.
- It includes only `<xf86drm.h>` and then uses the dumb-buffer request codes.
  Real libdrm's `xf86drm.h` includes `drm.h`, and Mesa carries that header
  (`include/drm-uapi/drm.h`, which includes `drm_mode.h`); our stand-in had
  assumed a caller that includes it first. panfrost does; `renderonly.c` does
  not.
- `jackpotlte_drm.h` now includes it too (`7ed0717`), under `__has_include`
  because `userspace/build.sh` builds the same header for the phone, where there
  is no `drm-uapi` copy and nothing asks for `DRM_IOCTL_MODE_*`. The only macro
  the two headers share (`DRM_CLOEXEC`, `DRM_RDWR`) is textually identical.
- Checked on the build machine for both branches `drm.h` takes (Linux, and with
  `__linux__` undefined — the branch Darwin takes), and that the phone build
  skips the include. Whether the iPhoneOS SDK's `<sys/ioccom.h>` gives the BSD
  branch what it wants is still only answerable by a Mesa run.

`0004-osmesa-es-profile.patch` was added (`6da7e9e`). OSMesa makes desktop GL
contexts only; the EAGL shim behind CoreAnimation has to make an OpenGL ES
context of the version it was asked for, and with `OSMesaCreateContextAttribs`
the only way to ask is `OSMESA_PROFILE`, which refused anything but the two
desktop values. The patch accepts a third, `OSMESA_ES_PROFILE` (0x38), and maps
it to `API_OPENGLES2` — the same state tracker with another profile. The
four-patch series applies in order to a pristine 25.0.7 tree.

The shim itself is written: `userspace/jackpotlte-gl/` holds the `EAGLContext`
implementation (`eagl.c`), the OSMesa backend behind it
(`jackpotlte_gl_osmesa.c`), the interface between them (`jackpotlte_gl.h`), and
the generated dispatch table. `userspace/build.sh` builds it as
`out/libjackpotlte-gl.dylib` (arm64 iOS).

- **No `gl*` function is called by name.** iOS's `OpenGLES.framework` is in the
  dyld shared cache and defines every one of them, so a by-name call would bind
  Apple's driver, which knows nothing of these contexts. Everything is resolved
  through `jackpotlte_gl_proc`, which is OSMesa's own `OSMesaGetProcAddress`.
  The build fails if `nm -u` on the dylib shows a ` _gl[A-Z]` name.
- The dispatch table's entries call `jackpotlte_gl_use(context)` before passing
  the call on: OSMesa's current context is per thread, and the display server's
  renderer is not always the thread that made the context.
- Ten `EAGLContext` methods are installed with `method_setImplementation` (the
  class's own encodings stay; a selector iOS does not have is logged, not added
  with a guessed signature): the six CoreAnimation's renderer asks, the two
  public initialisers (`initWithAPI:`, `initWithAPI:sharegroup:` — an object made
  by Apple's would have none of the shim's state), `-sharegroup` (nil; CoreImage
  asks, and the reference crashed there for want of it) and `-dealloc`.
- A surface becomes a texture by handing its ID to
  `glEGLImageTargetTexture2DOES`, which `0003`'s frontend hook turns into a
  buffer of that surface (`0154`'s `0x7e`) — the display's memory itself, no
  copy. A surface that cannot be one (not one plane of 32-bit BGRA pixels) is
  refused with a log line; a copy would be a different picture and a different
  bug. `-setParameter:to:` notes its parameter without reading the value,
  because what the caller passes it as has not been established here.

Three CI failures were found by reading the logs rather than guessing, and fixed:
the iPhoneOS SDK ships `IOSurface.framework` without a header (the six accessors
are declared in the file, as the iOS backend already does for IOKit);
`<objc/message.h>` declares `objc_msgSendSuper` as `void (void)`, so the union
member has to be that declaration and not a prototyped one; and `-Wextra`'s
`-Wcast-function-type-mismatch` rejects the `gl*` function-pointer casts, which
are unions now.

**Userspace CI `43488007184` is green on `a1a5a7f`** — all eleven steps, the
dylib compiled and linked for arm64 iOS with no `gl*` by name. The build's own
checks were then tightened (`069825f`): the no-`gl*`-by-name assertion no longer
sits in `if nm … | grep …` where a failing `nm` under `set -o pipefail` made it
pass by not running, the dylib is in the closing `file`/`lipo`/`ls -l`, and the
workflow's arm64 step checks it too.

Nothing has built green for Mesa: the two runs since the renderonly fix
(`43485844894`, `43486044591`) were both **cancelled after exactly 15 minutes in
the queue** with no runner assigned (`runner_name` empty). `43486044591` was
re-queued. Nothing has drawn a frame and nothing new has been on the phone.

The next thing is the loader: how `libjackpotlte-gl.dylib` gets inside
`backboardd`. This root has no `libredwood.dylib`, so the candidates are a
launchd `EnvironmentVariables` insert, a dyld interpose, or replacing
`OpenGLES` (cache-only). Settling it is a `userdata` change and a phone test, and
that needs the owner's word before anything is written.

The reference's own route settles the first two. `xnu/redwood.patch` adds
`sysctl kern.exec_inject`, holding `LIBRARY NAME NAME...` groups, and
`exec_inject_env()` in `exec_extract_strings()` (`bsd/kern/kern_exec.c`):
on exec, a program whose **file name** is one of the NAMEs has
`DYLD_INSERT_LIBRARIES=LIBRARY` appended to its environment, so dyld loads the
library at process start. Its own comment gives the reason it is not a launchd
job: *"launchd refuses to pass DYLD_ variables from a job description, which is
why the kernel adds this one."* The reference's `INJECT` default
(`userland/build.sh`) is `libredwood.dylib backboardd` among others, written to
`etc/inject.conf` and set once by their init before it hands process 1 to
launchd. So for this port the launchd-list route is out and a dyld interpose
meets the same gate as the kernel route; what is left is the kernel, and since
this port has no init of its own (Apple's launchd **is** process 1), the string
has to come from a boot-arg — a `LaunchDaemon` setting the sysctl would race
`backboardd`'s exec, which launchd starts in parallel with it.

The half that is not the injection is code signing: `cs_require_lv()`
(`bsd/kern/kern_cs.c`) is what makes a platform binary refuse a library that is
not itself platform-signed, and the shim is unsigned. The reference's patch
makes it return 0 everywhere, naming `libredwood.dylib` as the reason. This
port does not do that: `0035`/`0036` mark root-filesystem binaries
`CS_SIGNED | CS_PLATFORM_BINARY` and nothing else, and vanilla xnu-8796's
`cs_library_val_enable` boot-arg clears only the global half of that check, not
the per-process `CS_REQUIRE_LV` flag. Both the injection hook and
`cs_require_lv()` are BOOT-side; only placing the library in the root is a
`userdata` write, and that is the step that needs the owner's word. What is left
for the phone to answer: whether dyld honours the injected variable for
`backboardd` on this kernel, or `cs_require_lv()` has to be turned off first as
it did there. `docs/jackpotlte-mali.md` §9 item 3 has this with the citations.
