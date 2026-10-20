/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy S8 display pipeline probe, run by the loader (MMU off).
 *
 * Goal: show iOS frames without the CPU copy into the S-Boot framebuffer,
 * by pointing the DPU's input DMA at the frame instead (a "flip").
 * Exynos 8895 DPU (Samsung kernel drivers/video/fbdev/exynos/dpu): DECON_F
 * 0x12860000 (SHADOW_REG_UPDATE_REQ 0x60, WIN_CONTROL_0(w) 0x1000 + w*0x30)
 * fed by six DPP channels whose IDMA registers are the second DT reg block
 * (0x128b1000 + n*0x1000, n = 0..5; the sixth DPP's at 0x128b7000):
 * IDMA_ENABLE 0x00, IN_CON 0x08, SRC_SIZE 0x10, SRC_OFFSET 0x14,
 * IMG_SIZE 0x18, IN_BASE_ADDR_Y 0x40. The DT says default_idma = 0.
 *
 * Read-only dump, then one visible test on the channel that scans the
 * S-Boot framebuffer: its base moves 800 lines down for 3 s (the picture
 * jumps up), then it is restored.
 */
#include <stdint.h>
#include "display_probe.h"

#define DECON			0x12860000UL
#define SHADOW_REQ		0x60
#define SHADOW_GLOBAL		(1u << 31)
#define FB_PHYS			0xcc000000u
#define FB_STRIDE		(1440u * 4)
#define TIMER_HZ		26000000UL

static const uintptr_t idma[6] = {
	0x128b1000, 0x128b2000, 0x128b3000, 0x128b4000, 0x128b5000, 0x128b7000,
};
static const uintptr_t sysmmu[3] = { 0x12900000, 0x12910000, 0x12920000 };

static inline uint32_t rd(uintptr_t a) { return *(volatile uint32_t *)a; }
static inline void wr(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }

static uint64_t ticks(void)
{
	uint64_t v;
	__asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(v));
	return v;
}

static void delay_ms(unsigned ms)
{
	uint64_t end = ticks() + TIMER_HZ / 1000 * ms;
	while (ticks() < end)
		;
}

/* decon_reg_all_win_shadow_update_req + global (Samsung decon_reg.c). */
static void update(void)
{
	wr(DECON + SHADOW_REQ, rd(DECON + SHADOW_REQ) | SHADOW_GLOBAL | 0x3f);
	for (unsigned n = 0; n < 100 && rd(DECON + SHADOW_REQ); n++)
		delay_ms(1);
}

/* IDMA_ENABLE / DPP_ENABLE bit 4: SFR_UPDATE_FORCE (dpp_reg.c). */
static void force(unsigned n)
{
	static const uintptr_t dpp[6] = {
		0x12851000, 0x12852000, 0x12853000, 0x12854000, 0x12855000, 0x12890000,
	};
	wr(idma[n], rd(idma[n]) | (1u << 4));
	wr(dpp[n], rd(dpp[n]) | (1u << 4));
}

void display_probe(void (*kv)(const char *, uint64_t), void (*out)(const char *))
{
	out("DISPLAY probe (DECON_F + IDMA)\n");
	kv("decon 0x0", rd(DECON + 0x0));
	kv("0x60", rd(DECON + SHADOW_REQ));
	kv("0x70", rd(DECON + 0x70));
	kv("dp0", rd(DECON + 0x214));
	out("\n");
	for (unsigned w = 0; w < 6; w++) {
		uint32_t c = rd(DECON + 0x1000 + w * 0x30);
		if (c) {
			kv("win", w);
			kv("ctl0", c);
			kv("ctl1", rd(DECON + 0x1004 + w * 0x30));
			out("\n");
		}
	}
	int scan = -1;
	for (unsigned n = 0; n < 6; n++) {
		uintptr_t b = idma[n];
		uint32_t base = rd(b + 0x40);
		kv("idma", n);
		kv("en", rd(b + 0x00));
		kv("in", rd(b + 0x08));
		kv("src", rd(b + 0x10));
		kv("off", rd(b + 0x14));
		kv("img", rd(b + 0x18));
		kv("Y", base);
		out("\n");
		if (base == FB_PHYS && scan < 0)
			scan = (int)n;
	}
	if (scan >= 0) {
		kv("flip test on idma", (uint64_t)scan);
		uintptr_t y = idma[scan] + 0x40;
		out("\nTEST A: window+global update. PICTURE JUMPS UP NOW?\n");
		wr(y, FB_PHYS + FB_STRIDE * 800);
		update();
		delay_ms(3000);
		wr(y, FB_PHYS);
		update();
		delay_ms(1000);
		out("TEST B: + IDMA/DPP SFR force. PICTURE JUMPS UP NOW?\n");
		wr(y, FB_PHYS + FB_STRIDE * 800);
		force((unsigned)scan);
		update();
		delay_ms(3000);
		wr(y, FB_PHYS);
		force((unsigned)scan);
		update();
		kv("restored Y", rd(y));
		kv("idma en", rd(idma[scan]));
		out("\nTESTS DONE\n");
	} else {
		out("no IDMA reads 0xcc000000: no flip test\n");
	}
	/* Last: an IOMMU whose clock is gated could stall the bus. */
	for (unsigned s = 0; s < 3; s++) {
		kv("sysmmu", sysmmu[s]);
		kv("ctrl", rd(sysmmu[s] + 0x0));
		kv("cfg", rd(sysmmu[s] + 0x4));
		out("\n");
	}
	out("DISPLAY probe done\n");
}
