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

/* ---- RAM (MAINLINE exynos7885-jackpotlte.dts memory@80000000) ----
 *   low   0x80000000 + 0x3da00000  (ends 0xbda00000)
 *   mid   0xc0000000 + 0x40000000  (ends 0x100000000)
 *   high  0x880000000 + 0x80000000 (ends 0x900000000)
 * The high bank is where the XNU window goes, mirroring dreamlte. */
#define JT_LOW_RAM_BASE		0x80000000UL
#define JT_LOW_RAM_END		0xbda00000UL
#define JT_XNU_RAM_BASE		0x880000000UL
#define JT_XNU_RAM_END		0x900000000UL

/* ---- CPU (MAINLINE exynos7885.dtsi cpus) ----
 * 6x Cortex-A53: MPIDR 0x100..0x103 and 0x200..0x201 (two clusters)
 * 2x Cortex-A73: MPIDR 0x0..0x1
 * S-Boot is expected to boot MPIDR 0x100 (same family as dreamlte/A5). */
#define JT_BOOT_MPIDR		0x100

/* ---- GIC (MAINLINE exynos7885.dtsi gic@12301000) ----
 * GIC-400: distributor, CPU interface, virtual interface, virtual CPU. */
#define JT_GICD_BASE		0x12301000UL
#define JT_GICC_BASE		0x12302000UL
#define JT_GICH_BASE		0x12304000UL
#define JT_GICV_BASE		0x12306000UL

/* ---- PMU (MAINLINE exynos7885.dtsi pmu@11c80000) ----
 * The secondary-core power-up sequence (CPUn_CONFIGURATION etc.) must be
 * copied from SAM-K drivers/soc/samsung/exynos-pmu.c for Exynos7885. */
#define JT_PMU_BASE		0x11c80000UL

/* ---- Timer (MAINLINE exynos7885.dtsi timer + jackpotlte oscclk) ----
 * arm,armv8-timer: PPI 13 (secure phys), 14 (non-secure phys = CNTP),
 * 11 (virtual), 10 (hyp). oscclk 26 MHz. */
#define JT_TIMEBASE		26000000UL
#define JT_PHYS_TIMER_INTID	30	/* PPI 14 -> INTID 16 + 14 */
#define JT_VIRT_TIMER_INTID	27	/* PPI 11 -> INTID 16 + 11 */

/* ---- Storage (MAINLINE exynos7885.dtsi mmc_0@13500000) ----
 * eMMC, NOT UFS: "samsung,exynos7885-dw-mshc-smu". The A5XELTE port's
 * bsd/dev/a5xelte_emmc.c is the starting point; CIU timing and card-detect
 * come from SAM-K / MAINLINE (samsung,dw-mshc-*-timing). */
#define JT_EMMC_BASE		0x13500000UL

/* ---- Pinctrl (MAINLINE exynos7885.dtsi) ----
 * alive 0x11cb0000 (keys/UART), top 0x139b0000, fsys 0x13430000. */
#define JT_PINCTRL_ALIVE	0x11cb0000UL

/* ---- Buttons (MAINLINE exynos7885-jackpotlte.dts gpio-keys) ----
 * All active low on GP1 (pinctrl_alive):
 *   Volume Up   gpa1-5
 *   Volume Down gpa1-6
 *   Power       gpa1-7 (wakeup-source)
 * Wait - "gpa1" is the label in the DTS; the physical bank/offset must be
 * confirmed against MAINLINE exynos7885-pinctrl.dtsi + the live DT. */
#define JT_KEY_UP_BIT		5
#define JT_KEY_DOWN_BIT		6
#define JT_KEY_POWER_BIT	7

/* ---- Framebuffer / display (DECON) ----
 * UNKNOWN for jackpotlte. dreamlte used s3cfb.bootloaderfb = 0xcc000000 with
 * a 1440x2960 panel; jackpotlte is a 1080x2220 panel but the bootloader FB
 * base and the DECON base/trigger are NOT known. Read them from:
 *   - LIVE-DT: the S-Boot framebuffer node / chosen bootargs
 *     (s3cfb.bootloaderfb, or a reserved-memory framebuffer node), and
 *   - SAM-K: drivers/video/fbdev/exynos/decon/... register map, and
 *     DECON_F HW_SW_TRIG_CONTROL for the trigger unmask.
 *
 * Until then the screen console is compiled out (JT_HAVE_FB 0); the loader
 * still performs its real work (payload copy + jump). Do NOT guess an
 * address and write to it: with the MMU off that is a silent hang. */
#define JT_HAVE_FB		0
#define JT_FB_BASE		0UL		/* TODO(LIVE-DT/SAM-K) */
#define JT_FB_WIDTH		1080U		/* MAINLINE-ish panel size, unconfirmed */
#define JT_FB_HEIGHT		2220U		/* TODO(LIVE-DT): exact panel */
#define JT_FB_STRIDE		(JT_FB_WIDTH * 4)
#define JT_DECON_TRIG		0UL		/* TODO(SAM-K): DECON_F trig control */
#define JT_DECON_TRIG_UNMASK	0U		/* TODO(SAM-K) */

/* ---- ramoops / pstore (log channel) ----
 * UNKNOWN for jackpotlte. dreamlte kept two copies in TWRP's ramoops zones
 * (console 0x92080000, pmsg 0x92180000). Read jackpotlte's from:
 *   - LIVE-DT: the ramoops reserved-memory node (reg = base/size, and the
 *     "pmsg" / "console" sub-zones), or TWRP's /proc/iomem + /sys/fs/pstore.
 * Until then the pstore console is compiled out (JT_HAVE_RAMOOPS 0). */
#define JT_HAVE_RAMOOPS		0
#define JT_RAMOOPS_CONSOLE	0UL		/* TODO(LIVE-DT) */
#define JT_RAMOOPS_PMSG		0UL		/* TODO(LIVE-DT) */
#define JT_RAMOOPS_SIZE		0x80000UL	/* dreamlte's zone size; unconfirmed */

/* XNU's start_first_cpu writes an 8-byte entry marker into the loader's
 * pstore text (A5_ENTRY_MARKER_PHYS). Derived from the ramoops base once
 * known; 0 disables the marker. */
#define JT_ENTRY_MARKER_PHYS	0UL		/* TODO: JT_RAMOOPS_CONSOLE + offset */

/* DMA scratch for a storage probe (outside the XNU window). dreamlte used
 * 0x8f0000000 in the high bank. jackpotlte's high bank is 0x880000000-
 * 0x900000000, so keep the scratch at its top minus 16 MiB. */
#define JT_PROBE_DMA		0x8ff00000UL

#endif /* JACKPOTLTE_BOARD_H */
