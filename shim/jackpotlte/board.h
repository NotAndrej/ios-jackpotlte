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
 *   pmu_cpu_offset() has a CONFIG_SOC_EXYNOS7885 override that numbers the
 *   cores by MPIDR Aff1 (not the generic (Aff1<<2)|Aff0): Aff1=0 (A73
 *   0x0-0x1) -> 0..1, Aff1=2 (A53 0x200-0x201) -> 2..3, Aff1=1 (A53
 *   0x100-0x103) -> 4..7. Patch 0135 uses this table. */
#define JT_PMU_BASE		0x11c80000UL
#define JT_PMU_CPU_CFG		0x2000
#define JT_PMU_CPU_STATUS	0x2004
#define JT_PMU_CPU_STEP		0x80
#define JT_PMU_CPU_PWR		0xf

/* PMU reset / reboot reason (SAM-K drivers/soc/samsung/exynos-reboot.c and the
 * "exynos,reboot { pmu_base = <0x11C80000> }" node in exynos7885.dtsi).
 * exynos_reboot() writes the reason to SYSIP_DAT0 and then 1 to SWRESET. */
#define JT_PMU_SYSIP_DAT0	0x0810U	/* reboot reason / boot command */
#define JT_PMU_SWRESET		0x0400U	/* write 1: SoC software reset */
#define JT_REBOOT_RECOVERY	0xffU	/* REBOOT_MODE_RECOVERY */
#define JT_REBOOT_NORMAL	0x00U	/* REBOOT_MODE_NORMAL */

/* Bring-up canary. The shim has no readable output when DECON is not scanning
 * out the handover framebuffer, so every terminal state (probe done, failure,
 * exception) waits this long and then resets the SoC into the recovery
 * partition. A phone that drops into recovery by itself is proof the shim ran
 * even when the screen showed nothing. Set 0 to disable. */
#define JT_CANARY_MS		8000U

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
 * gpa1 lives in pin-controller 0 (ALIVE), which is pinctrl@11CB0000
 * (SAM-K arch/arm64/boot/dts/exynos/dtb/exynos7885.dtsi:601,
 * reg <0x0 0x11CB0000 0x1000>). Its bank offset is 0x060 from the
 * exynos7885_pin_banks0 table (SAM-K
 * drivers/pinctrl/samsung/pinctrl-exynos.c:1710, "gpa1" reg 0x060) and
 * bank_type_7 places CON/PUD/DRV at +0x00/+0x08/+0x0c with DAT at +0x04
 * (pinctrl-exynos.c:78), so GPA1CON=0x060, GPA1DAT=0x064, GPA1PUD=0x068
 * inside the 4 KiB window. */
#define JT_PINCTRL_GPA1		0x060U	/* gpa1 bank base in pinctrl_0 */
#define JT_GPA1_CON		0x060U
#define JT_GPA1_DAT		0x064U
#define JT_GPA1_PUD		0x068U
#define JT_KEY_UP_BIT		5
#define JT_KEY_DOWN_BIT		6
#define JT_KEY_POWER_BIT	7

/* ---- Framebuffer / display (SAM-K exynos7885-rmem.dtsi + dpu_7885) ----
 * framebuffer@0xEC000000 (compatible "exynos,dpu_fb_handover"), reg size
 * 0x1400000 (20 MiB). dpp_drv.c reads exactly this reg (res[1],res[2]) as the
 * bootloader framebuffer's phy_addr and size.
 * Panel s6e3fa7: resolution 1080x2220, MIPI command mode, 4 data lanes.
 * DECON_F @ 0x14860000, HW_SW_TRIG_CONTROL at offset 0x0070
 * (drivers/video/fbdev/exynos/dpu_7885/regs-decon.h). The panel runs
 * trig_mode=0 (HW trigger) from the DSI TE line. S-Boot leaves that trigger
 * masked, so the panel keeps the last frame until the loader re-arms it:
 * writing HW_TRIG_EN (bit 0) | HW_TRIG_EDGE_POLARITY (bit 7) |
 * TRIG_AUTO_MASK_EN (bit 12) = 0x1281 enables the HW trigger without setting
 * SW_TRIG_EN (bit 8), so DECON keeps scanning the handover framebuffer on each
 * TE. It is the S8 shim's proven trigger and put the loader banner and XNU
 * kprintf on the A8 panel (stage 2/4 photos, 2026-11-08). "Using
 * TRIG_AUTO_MASK_EN=1'b0 is strictly prohibited" (regs-decon.h). */
#define JT_HAVE_FB		1
#define JT_FB_BASE		0xec000000UL
#define JT_FB_WIDTH		1080U
#define JT_FB_HEIGHT		2220U
#define JT_FB_STRIDE		(JT_FB_WIDTH * 4)
#define JT_DECON_BASE		0x14860000UL
#define JT_DECON_TRIG_OFF	0x0070U	/* HW_SW_TRIG_CONTROL */
#define JT_DECON_TRIG		(JT_DECON_BASE + JT_DECON_TRIG_OFF)
#define JT_DECON_TRIG_UNMASK	0x1281U	/* HW_TRIG_EN|EDGE_POLARITY|AUTO_MASK */

/* ---- ramoops / pstore (the text log channel) ----
 * Derived from the ROM's own kernel: it is Eureka 4.4.302-p6 (uname on the
 * phone), i.e. this tree. /proc/config.gz on the phone says
 *   CONFIG_EXYNOS_SNAPSHOT=y, _PSTORE=y, _HOOK_LOGGER=y, _SFRDUMP=y,
 *   MINIMIZED_MODE not set, EXYNOS_CORESIGHT_ETR not set,
 *   CONFIG_PSTORE=y, PSTORE_RAM=y, PSTORE_CONSOLE=y, PSTORE_PMSG=y.
 * drivers/trace/exynos-ss.c then lays the ESS region out from ess_setup=
 * 0x86000000 (S-Boot's /chosen bootargs) in ess_items[] order, each item
 * taking its size because entry.enabled_init is true and persist is true for
 * log_pstore (so exynos_ss_fixmap does not memset it):
 *   log_kevents   0x800000  @ +0x0000000
 *   log_kernel    0x200000  @ +0x0800000
 *   log_platform  0x400000  @ +0x0a00000   (HOOK_LOGGER)
 *   log_sfr       0x400000  @ +0x0e00000   (SFRDUMP)
 *   log_pstore    0x008000  @ +0x1200000   (32 KiB)
 *   -> 0x86000000 + 0x1200000 = 0x87200000.
 * ess_pstore_init() hands that zone to ramoops with mem_size = 32 KiB,
 * pmsg_size = 16 KiB, record_size = 16 KiB, dump_oops = 1, no console zone and
 * no ECC. fs/pstore/ram.c allocates the dump records first, then console
 * (size 0, skipped), then ftrace (size 0), then pmsg, so:
 *   dmesg record zone (PRZ "DBGC") 0x87200000, 16 KiB
 *   pmsg zone  (PRZ "DBGC")        0x87204000, 16 KiB
 * A record written here becomes /sys/fs/pstore/dmesg-ramoops-0 on the next
 * boot; AOSP's PstoreManager copies it to dropbox as SYSTEM_LAST_KMSG, which
 * `adb shell dumpsys dropbox --print` shows without root. Reading it in
 * Lineage recovery's root shell works too. */
#define JT_HAVE_RAMOOPS		1
#define JT_RAMOOPS_CONSOLE	0x87200000UL	/* dmesg record 0 */
#define JT_RAMOOPS_PMSG		0x87204000UL	/* pmsg */
#define JT_RAMOOPS_SIZE		0x4000UL	/* 16 KiB per zone */

/* XNU's start_first_cpu writes an 8-byte entry marker into the loader's
 * pstore text (A5_ENTRY_MARKER_PHYS). Derived from the ramoops base once
 * known; 0 disables the marker. */
#define JT_ENTRY_MARKER_PHYS	0UL		/* TODO: JT_RAMOOPS_CONSOLE + offset */

/* DMA scratch for a storage probe (outside the XNU window). The high bank
 * ends at 0x8c0000000; keep the scratch 16 MiB below the top. */
#define JT_PROBE_DMA		0x8ff00000UL

#endif /* JACKPOTLTE_BOARD_H */
