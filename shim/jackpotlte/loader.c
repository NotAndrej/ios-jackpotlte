/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy A8 2018 (SM-A530F, jackpotlte, Exynos 7885) XNU loader.
 *
 * Adapted from shim/dreamlte/loader.c (Galaxy S8, Exynos 8895). Same S-Boot
 * handoff and same tools/mkpayload.py "SWEETXNU" v1 container, but:
 *   - the 8895 hardware probes (UFS, PMIC, touch, display, battery, Mongoose)
 *     are removed: they touch Exynos 8895 MMIO and are not jackpotlte facts;
 *   - the screen console and the pstore log are compiled out until the A8's
 *     framebuffer base and ramoops zones are read from the live DT / Samsung
 *     source (see shim/jackpotlte/board.h). With the MMU off, writing to a
 *     guessed address is a silent hang, so we refuse to do it.
 *
 * The loader's real job is unchanged: copy the SWEETXNU payload from the BOOT
 * ramdisk into the XNU RAM window named by the ADT, build boot_args and jump to
 * XNU's physical entry with the MMU and caches off.
 */
#include <stdint.h>
#include "board.h"
#include "../fdt.h"
#if JT_HAVE_FB
#include "../font.h"
#endif

#define PAGE_SIZE		4096UL
#define BOOT_LINE_LENGTH	608

/* ---- board aliases (single place to change once facts arrive) ---- */
#define LOW_RAM_BASE		JT_LOW_RAM_BASE
#define LOW_RAM_END		JT_LOW_RAM_END
#define XNU_RAM_BASE		JT_XNU_RAM_BASE
#define XNU_RAM_END		JT_XNU_RAM_END

/* ---- payload (tools/mkpayload.py v1) ---- */
struct payload_seg {
	uint64_t vmaddr, vmsize, file_off, file_size;
};
struct payload {
	char magic[8];
	uint32_t version, nsegs;
	uint64_t virt_base, entry_va, adt_off, adt_size, kernel_end;
	uint64_t phys_base, mem_size;
	char cmdline[BOOT_LINE_LENGTH];
	struct payload_seg segs[];
};
struct boot_video {
	uint64_t v_baseAddr, v_display, v_rowBytes, v_width, v_height, v_depth;
};
struct boot_args {
	uint16_t Revision, Version;
	uint64_t virtBase, physBase, memSize, topOfKernelData;
	struct boot_video Video;
	uint32_t machineType;
	uint64_t deviceTreeP;
	uint32_t deviceTreeLength;
	char CommandLine[BOOT_LINE_LENGTH];
	uint64_t bootFlags, memSizeActual;
};
_Static_assert(__builtin_offsetof(struct payload, segs) == 0x2a8, "payload layout");
_Static_assert(__builtin_offsetof(struct boot_args, CommandLine) == 108, "boot_args layout");
_Static_assert(sizeof(struct boot_args) == 736, "boot_args size");

uint64_t jt_boot_args;	/* read by start.S */

/* ---- console -----------------------------------------------------------
 * Two independent sinks, each gated by a board fact. If neither is known the
 * messages are dropped (the loader still runs; we just cannot show them yet).
 */
#if JT_HAVE_RAMOOPS
struct prz {
	uint32_t sig, start, size;
	uint8_t data[];
};
#define PRZ_SIG			0x43474244U	/* "DBGC" */
#define PRZ_DATA_MAX		(JT_RAMOOPS_SIZE - 12)
static volatile struct prz *const prz = (volatile struct prz *)JT_RAMOOPS_CONSOLE;
static volatile struct prz *const prz2 = (volatile struct prz *)JT_RAMOOPS_PMSG;
static uint32_t log_len, log2_len;

static void prz_put(volatile struct prz *z, uint32_t *len, char c)
{
	if (*len >= PRZ_DATA_MAX)
		return;
	z->data[(*len)++] = (uint8_t)c;
	z->start = *len;
	z->size = *len;
}
#endif

static uint32_t fg = 0xffffffffU;

#if JT_HAVE_FB
static volatile uint32_t *const fb = (volatile uint32_t *)JT_FB_BASE;
#define SCALE	3
#define CELL_W	(8 * SCALE)
#define CELL_H	(16 * SCALE + 6)
#define COLS	((JT_FB_WIDTH - 48) / CELL_W)
#define ROWS	((JT_FB_HEIGHT - 160) / CELL_H)
static unsigned int row, col;

static void fill(int x0, int y0, int w, int h, uint32_t c)
{
	for (int y = y0; y < y0 + h && y < (int)JT_FB_HEIGHT; y++)
		for (int x = x0; x < x0 + w && x < (int)JT_FB_WIDTH; x++)
			fb[y * (JT_FB_STRIDE / 4) + x] = c;
}

static void scr_ch(char c)
{
	if (c == '\n' || col >= COLS) {
		col = 0;
		row = (row + 1) % ROWS;
		fill(0, 120 + row * CELL_H, JT_FB_WIDTH, CELL_H, 0xff000000U);
		if (c == '\n')
			return;
	}
	if (c < 0x20 || c > 0x7e)
		c = '?';
	const unsigned char *g = font8x16[c - 0x20];
	int x0 = 24 + col * CELL_W, y0 = 120 + row * CELL_H;
	for (int gy = 0; gy < 16; gy++)
		for (int gx = 0; gx < 8; gx++)
			fill(x0 + gx * SCALE, y0 + gy * SCALE, SCALE, SCALE,
			     (g[gy] & (0x80 >> gx)) ? fg : 0xff000000U);
	col++;
}
#endif

static void log_ch(char c)
{
#if JT_HAVE_RAMOOPS
	prz_put(prz, &log_len, c);
	prz_put(prz2, &log2_len, c);
#endif
#if JT_HAVE_FB
	scr_ch(c);
#else
	(void)c;
#endif
}

static void out(const char *s)
{
	while (*s)
		log_ch(*s++);
}

static void hex(uint64_t v)
{
	static const char digits[] = "0123456789abcdef";
	char b[19] = "0x";
	int n = 2, started = 0;
	for (int i = 60; i >= 0; i -= 4) {
		int d = (v >> i) & 0xf;
		if (d || started || i == 0) {
			b[n++] = digits[d];
			started = 1;
		}
	}
	b[n] = 0;
	out(b);
}

static void kv(const char *k, uint64_t v)
{
	out(k);
	out("=");
	hex(v);
	out(" ");
}

/* MMU off: everything is Device memory, keep accesses naturally aligned. */
static void copy(uintptr_t dst, uintptr_t src, uint64_t n)
{
	if (!((dst | src) & 7)) {
		for (; n >= 8; n -= 8, dst += 8, src += 8)
			*(volatile uint64_t *)dst = *(volatile uint64_t *)src;
	}
	for (; n; n--, dst++, src++)
		*(volatile uint8_t *)dst = *(volatile uint8_t *)src;
}

static void zero(uintptr_t dst, uint64_t n)
{
	for (; n && (dst & 7); n--, dst++)
		*(volatile uint8_t *)dst = 0;
	for (; n >= 8; n -= 8, dst += 8)
		*(volatile uint64_t *)dst = 0;
	for (; n; n--, dst++)
		*(volatile uint8_t *)dst = 0;
}

static void clean(uintptr_t start, uintptr_t end)
{
	uint64_t ctr;
	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
	uintptr_t line = 4UL << ((ctr >> 16) & 0xf);
	for (uintptr_t p = start & ~(line - 1); p < end; p += line)
		__asm__ volatile("dc civac, %0" : : "r"(p) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

static int magic_ok(const volatile char *m)
{
	const char *want = "SWEETXNU";
	for (int i = 0; i < 8; i++)
		if (m[i] != want[i])
			return 0;
	return 1;
}

static uint64_t fail(const char *why)
{
	fg = 0xffff4040U;
	out("FAIL: ");
	out(why);
	out("\n");
	return 0;
}

uint64_t jt_main(uintptr_t dtb, uint64_t el, uint64_t sctlr)
{
#if JT_HAVE_FB && JT_DECON_TRIG
	*(volatile uint32_t *)JT_DECON_TRIG = JT_DECON_TRIG_UNMASK;
#endif
#if JT_HAVE_RAMOOPS
	prz->sig = PRZ_SIG;
	prz->start = prz->size = 0;
	prz2->sig = PRZ_SIG;
	prz2->start = prz2->size = 0;
#endif
#if JT_HAVE_FB && JT_DECON_TRIG == 0
	/* No verified trigger: clear the screen only, never poke DECON. */
	for (int y = 0; y < (int)JT_FB_HEIGHT; y++)
		for (int x = 0; x < (int)JT_FB_WIDTH; x++)
			((volatile uint32_t *)JT_FB_BASE)[y * (JT_FB_STRIDE / 4) + x] = 0xff000000U;
	fg = 0xffffff00U;
#endif
	out("JACKPOTLTE LOADER BEGIN  (ios-jackpotlte, Exynos 7885)\n");
	fg = 0xffffffffU;
	kv("EL", el);
	kv("SCTLR", sctlr);
	kv("DTB", dtb);
	out("\n");
	if (el != 1)
		return fail("not entered at EL1");

	uint64_t rd_start, rd_end;
	if (!fdt_chosen_u64(dtb, "linux,initrd-start", &rd_start) ||
	    !fdt_chosen_u64(dtb, "linux,initrd-end", &rd_end))
		return fail("no initrd in /chosen");
	kv("initrd", rd_start);
	kv("end", rd_end);
	out("\n");
	if (rd_start < LOW_RAM_BASE || rd_end > LOW_RAM_END || rd_end <= rd_start)
		return fail("initrd outside low RAM");

	const volatile struct payload *p = (const volatile struct payload *)rd_start;
	uint64_t length = rd_end - rd_start;
	if (length < sizeof(struct payload) || !magic_ok(p->magic) || p->version != 1)
		return fail("initrd is not a SWEETXNU v1 payload");
	uint64_t nsegs = p->nsegs;
	kv("segs", nsegs);
	kv("virt", p->virt_base);
	kv("entry", p->entry_va);
	out("\n");
	kv("phys", p->phys_base);
	kv("mem", p->mem_size);
	kv("adt", p->adt_size);
	out("\n");
	if (!nsegs || nsegs > 32 ||
	    __builtin_offsetof(struct payload, segs) + nsegs * sizeof(struct payload_seg) > length ||
	    p->adt_off > length || p->adt_size > length - p->adt_off)
		return fail("bad payload header");
	if (p->phys_base < XNU_RAM_BASE || p->phys_base >= XNU_RAM_END || !p->mem_size ||
	    p->mem_size > XNU_RAM_END - p->phys_base ||
	    p->entry_va < p->virt_base || p->kernel_end < p->virt_base)
		return fail("payload RAM outside the XNU bank");
	uintptr_t mem_end = p->phys_base + p->mem_size;
	uintptr_t kernel_end = p->phys_base + (p->kernel_end - p->virt_base);
	if (kernel_end >= mem_end)
		return fail("kernel larger than RAM window");

	/* iBoot hands XNU zeroed RAM; S-Boot leaves the previous OS's data.
	 * (dreamlte boot 3-7: not zeroing made vm_mem_bootstrap hang.) */
	out("zeroing XNU RAM...\n");
	zero(p->phys_base, p->mem_size);
	out("zeroed\n");

	for (uint64_t i = 0; i < nsegs; i++) {
		const volatile struct payload_seg *s = &p->segs[i];
		if (s->vmaddr < p->virt_base || s->file_size > s->vmsize ||
		    s->file_off > length || s->file_size > length - s->file_off ||
		    s->vmsize > p->mem_size || s->vmaddr - p->virt_base > p->mem_size - s->vmsize)
			return fail("bad segment");
		uintptr_t dst = p->phys_base + (s->vmaddr - p->virt_base);
		copy(dst, rd_start + s->file_off, s->file_size);
		zero(dst + s->file_size, s->vmsize - s->file_size);
		clean(dst, dst + s->vmsize);
		out("seg ");
		hex(i);
		out(" -> ");
		hex(dst);
		out(" ");
		hex(s->vmsize);
		out("\n");
	}

	uintptr_t adt_pa = kernel_end;
	uintptr_t ba_pa = (adt_pa + p->adt_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	if (ba_pa + PAGE_SIZE > mem_end)
		return fail("no room for ADT/boot_args");
	copy(adt_pa, rd_start + p->adt_off, p->adt_size);
	zero(ba_pa, PAGE_SIZE);
	volatile struct boot_args *ba = (volatile struct boot_args *)ba_pa;
	ba->Revision = 2;
	ba->Version = 2;
	ba->virtBase = p->virt_base;
	ba->physBase = p->phys_base;
	ba->memSize = p->mem_size;
	ba->topOfKernelData = ba_pa + PAGE_SIZE;
	ba->deviceTreeP = p->virt_base + (adt_pa - p->phys_base);
	ba->deviceTreeLength = (uint32_t)p->adt_size;
	for (int i = 0; i < BOOT_LINE_LENGTH; i++)
		ba->CommandLine[i] = p->cmdline[i];
	ba->memSizeActual = p->mem_size;
	clean(adt_pa, ba_pa + PAGE_SIZE);

	uintptr_t entry_pa = p->phys_base + (p->entry_va - p->virt_base);
	if (entry_pa >= kernel_end)
		return fail("entry outside kernel");
	jt_boot_args = ba_pa;
	kv("adt_pa", adt_pa);
	kv("boot_args", ba_pa);
	out("\n");
	kv("entry_pa", entry_pa);
	out("\ncmdline: ");
	for (int i = 0; i < BOOT_LINE_LENGTH && p->cmdline[i]; i++) {
		char c[2] = { p->cmdline[i], 0 };
		out(c);
	}
	out("\n");
	fg = 0xff40ff40U;
	out("\nJUMP TO XNU\n");
#if JT_HAVE_RAMOOPS
	clean(JT_RAMOOPS_CONSOLE, JT_RAMOOPS_CONSOLE + JT_RAMOOPS_SIZE);
	clean(JT_RAMOOPS_PMSG, JT_RAMOOPS_PMSG + JT_RAMOOPS_SIZE);
#endif
	return entry_pa;
}

void jt_exception(uint64_t slot)
{
	uint64_t esr, elr, far;
	__asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
	__asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
	__asm__ volatile("mrs %0, far_el1" : "=r"(far));
	fg = 0xffff4040U;
	out("\nLOADER EXCEPTION ");
	kv("slot", slot);
	kv("ESR", esr);
	kv("ELR", elr);
	kv("FAR", far);
	out("\n");
	for (;;)
		__asm__ volatile("wfe");
}

/* gcc may emit calls for struct copies; keep the image self-contained. */
void *memset(void *d, int c, unsigned long n)
{
	unsigned char *q = d;
	while (n--)
		*q++ = (unsigned char)c;
	return d;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
	unsigned char *q = d;
	const unsigned char *r = s;
	while (n--)
		*q++ = *r++;
	return d;
}
