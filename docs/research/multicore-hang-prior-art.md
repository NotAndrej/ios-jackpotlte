# Multi-cluster XNU secondary-CPU hang: prior art

Research-only note. Written 2026-11-08. Read-only; no device or git state changed.
This document summarises what other public XNU port projects show for the
`cpus=6/8` panic on Galaxy A8 2018 (`jackpotlte`, Exynos 7885), specifically
`cpu_signal failed in DebuggerXCallEnter`, `Ticket Lock ... unexpectedly owned
@lock_ticket.c:143`, and `pset_find(cluster_id=...) returned pset -1`.

## 1. Our symptom (for matching)

From the photo in `JACKPOTLTE_PROGRESS.md` ("Enabling all 8 cores" and
"`cpus=4` boots"):

```
cpu=1 CPU_ON target=0x101 entry_pa=0x881417000 context=1
cpu=1 entry 0x881417000 via low trampoline 0x8f000000
cpu=1 cluster=1 pmu before: cfg=0x0 status=0x0f reset=0x0 cpuseq=0x0 noncpu=0xc00f l2=0x1
cpu=1 entered InitCPU mpidr=0x101 EL1 IRQs=0
cpu_signal failed in DebuggerXCallEnter            (repeated x4)
panic(cpu 0 caller 0xfffffff007a66300): Ticket Lock 0xfffffff0083c0680 is
  unexpectedly owned by thread 0xfffffff6f053b430 @lock_ticket.c:143
```

Also on screen: an earlier harmless `pset_find(cluster_id=1) returned pset -1`
debug print (`osfmk/arm64/machine_routines.c:1424` in our tree), which is
followed by `pset_create()` and is not the bug by itself. `cpus=4` (one cluster)
boots; `cpus=6` and `cpus=8` panic.

## 2. Direct match: `georgev-6974/s9plus-ios` (Exynos 9810, star2lte)

Verified by reading
`https://raw.githubusercontent.com/georgev-6974/s9plus-ios/main/STAR2LTE_PROGRESS.md`
(fetched 2026-11-08). This is a public XNU port for another Samsung Exynos
multi-cluster SoC (Exynos 9810, Cortex-A55 plus Meerkat/M3), and it documents
the same failure family.

Verbatim, from that file's "HANDOFF 2026-11-07 (evening)":

> With cpus=2, panic "Ticket lock ... unexpectedly owned" (lock_ticket.c:155),
> ctid == lock owner.
> - Seen on cpu 0 (lock = _pset0 at 0xfffffff007b74880) and on cpu 1 (lock
>   0xfffffff03ba48100, probably a dynamic pset for cluster 1; pset_find(cluster_id=1)
>   returned -1 in the log).
> - Stack is always `lck_ticket_contended <- thread_select <- thread_block_reason
>   <- Call_continuation`.
> - On cpu 1 the owner equals cpu 1's own thread (0x7b6bc) on a freshly created
>   lock: unexplained.
> - No Sweet patch calls pset_lock/pset_unlock (grep empty); processor_startup
>   balances lock/unlock.

And, later, the resolution it reached:

> **Root cause of the "Ticket lock unexpectedly owned" panic (0145 era):** XNU
> `ml_parse_cpu_topology()` derives the logical cluster from MPIDR Aff1, so
> 4 cores with Aff1 0..3 became 4 clusters. Fixed by **0149**
> (`tools/star2lte/make-0149.py`, machine_routines.c: cluster-id from the ADT,
> every core's physical Aff1 registered in `cluster_phys_to_logical`) and **0150**
> (`make-0150.py`, board_config.h: `MAX_CPU_CLUSTER_PHY_ID=7` for A5XELTE;
> default was `MAX_CPU_CLUSTERS-1 = 1`, assertion `machine_routines.c:1058`
> failed for cpus=4).

And:

> With cluster-id 1 for the M3 node: `pset_find(cluster_id=1) returned -1` is
> normal (pset gets created), but the `_pset0` ticket-lock panic returns as
> soon as a cluster-1 processor registers. Hypothesis (unproven): this build
> is not `__AMP__`, two psets under pset_node0 break the scheduler.
> **Workaround in use: all 8 cores in logical cluster 0.**

And the local ADT-side change:

> **Local ADT edits in `tools/mkadt_star2lte.py`** (no CI needed ...): M3 nodes
> now also get `arm,cortex-a53` in `compatible` ... and `cluster-id` is 0 for
> ALL cores (one pset).

Labels: this is **verified** for what the document says; whether it is the whole
truth for our A8 is **inference**, but it is the closest published case.

## 3. Other XNU port projects checked

| Project | What it is | Relevance | Status |
| --- | --- | --- | --- |
| `dipa-studio/ios-sweet` (and forks such as `Floppikrob/ios-galaxy-s8`, `ours`) | Sweet/A5 single-core A5X bring-up; SMP only later | Baseline; verifies `ml_processor_register` / `processor_start` / SGI self-test path | Single CPU only; notes say "Two-CPU topology fails explicitly (`research/q0018-smp`)" (verified quote from repo code search). Not the ticket-lock hang. |
| `Floppikrob/ios-galaxy-s8` (`A5XELTE_PROGRESS.md`) | S8/A5X donor we forked from | SMP patches `0065`, `0078`, `0112`; first SMP phone result was "cpu 1 NOT alive after 10s" before the 0135/0136 PMU fixes | SMP difficulty reported, but I found no published ticket-lock hang there (verified: our repo logs say "secondary CPU startup failure, not verified SMP scheduling"; no `pset_find` bug). |
| `georgev-6974/s9plus-ios` | XNU port, Exynos 9810 | **Direct match** of the ticket-lock hang and of `pset_find(cluster_id=1) returned -1` | Verified (see §2). |
| `Emilelu/UniXNU-mondrian`, `MC-XiaoXiao/ios_sagit`, `lechaukha12/xnu-xzs`, `FoXLinne/UniXNU_marble` | XNU forks containing `ml_processor_register` and the standard `pset_find` debug print | All contain the same stock XNU instrumentation string | No project-specific multi-cluster fix found (verified via GitHub code search; mostly carry the kernel as-is). |
| `monsiu/...` other UniXNU branches | Snapdragon-class devices | Same SweetCPU driver lineage as ours | No verified hit of the exact hang in public code/issues I searched. |
| `oosexgang/hakios-*` | XNU on Snapdragon/MTK devices | Many boot iOS; SMP generally over PSCI on homogeneous clusters | No verified mention of `cpu_signal failed in DebuggerXCallEnter`, `lock_ticket.c:143`, or `pset_find(cluster_id=...)` in the repos/issues I searched. (HakiOS tends to keep SMP to one cluster or homogeneous cluster counts; not Exynos-specific.) |
| `ry-diffusion/Chlorophyll` / Bergamota project | iOS on Poco F1 (Adreno) | GPU-oriented; not an Exynos cluster port | No relevant hit. The name "Bergamota" in XNU context refers to `ry-diffusion/Chlorophyll` (Metal over Mesa on Adreno 630), not a multi-cluster Exynos bring-up. |
| Stock `apple-oss-distributions/xnu` | Reference kernel | Defines `__AMP__`, `HAS_CLUSTER`, `cluster_initialized`, `cluster_phys_to_logical` | Not a port; but is where the mechanics come from (verified, see §5). |

I searched GitHub via `gh search code`/`gh search repos` with terms:
`"SweetCPU"`, `"pset_find(cluster_id"`, `"DebuggerXCallEnter"`,
`"cluster_initialized"`, `"ml_processor_register"`, plus repo names
`ios-sweet`, `dreamlte`, `jackpotlte`, `HakiOS`, `UniXNU`, `Bergamota`.
Result: the only project reporting the exact ticket-lock/pset failure family is
`georgev-6974/s9plus-ios`. The exact console phrase `cpu_signal failed in
DebuggerXCallEnter` appears in GitHub code search only in Apple's stock XNU
sources, not in third-party project code (verified: 2026-11-08).

## 4. Themes from your question: present or not in prior art

1. **`HAS_CLUSTER` / `cluster_master` / `cluster_initialized` being wrong for
   multi-cluster Exynos.** **Yes (verified, star2lte).** In XNU:

   ```c
   #if HAS_CLUSTER
       this_cpu_datap->cluster_master = !OSTestAndSet(this_cpu_datap->cpu_cluster_id, &cluster_initialized);
   #else
       this_cpu_datap->cluster_master = is_boot_cpu;
   #endif
   ```

   (`osfmk/arm64/machine_routines.c:1412-1415`, apple-oss-distributions tree.)
   `cluster_initialized` is a single byte indexed by logical `cpu_cluster_id`,
   so it only works if logical cluster ids are sparse/disjoint. Star2lte hit the
   assertion `phys_cluster_id <= MAX_CPU_CLUSTER_PHY_ID` when the Exynos Aff1
   values 0..3 were used directly, and fixed it by raising the bound and
   remapping.

2. **`pset_create` / `pset_find` / `pset_cluster_id` mismatches between ADT
   `cluster-id` and what the secondary reports.** **Yes (verified, star2lte).**
   `pset = pset_find(in_processor_info->cluster_id, NULL);` then
   `pset_create(..., this_cpu_datap->cpu_cluster_id, this_cpu_datap->cpu_cluster_id)`.
   Star2lte shows `pset_find(cluster_id=1) returned -1` is by itself harmless
   (a new pset is created), but registering a CPU in that second pset panics.
   The trigger is the combination, not the print.

3. **All cores in a cluster sharing the same `cluster-id` / `l2-cache-id` in the
   ADT, and a collision or off-by-one breaking pset lookup.** **Partially yes
   (verified).** Apple code uses the *MPIDR* affinity (or `cluster_type`) to
   derive `phys_cluster_id` by default; it does not consume ADT `cluster-id`
   unless the port patches it to. Star2lte's actual fix was a *kernel* patch to
   read `cluster-id` from the ADT and use it as the logical cluster, plus local
   ADT edits forcing **all cores to logical cluster 0**. Before that, multiple
   Exynos clusters became multiple logical clusters. This is the single most
   likely root-cause match for our `cpus=6/8` failure: `cpus=4` works because it
   stays in one cluster; `cpus=6` introduces cluster 1 and panics.

4. **Timer / interrupt-controller per-cluster (GIC or arch timer PPI offset for
   CPUs 4+ wrong, so the secondary never takes its IPI).** **Not supported by
   prior art I found.** Star2lte's ticket-lock stack is always
   `lck_ticket_contended <- thread_select <- thread_block_reason <- Call_continuation`,
   i.e. a scheduler/pset problem, not a missing timer interrupt. Our secondary's
   `InitCPU mpidr=0x101 EL1 IRQs=0` matches that shape (secondary runs, cross
   call fails). The PPI/GIC configuration is not implicated by any published
   Exynos XNU port I checked; if CPUs 4+ had the wrong PPI the usual symptom
   would be alive-timeout, not this ticket lock. Treat as **inference**, not
   verified.

5. **Boot CPU's xcalls require secondary's timer interrupts enabled /
   `set_max_cpu_number` / `cpu_data` published first.** **Partially yes
   (verification in code, not in the port docs).** In `osfmk/arm64/cpu.c` the
   secondary path publishes `cpu_data` with cache cleans, and our 0065 patch
   makes `initCPU()` signal the self-SGI and *then* publish `initialized`,
   with `startAll()` waiting for that publication plus `sweet_cpu_signal_is_enabled()`.
   Star2lte independently enforces the same ordering ("Secondary startup runs at
   splsched: queue the self-IPI and return. Waiting here with DAIF.I set would
   deadlock every secondary"). That is consistent with our code, but prior art
   does not attribute the ticket lock to a missed publication step.

6. **Fix in the ADT generator (cpu list, cluster-id, l2-cache-id, die-cluster-id)
   rather than the kernel.** **Both.** Star2lte's fix spanned the kernel
   (read `cluster-id` from ADT; raise `MAX_CPU_CLUSTER_PHY_ID`) **and** the
   ADT (`tools/mkadt_star2lte.py`: `cluster-id` 0 for all cores; one pset).
   Our own `tools/mkadt_jackpotlte.py` tags three clusters (0/1/2) with
   `l2-cache-id=cluster` and `die-cluster-id=cluster`, and patch 0134 raises
   `MAX_CPU_CLUSTERS` to 3 but does not read ADT `cluster-id` into topology in
   the way 0149 does. So our current combination is exactly the "wrong handling"
   star2lte corrected.

## 5. Why the code path panics (verified against Apple XNU)

In `ml_processor_register` (apple-oss-distributions tree, 8796.141.3):

```c
#if HAS_CLUSTER
    this_cpu_datap->cluster_master = !OSTestAndSet(this_cpu_datap->cpu_cluster_id, &cluster_initialized);
#else
    this_cpu_datap->cluster_master = is_boot_cpu;
#endif
lck_mtx_lock(&pset_create_lock);
pset = pset_find(in_processor_info->cluster_id, NULL);
...
if (pset == NULL) {
#if __AMP__
    ... pset_create(... &pset_node0 or &pset_node1 ...)
#else
    ... pset_create(&pset_node0, PSET_SMP, cluster_id, cluster_id)
#endif
}
```

Our SWEET/A5XELTE build forces `HAS_CLUSTER` on (patch 0115 gates several
`#if HAS_CLUSTER` blocks with `|| defined(A5XELTE)`), so `cluster_initialized`
is a byte bitset indexed by *logical cluster id*, and non-`__AMP__` ports
always create psets under `pset_node0`. Two distinct logical clusters on a
non-AMP port means two psets under the same node, which star2lte observed to
break scheduler bookkeeping and surface as `Ticket Lock ... unexpectedly owned`.
`cpus=4` avoids it because all four A53s share one logical cluster; `cpus=6`
adds cluster 1 and trips it.

## 6. Recommended next step on our side

Highest value, lowest risk, directly mirrors the published fix:

1. **A/B test: all cores in one logical cluster.** Patch `ml_parse_cpu_topology`
   (the code touched by 0115) so the logical `cluster_id` is taken from the ADT
   `cluster-id` property instead of being derived from MPIDR Aff1, then set
   `cluster-id = 0` for every `cpuN` node in `tools/mkadt_jackpotlte.py`.
   Rebuild ADT + kernel, boot with `cpus=8`. If the ticket lock disappears,
   the cause is confirmed to be pset bookkeeping, not PMU power-up.
2. **Raise the physical-cluster bound defensively.** Set
   `MAX_CPU_CLUSTER_PHY_ID` to 7 for `JACKPOTLTE` as well (star2lte's value),
   not just `MAX_CPU_CLUSTERS=3`. Cheap, removes a latent assertion.
3. **Keep `cpus=4` as the known-good baseline** while testing; do not combine
   with the untested 0136 CV cluster-power changes in the same experiment.
4. If single-cluster boots but multi-cluster topology is wanted later, only then
   introduce per-cluster logical IDs and test `__AMP__` behaviour or the
   pset_node1 path; nothing in prior art says that is required for iOS runtimes.

## 7. Confidence / not found

- Verified: star2lte's documented failure family and its 0149/0150 fix; XNU
  source lines cited; our `cpus=4` works while `cpus=6/8` panics; our ADT tags
  three clusters.
- Claim (strong but unproven on our hardware): star2lte's cluster/pset cause is
  the same as our panic. The stack differs in exact lock address and MPIDR
  (Exynos 9810 vs 7885), but the symptom family matches precisely.
- Not found: any other public project (HakiOS, UniXNU branches, Bergamota/Poco
  F1, dreamlte/S8) reporting this exact hang or publishing a different fix;
  the only direct prior art is `georgev-6974/s9plus-ios`. No evidence found
  that a timer/GIC PPI offset or interrupt-controller bug caused this panic in
  prior work.

## Sources

- `georgev-6974/s9plus-ios` @ `main`, `STAR2LTE_PROGRESS.md`
  (fetched 2026-11-08 via `gh api .../contents/...`).
- XNU reference source: `/tmp/opencode/xnu-xnu-8796.141.3/osfmk/arm64/machine_routines.c`
  lines 1040-1085, 1390-1432 (981... i.e. XNU 8796.141.3).
- Our repo: `JACKPOTLTE_PROGRESS.md`, `xnu/patches/0115-*.patch`,
  `xnu/patches/0134-*.patch`, `tools/mkadt_jackpotlte.py` (read-only).
- Project notes quoted from `dipa-studio/ios-sweet` NOTES.md (code search, 2026-11-08).
- GitHub search log (2026-11-08): `gh search code "pset_find(cluster_id"`,
  `gh search code "DebuggerXCallEnter"`, `gh search code "cluster_initialized"`,
  `gh search repos "HakiOS"/"UniXNU"/"Bergamota"/"jackpotlte"/"dreamlte"`.