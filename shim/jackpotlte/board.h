/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy A8 2018 (SM-A530F, jackpotlte, Exynos 7885) board facts.
 *
 * EVERY value here is either (a) sourced from a public reference, named in
 * the comment, or (b) an explicit TODO with the place to read it from. Do not
 * replace a TODO with a guess: put the readback in docs/jackpotlte-bringup.md
 * first, then update this file.
 *
 * Sources
 *   MAINLINE: arch/arm64/boot/dts/exynos/exynos7885.dtsi and
 *             exynos7885-jackpotlte.dts (the mainline device tree).
 *   SAM-K:    Samsung's open-source 7885 kernel for this model (not yet
 *             obtained; the only authority for DECON/PMIC/ramoops/bootloader).
 *   LIVE-DT:  the device's own DT, `dd if=/sys/firmware/fdt` in TWRP, decoded
 *             with dtc. This is the authority for what S-Boot actually leaves.
 *   PHONE:    a direct readback/photo from the device.
 */
#ifndef JACKPOTLTE_BOARD_H
#define JACKPOTLTE_BOARD_H

/* ---- RAM (SAM-K exynos7885-jackpotlte_common.dtsi memory nodes) ----
 *   low   0x80000000 + 0x3da00000  (ends 0xbda00000)
 *   mid   0xc0000000 + 0x40000000  (ends 0x100000000)
 *   high  0x880000000 + 0x40000000 (ends 0x8c0000000)
 * The mainline jackpotlte DTS lists the high bank as 0x80000000 (2 GiB); the
 * Samsung 4.4 tree for this board says 0x40000000 (1 GiB). The XNU window is
 * 512 MiB inside the bank either way, so both are safe.
 * S-Boot's own /chosen has linux,initrd = 0x84000000..0x840fffff. */
#define JT_LOW_RAM_BASE		0x80000000UL
#define JT_LOW_RAM_END		0xbda00000UL
#define JT_XNU_RAM_BASE		0x880000000UL
#define JT_XNU_RAM_END		0x8c0000000UL

/* ---- CPU (SAM-K exynos7885.dtsi cpus: logical-id 0..7) ----
 * 6x Cortex-A53: MPIDR 0x100..0x103 (cluster 0) and 0x200..0x201 (cluster 1)
 * 2x Cortex-A73: MPIDR 0x0..0x1 (cluster 2)
 * S-Boot boots MPIDR 0x100 (same family as dreamlte/A5); confirmed by the
 * loader banner in stage 2, not yet observed. */
#define JT_BOOT_MPIDR		0x100

/* ---- GIC (SAM-K exynos7885.dtsi gic@123001000) ----
 * GIC-400: distributor, CPU interface, virtual interface, virtual CPU. */
#define JT_GICD_BASE		0x12301000UL
#define JT_GICC_BASE		0x12302000UL
#define JT_GICH_BASE		0x12304000UL
#define JT_GICV_BASE		0x12306000UL

/* ---- PMU (SAM-K exynos7885.dtsi system-controller@11c80000 and
 * drivers/soc/samsung/exynos-pmu.c) ----
 * Secondary-core power-up (Exynos7885 uses a quirky core numbering):
 *   PMU_CPU_CONFIG_BASE 0x2000, PMU_CPU_STATUS_BASE 0x2004,
 *   step 0x80 per core, CPU_LOCAL_PWR_CFG 0xF.
 *   cluster 0 -> cpuid 0..1; cluster 1 -> cpuid 4..5; cluster 2 -> cpuid 6..7. */
#define JT_PMU_BASE		0x11c80000UL
#define JT_PMU_CPU_CFG		0x2000
#define JT_PMU_CPU_STATUS	0x2004
#define JT_PMU_CPU_STEP		0x80
#define JT_PMU_CPU_PWR		0xf

/* ---- Timer (SAM-K exynos7885.dtsi timer) ----
 * arm,armv8-timer, clock-frequency 26000000. The arch timer PPIs (with the
 * DT GIC numbering): secure phys 13, non-secure phys 14, virtual 11, hyp 10.
 * INTID = 16 + PPI. */
#define JT_TIMEBASE		26000000UL
#define JT_PHYS_TIMER_INTID	30	/* PPI 14 -> INTID 16 + 14 */
#define JT_VIRT_TIMER_INTID	27	/* PPI 11 -> INTID 16 + 11 */

/* ---- Storage (SAM-K exynos7885.dtsi dwmmc_0@13500000) ----
 * eMMC, NOT UFS: "samsung,exynos-dw-mshc", reg 0x13500000+0x2000 plus a
 * command-queue window 0x13508000+0x200 and an SMU (exynos-smu) block.
 * The A5XELTE port's bsd/dev/a5xelte_emmc.c (Samsung dw-mshc) is the base. */
#define JT_EMMC_BASE		0x13500000UL
#define JT_EMMC_CMDQ		0x13508000UL

/* ---- Pinctrl (SAM-K exynos7885.dtsi) ----
 * pinctrl_0 (alive) 0x11CB0000, pinctrl_3 (top) 0x139B0000. */
#define JT_PINCTRL_ALIVE	0x11cb0000UL

/* ---- Buttons (SAM-K exynos7885-jackpotlte_common.dtsi gpio-keys on gpa1) ----
 * All active low, interrupt-parent gpa1, flag 0xf:
 *   Volume Down gpa1-6
 *   Volume Up   gpa1-5
 *   Power       gpa1-7
 * The gpa1 bank offset inside pinctrl_0 must be read from the driver's bank
 * table (SAM-K drivers/pinctrl/samsung/) or the live DT before use. */
#define JT_KEY_UP_BIT		5
#define JT_KEY_DOWN_BIT		6
#define JT_KEY_POWER_BIT	7

/* ---- Framebuffer / display (SAM-K exynos7885-rmem.dtsi + dpu_7885) ----
 * framebuffer@0xEC000000 (compatible "exynos,dpu_fb_handover"), reg size
 * 0x1400000 (20 MiB). dpp_drv.c reads exactly this reg (res[1],res[2]) as the
 * bootloader framebuffer's phy_addr and size.
 * Panel s6e3fa7: resolution 1080x2220, MIPI command mode, 4 data lanes.
 * DECON_F @ 0x14860000, HW_SW_TRIG_CONTROL at offset 0x0070; the panel runs
 * trig_mode=0 (HW trigger) from the DSI TE line, so the loader must NOT force
 * a software trigger before XNU drives the display. */
#define JT_HAVE_FB		1
#define JT_FB_BASE		0xec000000UL
#define JT_FB_WIDTH		1080U
#define JT_FB_HEIGHT		2220U
#define JT_FB_STRIDE		(JT_FB_WIDTH * 4)
#define JT_DECON_BASE		0x14860000UL
#define JT_DECON_TRIG_OFF	0x0070U	/* HW_SW_TRIG_CONTROL */
#define JT_DECON_TRIG		0UL	/* do not poke DECON before XNU */

/* ---- ramoops / pstore (log channel) ----
 * Samsung 4.4: the pstore/ramoops zone comes from the exynos-snapshot (ESS)
 * region named by the bootargs ess_setup=0x86000000 ("log_pstore" item), set
 * up in drivers/trace/exynos-ss.c. The exact sub-offset of the "log_pstore"
 * item inside the ESS region must be read from exynos-ss.c's ESS layout for
 * 7885 (or from the live DT / TWRP /proc/iomem). Until then the pstore
 * console stays off; the screen console above is the stage-2 channel. */
#define JT_HAVE_RAMOOPS		0
#define JT_RAMOOPS_CONSOLE	0UL		/* TODO(SAM-K exynos-ss.c / LIVE-DT) */
#define JT_RAMOOPS_PMSG		0UL		/* TODO(SAM-K exynos-ss.c / LIVE-DT) */
#define JT_RAMOOPS_SIZE		0x80000UL	/* dreamlte's zone size; unconfirmed */

/* XNU's start_first_cpu writes an 8-byte entry marker into the loader's
 * pstore text (A5_ENTRY_MARKER_PHYS). Derived from the ramoops base once
 * known; 0 disables the marker. */
#define JT_ENTRY_MARKER_PHYS	0UL		/* TODO: JT_RAMOOPS_CONSOLE + offset */

/* DMA scratch for a storage probe (outside the XNU window). The high bank
 * ends at 0x8c0000000; keep the scratch 16 MiB below the top. */
#define JT_PROBE_DMA		0x8ff00000UL

#endif /* JACKPOTLTE_BOARD_H */
