# Samsung Exynos 7885 secondary CPU power-up

Research-only note for the A8 (jackpotlte) port. Covers why secondaries in an unpowered cluster do not report alive, and the documented PMU/PSCI sequence available in public sources.

### Verifier's note (2026-11-08)

Checked against `torvalds/linux` and Samsung's 4.4 `exynos-pmu.c`:

- Confirmed: PSCI node and `cpu_on = <0xc4000003>`; all 8 CPUs are `enable-method = "psci"`.
  (Upstream's node is `compatible = "arm,psci"`, not `arm,psci-1.0` — cosmetic, but
  quote it as written in the DT.)
- Confirmed exactly: `PMU_CPU_CONFIG_BASE 0x2000`, `PMU_CPU_STATUS_BASE 0x2004`,
  `CPU_LOCAL_PWR_CFG 0xF`, `CLUSTER_ADDR_OFFSET 0x20`, `PMU_CPUSEQ_OPTION_BASE 0x2488`,
  `PMU_NONCPU_STATUS_BASE 0x2404`, `PMU_L2_STATUS_BASE 0x2604`, `NONCPU_LOCAL_PWR_CFG 0xF`,
  `L2_LOCAL_PWR_CFG 0x7`, `PMU_CPU_RESET_BASE 0x200C`, and
  `DISABLE_RESET = BIT(12)|BIT(9)|BIT(8) = 0x1300`.
- Confirmed: `exynos_cluster_up()` is `pmu_cpuseq_ctrl(cpu, false)` — **clear** bit 0.
- **Correction 1 — the reset write has no caller.** `exynos_cpu_reset_enable()` and
  `exynos_cpu_reset_disable()` are defined and never called anywhere in the vendor
  tree (`grep -rn` over `arch/arm64/` and `drivers/` finds only the definitions).
  Samsung's Linux relies on PSCI firmware for secondary reset, not on these helpers.
  So §6 step 3 (`|= 0x1300`) is **our** invention layered on unused code, not a
  vendor-required step, and writing it may fight the firmware.
- **Correction 2 — vendor power-up is a single write.** `exynos_cpu_up()` is
  `pmu_cpu_ctrl(cpu, 1)` → one `regmap_update_bits(cfg, 0xF, 0xF)`. There is **no**
  clear-then-poll-zero-then-set sequence and no polling in this file. §6's two-step is
  inference from another Samsung SoC family, not 7885 vendor code.
- Net: the *ordering* (cluster seq → non-CPU/L2 → core cfg → PSCI `CPU_ON`) is sound,
  but every step after "clear CPUSEQ bit 0" is unproven on this SoC. The safest next
  test is PSCI-only (no PMU writes beyond CPUSEQ) to see whether firmware already
  handles the cold cluster — that is what Linux does.

## 1. Summary

- **Mechanism used upstream and in XNU: PSCI CPU_ON over SMC.** The mainline Exynos7885 device tree puts all 8 CPUs under `enable-method = "psci"` and has a `psci { compatible = "arm,psci"; method = "smc"; cpu_on = <0xc4000003>; ... }` node. Linux arm64 starts secondaries through `cpu_psci_ops`; there is **no** upstream `arch/arm64/kernel/mcpm-exynos.c` and no `drivers/firmware/mcpm*` reference in the current upstream tree. Samsung's proprietary monitor/firmware implements PSCI.
- **Vendor 4.4 direct path:** `drivers/soc/samsung/exynos-pmu.c` (Eureka/SAM-K) exposes the exact PMU core/cluster power offsets and helpers such as `exynos_cluster_up()` and `exynos_cpu_reset_disable()`. It is gated by `CONFIG_SOC_EXYNOS7885`.
- **The Exynos7885 PMU slot mapping is table-driven by `pmu_cpu_offset()` in vendor code and is not linear `(aff1 << 2)|aff0`.** It compensates for the fact that MPIDR Aff1=1 is the 4-core A53 cluster while Aff1=2 is only 2 cores.
- **Cold-cluster sequencing is the suspected failure.** Only the boot A53 cluster (MPIDR `0x100-0x103`, slot block 4-7) comes up under Linux/XNU without extra action. For the other two clusters vendor code requires `exynos_cluster_up()` first: clear bit 0 of `CPUSEQ_OPTION` (`0x2488 + cluster*0x20`) before programming a core in that cluster.
- **XNU's 0135/0136 implementation reflects this:** trampoline at `0x8f000000`, PMU core local power at slot `0x2000 + slot*0x80`, optional reset deassert at `0x200c + slot*0x80`, CPUSEQ bit clear at `0x2488 + cluster*0x20`, then PSCI CPU_ON, with logs of `noncpu=...` and `l2=...`. As of the repo progress notes, `cpus=8` after 0136 is still not yet verified on hardware.

## 2. Upstream Linux (primary source)

### 2.1 Exynos7885 CPU bring-up method

- **File:** `arch/arm64/boot/dts/exynos/exynos7885.dtsi`, current upstream master (Linux 7.4-rc1 snapshot, fetched 2026-11-08)
- **CPU nodes:** MPIDRs `0x100`, `0x101`, `0x102`, `0x103`, `0x200`, `0x201`, `0x0`, `0x1`; every node has `enable-method = "psci";`.
- **PSCI node:** lines 150-154:
  ```dts
  psci {
      compatible = "arm,psci";
      method = "smc";
      cpu_suspend = <0xc4000001>;
      cpu_off = <0x84000002>;
      cpu_on = <0xc4000003>;
  };
  ```
- **PMU system controller:** lines 298-301:
  ```dts
  pmu_system_controller: system-controller@11c80000 {
      compatible = "samsung,exynos7885-pmu",
                   "samsung,exynos7-pmu", "syscon";
      reg = <0x11c80000 0x10000>;
  };
  ```
- **Binding:** `Documentation/devicetree/bindings/soc/samsung/exynos-pmu.yaml`, line 37 accepts `samsung,exynos7885-pmu` with fallback to `samsung,exynos7-pmu` and then `syscon`. The requested `Documentation/devicetree/bindings/arm/samsung/pmu.yaml` path does not exist in this tree.

Implication: upstream treats 7885 as a normal PSCI system. The kernel does not write Exynos PMU core power registers directly during secondary boot; firmware is expected to implement `CPU_ON` and the cluster power state machine. The current upstream `drivers/soc/samsung/exynos-pmu.c` has no `exynos_cpu_power_up`, no `CPUSEQ_OPTION`, and no `DISABLE_RESET` symbol for 7885.

### 2.2 PSCI discovery on the phone

- XNU log from the progress notes: `A5XELTE CPU: cpu=1 CPU_ON target=0x101 entry_pa=0x... context=1 ... PSCI CPU_ON rs=0` for a secondary in the same A53 cluster. This is a successful firmware ACK, while `mpidr=0x101 NOT alive after 10s`.
- Therefore S-Boot firmware does implement PSCI `CPU_ON` on A8. Public Trusted Firmware-A sources do not appear to contain a Samsung Exynos 7885 platform; the exact monitor implementation is proprietary. The observed interface is only the PSCI ABI from DTS/SMC IDs.

## 3. Samsung vendor 4.4 path (Eureka / Eureka-Kernel master)

Base: `eurekadevelopment/Eureka-Kernel`, branch `master`.

- **Source file:** `drivers/soc/samsung/exynos-pmu.c` (393 lines), function `exynos_pmu_probe()` at lines 349-367 uses:
  - `syscon_regmap_lookup_by_phandle(dev->of_node, "samsung,syscon-phandle")`
  - compatible `samsung,exynos-pmu`

- **Generic mapping (not used for 7885):** lines 54-58:
  ```c
  #define pmu_cpu_offset(mpidr) \
      ((MPIDR_AFFINITY_LEVEL(mpidr, 1) << 2 \
       | MPIDR_AFFINITY_LEVEL(mpidr, 0)) \
       * 0x80)
  ```
- **7885 override:** lines 59-83:
  ```c
  #define CLUSTER0_CORES_CNT (2)
  #define CLUSTER2_CORES_CNT (2)
  unsigned int pmu_cpu_offset(unsigned int mpidr)
  {
      unsigned int cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
      ...
      switch (cluster) {
      case 1:
          cpuid += CLUSTER2_CORES_CNT;   /* 2 */
      case 2:
          cpuid += CLUSTER0_CORES_CNT;   /* 2 */
      case 0:
          cpuid += MPIDR_AFFINITY_LEVEL(mpidr, 0);
          break;
      }
      offset = 0x80 * cpuid;
      return offset;
  }
  ```
- **Core local registers:** lines 84-107:
  - `PMU_CPU_CONFIG_BASE = 0x2000`
  - `PMU_CPU_STATUS_BASE = 0x2004`
  - `CPU_LOCAL_PWR_CFG = 0xF`
  - `pmu_cpu_ctrl(cpu, enable)`:
    ```c
    offset = pmu_cpu_offset(mpidr);
    regmap_update_bits(pmureg, PMU_CPU_CONFIG_BASE + offset,
                       CPU_LOCAL_PWR_CFG,
                       enable ? CPU_LOCAL_PWR_CFG : 0);
    ```
    So write `0xF` to config to power a core up, and write 0 to clear/mask down.
  - `pmu_cpu_state(cpu)` reads `PMU_CPU_STATUS_BASE + offset` and returns true only if `(val & 0xF) == 0xF`.
- **Cluster registers:** lines 109-149:
  - `CLUSTER_ADDR_OFFSET = 0x20`
  - `PMU_CPUSEQ_OPTION_BASE = 0x2488` (CPUSEQ_OPTION)
  - `PMU_NONCPU_STATUS_BASE = 0x2404`
  - `PMU_L2_STATUS_BASE = 0x2604`
  - `NONCPU_LOCAL_PWR_CFG = 0xF`
  - `L2_LOCAL_PWR_CFG = 0x7`
  - `pmu_cpuseq_ctrl(cpu, enable)`:
    ```c
    offset = phy_cluster(cpu) * CLUSTER_ADDR_OFFSET; /* Aff1 * 0x20 */
    regmap_update_bits(pmureg,
        PMU_CPUSEQ_OPTION_BASE + offset, 1, enable);
    ```
    Note the polarity: `enable == true` sets bit0 (`exynos_cluster_down`), while `enable == false` clears bit0 (`exynos_cluster_up`).
  - `pmu_noncpu_state()` returns true if `(noncpu_stat & 0xF) != 0`.
  - `pmu_l2_state()` returns true if `(l2_stat & 0x7) != 0`.
- **Cluster helpers:** lines 166-188:
  - `exynos_cluster_up(unsigned int cpu) { pmu_cpuseq_ctrl(cpu, false); }`
  - `exynos_cluster_down(unsigned int cpu) { pmu_cpuseq_ctrl(cpu, true); }`
  - `exynos_cluster_state(unsigned int cpu) { return pmu_l2_state(cpu) && pmu_noncpu_state(cpu); }`
- **Core reset registers:** lines 191-216:
  - `PMU_CPU_RESET_BASE = 0x200C`
  - `DISABLE_WDT_CPUPORESET = BIT(12)`
  - `DISABLE_CORERESET = BIT(9)`
  - `DISABLE_CPUPORESET = BIT(8)`
  - `DISABLE_RESET = BIT(8) | BIT(9) | BIT(12) = 0x1300`
  - `exynos_cpu_reset_enable(cpu)` writes mask `0x1300`, val `0` (clears bits, asserts reset)
  - `exynos_cpu_reset_disable(cpu)` writes mask `0x1300`, val `0x1300` (sets bits, keeps core in the not-in-reset state)
- **Wait/poll helper:** lines 317-336 uses a 2000 ms timeout waiting for all outgoing cores to satisfy `cpu_is_offline(cpu) && !exynos_cpu_state(cpu)`.

**Important caveat:** this vendor driver does **not** itself issue PSCI calls for secondary bring-up. `exynos_cluster_up()` only toggles CPUSEQ bit 0, and the core path only toggles `0xF`. On 7885 the normal world must mirror the vendor PMU interactions around PSCI CPU_ON.

## 4. Mapping table: MPIDR to PMU slot

Derived directly from Eureka `pmu_cpu_offset()` and XNU's 0135 mirror: `aff1 == 1 ? 4+aff0 : aff1 == 2 ? 2+aff0 : aff0`.

| MPIDR block | Hardware role in DTS/mainline | Aff1 cluster | Aff0 core | PMU core slot | Core config/status | Reset register |
|---|---|---:|---:|---:|---|---|
| `0x0`, `0x1` | 2x Cortex-A73 | 0 | 0-1 | `0`, `1` | `0x2000 + slot*0x80`, `+0x2004` | `0x200c + slot*0x80` |
| `0x100-0x103` | 4x Cortex-A53 (boot cluster) | 1 | 0-3 | `4-7` | same formula | same formula |
| `0x200-0x201` | 2x Cortex-A53 | 2 | 0-1 | `2-3` | same formula | same formula |

Cluster/common register addressing for all three clusters uses **Aff1 as the hardware cluster id**, with one `0x20` stride:

| Aff1 / MPIDR block | Cluster name | `CPUSEQ_OPTION` | non-CPU power status | L2 power status |
|---|---|---|---|---|
| 0 (A73 `0x0-0x1`) | physical A73 cluster | `0x11c82488` | `0x11c82404` | `0x11c82604` |
| 1 (A53 `0x100-0x103`) | boot/primary A53 cluster | `0x11c824a8` | `0x11c82424` | `0x11c82624` |
| 2 (A53 `0x200-0x201`) | second A53 cluster | `0x11c824c8` | `0x11c82444` | `0x11c82644` |

All addresses are `PMU_BASE + ...`, with `PMU_BASE = 0x11c80000`.

### Where the mapping breaks relative to generic Exynos formula

- Generic vendor formula `pmu_cpu_offset() = ((Aff1 << 2) | Aff0) * 0x80` would place:
  - Aff1=1 -> slots 4-7 (matches the 7885 override, no difference here)
  - Aff1=2 -> **slots 8-9** (7885 override maps to 2-3)
  - Aff1=0 -> slots 0-1 (matches)
- S8 dreamlte code assumes consecutive clusters (`0x100-0x103`, then a Mongoose block at `0x0-0x3`). The 7885 override deliberately reorders the small second A53 cluster (Aff1=2) to a low slot hole at 2-3. Because 7885 has no second 4-core cluster, the generic formula would leave 8-9 outside a sane 8-core array.

## 5. What firmware/OS state must be undone

- **Boot cluster:** MPIDR `0x100-0x103` is the cluster Linux/XNU starts on. Its CPUSEQ sequence is treated as already enabled, and `cpus=4` boots. Vendor `core_status_show()` distinguishes boot/nonboot cluster L2/noncpu reporting but does not initialize cluster common config at probe time.
- **Second A53 cluster (`0x200-0x201`):** must be enabled through `exynos_cluster_up()`. Before that, the sequence bit is expected to be **set (1)**, holding the cluster out of sequence. Clearing bit 0 is the only vendor-defined cluster power-up action before core power.
- **A73 cluster (`0x0-0x1`):** also requires the same sequence enable, even though it is Aff1=0 and has no separate non-L2 register write in vendor code. Inference from XNU's `cpus=2` test is that S-Boot leaves it out of sequence until software re-enables it.
- **Core local power:** each core's `0x2000 + slot*0x80` must be off (`& ~0xF`) and then on (`|= 0xF`). Avoid issuing the same edge without polling or timeout.
- **Core reset:** vendor exposes reset control at `0x200c + slot*0x80`. Set `DISABLE_RESET` (`0x1300`) to associate reset with the not-in-reset state. Clearing those bits asserts reset. Whether firmware's PSCI CPU_ON requires XNU to write this directly is unconfirmed; current 0136 does not set it.

## 6. Ordered register-write recipe for a cold, unpowered cluster

All addresses are relative to `PMU_BASE = 0x11c80000`. `cluster = Aff1` (0, 1, or 2). `slot = pmu_cpu_offset(MPIDR) / 0x80`.

```
1. If the cluster is not already enabled:
   addr = PMU_BASE + 0x2488 + cluster*0x20   // CPUSEQ_OPTION
   reg &= ~0x00000001                          // clear bit 0 (exynos_cluster_up)
   DSB SY

2. Observe (do not force) cluster state:
   noncpu = read(PMU_BASE + 0x2404 + cluster*0x20) & 0xF
   l2     = read(PMU_BASE + 0x2604 + cluster*0x20) & 0x7
   // vendor cluster_state() is true only when both are nonzero

3. For the selected core:
   reset_addr = PMU_BASE + 0x200c + slot*0x80
   set masked bits: reset_addr |= 0x1300      // bits 8, 9, 12
   // 0x1300 = DISABLE_CPUPORESET | DISABLE_CORERESET | DISABLE_WDT_CPUPORESET

   cfg_addr = PMU_BASE + 0x2000 + slot*0x80
   sta_addr = PMU_BASE + 0x2004 + slot*0x80
   clear [cfg_addr] &= ~0x0F
   poll [sta_addr] until (val & 0xF) == 0
   write [cfg_addr] |= 0x0F
   poll [sta_addr] until (val & 0xF) == 0xF

4. DSB SY

5. PSCI CPU_ON (SMC), with a 32-bit stub if needed:
   CPU_ON(0xc4000003):
       entry_point = 32-bit low-bank trampoline (XNU uses 0x8f000000 with
                    "ldr x1,#8; br x1; .quad entry")
       context_id  = Linux logical CPU
   Expect rc = 0 (firmware ACK) before polling for alive.
   OS alive timeout (XNU) is 10 s.

6. Shutdown/hotplug reverse order, only after all offline cores are off:
   core config off: cfg_addr &= ~0xF
   wait cfg_addr == 0 or status != 0xF
   exynos_cluster_down(): CPUSEQ_OPTION addr |= 0x1
```

Bit polarity:
- Set (1): core config `0xF`; reset register mask `0x1300`; CPUSEQ bit0 in `exynos_cluster_down()`.
- Cleared (0): pre-edge core config, especially before writing `0xF`; CPUSEQ bit0 in `exynos_cluster_up()`.
- Polled: core status toward `0x0` when turning off and `0xF` when turning on.

## 7. Upstream vs vendor vs inference callouts

- **Upstream documented fact:** 7885 uses PSCI `method = "smc"` and function IDs from DTS; the PMU node is syscon only; binding path is `soc/samsung/exynos-pmu.yaml`.
- **Vendor documented fact:** exact PMU offsets, values `0xF`, `0x7`, `0x1300`, and CPUSEQ_OPTION bit polarity from Eureka `exynos-pmu.c: 59-216`.
- **Inference / XNU-specific:** the assertion that second A53/A73 clusters are left out of sequence or held powered-down by the bootloader is based on XNU's observed `cpus=4` success and `cpus=2` failure, plus vendor wording. No bootloader log here documents S-Boot initial cluster power state. The direct multi-register ordering in this section is XNU's composed recipe, not a single complete vendor routine found in this source.

## 8. Confidence / not confirmed

Confirmed:
- Upstream Linux DTS uses PSCI and function IDs `0xc4000001` / `0x84000002` / `0xc4000003` for 7885.
- Vendor 7885 PMU slot mapping: Aff1=0 -> 0-1, Aff1=1 -> 4-7, Aff1=2 -> 2-3.
- `exynos_cluster_up()` only writes bit 0 to 0 through `PMU_CPUSEQ_OPTION_BASE + cluster*0x20`.
- Core local power values `0xF`, L2 mask `0x7`, non-CPU mask `0xF`, and reset mask `0x1300`.

Not confirmed:
- whether `cpus=8` after 0136 succeeds on hardware (untested as of repo progress 2026-11-08).
- actual S-Boot initial power/reset state of the A73 cluster and second A53 cluster.
- whether setting reset bits `8`, `9`, `12` is needed before PSCI or is redundant with firmware CPU_ON.
- exact required delay for noncpu/L2 rise beyond XNU's polling loops and vendor's 2000 ms CPU down wait.
- existence of a public Samsung TrustedFirmware/A or monitor source for 7885 (none found).

## 9. Cited files/paths

- Upstream: `torvalds/linux @ 7.4-rc1`, `arch/arm64/boot/dts/exynos/exynos7885.dtsi` (lines 94-154, 298-301).
- Upstream binding: `Documentation/devicetree/bindings/soc/samsung/exynos-pmu.yaml` (line 37).
- Vendor: `eurekadevelopment/Eureka-Kernel @ master`, `drivers/soc/samsung/exynos-pmu.c` (lines 54-216).
- XNU port patches: `xnu/patches/0135-jackpotlte-secondary-core-start.patch`, `xnu/patches/0136-jackpotlte-cluster-power.patch`.
- Repo state: `JACKPOTLTE_PROGRESS.md`, "Enabling all 8 cores" and "`cpus=4` boots" sections.
