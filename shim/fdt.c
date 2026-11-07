/*
 * Just enough flattened-device-tree parsing for the Samsung EL1/MMU-off
 * loaders: find /chosen (initrd location) and, for the jackpotlte bring-up
 * console, dump generic node/property values. Big-endian, 4-byte aligned.
 */
#include "fdt.h"

#define FDT_MAGIC	0xd00dfeed
#define FDT_BEGIN_NODE	1
#define FDT_END_NODE	2
#define FDT_PROP	3
#define FDT_NOP		4
#define FDT_END		9

static uint32_t be32(uintptr_t a)
{
	return __builtin_bswap32(*(volatile uint32_t *)a);
}

static int streq(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *a == *b;
}

static uintptr_t align4(uintptr_t a)
{
	return (a + 3) & ~3UL;
}

static uintptr_t skip_str(uintptr_t p)
{
	/* byte loads are fine on Device memory as long as they are aligned to 1 */
	while (*(volatile char *)p)
		p++;
	return p + 1;
}

/* Node names carry a unit suffix ("memory@80000000"); compare the base only. */
static int name_is(const char *full, const char *want)
{
	int i = 0;
	while (full[i] && full[i] != '@') {
		if (full[i] != want[i])
			return 0;
		i++;
	}
	return want[i] == 0;
}

int fdt_chosen_u64(uintptr_t fdt, const char *prop, uint64_t *out)
{
	if (!fdt || be32(fdt) != FDT_MAGIC)
		return 0;
	uintptr_t p = fdt + be32(fdt + 8);		/* off_dt_struct */
	uintptr_t strings = fdt + be32(fdt + 12);	/* off_dt_strings */
	int depth = 0, in_chosen = 0;

	for (;;) {
		uint32_t tok = be32(p);
		p += 4;
		switch (tok) {
		case FDT_BEGIN_NODE: {
			const char *name = (const char *)p;
			depth++;
			in_chosen = depth == 2 && streq(name, "chosen");
			p = align4(skip_str(p));
			break;
		}
		case FDT_END_NODE:
			if (in_chosen)
				return 0;
			depth--;
			break;
		case FDT_PROP: {
			uint32_t len = be32(p), nameoff = be32(p + 4);
			uintptr_t val = p + 8;
			if (in_chosen && streq((const char *)(strings + nameoff), prop)) {
				if (len == 4)
					*out = be32(val);
				else if (len == 8)
					*out = (uint64_t)be32(val) << 32 | be32(val + 4);
				else
					return 0;
				return 1;
			}
			p = align4(val + len);
			break;
		}
		case FDT_NOP:
			break;
		default:	/* FDT_END or garbage */
			return 0;
		}
	}
}

/*
 * Walk the tree tracking depth. `match_depth` is the depth at which a node
 * named `want` was entered, or -1. Properties are read only at that depth, so
 * a nested node of the same name cannot shadow it.
 */
int fdt_node_u64(uintptr_t fdt, const char *node, const char *prop, uint64_t *out)
{
	if (!fdt || be32(fdt) != FDT_MAGIC)
		return 0;
	uintptr_t p = fdt + be32(fdt + 8);
	uintptr_t strings = fdt + be32(fdt + 12);
	int depth = 0, match_depth = -1;

	for (;;) {
		uint32_t tok = be32(p);
		p += 4;
		switch (tok) {
		case FDT_BEGIN_NODE:
			depth++;
			if (match_depth < 0 && name_is((const char *)p, node))
				match_depth = depth;
			p = align4(skip_str(p));
			break;
		case FDT_END_NODE:
			if (depth == match_depth)
				return 0;
			depth--;
			break;
		case FDT_PROP: {
			uint32_t len = be32(p), nameoff = be32(p + 4);
			uintptr_t val = p + 8;
			if (depth == match_depth && streq((const char *)(strings + nameoff), prop)) {
				if (len == 4)
					*out = be32(val);
				else if (len == 8)
					*out = (uint64_t)be32(val) << 32 | be32(val + 4);
				else
					return 0;
				return 1;
			}
			p = align4(val + len);
			break;
		}
		case FDT_NOP:
			break;
		default:
			return 0;
		}
	}
}

int fdt_node_str(uintptr_t fdt, const char *node, const char *prop, char *buf, int max)
{
	if (!fdt || be32(fdt) != FDT_MAGIC || max <= 0)
		return 0;
	uintptr_t p = fdt + be32(fdt + 8);
	uintptr_t strings = fdt + be32(fdt + 12);
	int depth = 0, match_depth = -1;

	for (;;) {
		uint32_t tok = be32(p);
		p += 4;
		switch (tok) {
		case FDT_BEGIN_NODE:
			depth++;
			if (match_depth < 0 && name_is((const char *)p, node))
				match_depth = depth;
			p = align4(skip_str(p));
			break;
		case FDT_END_NODE:
			if (depth == match_depth)
				return 0;
			depth--;
			break;
		case FDT_PROP: {
			uint32_t len = be32(p), nameoff = be32(p + 4);
			uintptr_t val = p + 8;
			if (depth == match_depth && streq((const char *)(strings + nameoff), prop)) {
				int n = (int)len < max - 1 ? (int)len : max - 1;
				for (int k = 0; k < n; k++)
					buf[k] = *(volatile char *)(val + k);
				buf[n] = 0;
				return n;
			}
			p = align4(val + len);
			break;
		}
		case FDT_NOP:
			break;
		default:
			return 0;
		}
	}
}

int fdt_has_node(uintptr_t fdt, const char *name)
{
	if (!fdt || be32(fdt) != FDT_MAGIC)
		return 0;
	uintptr_t p = fdt + be32(fdt + 8);

	for (;;) {
		uint32_t tok = be32(p);
		p += 4;
		switch (tok) {
		case FDT_BEGIN_NODE:
			if (name_is((const char *)p, name))
				return 1;
			p = align4(skip_str(p));
			break;
		case FDT_END_NODE:
			break;
		case FDT_PROP: {
			uint32_t len = be32(p);
			p = align4(p + 8 + len);
			break;
		}
		case FDT_NOP:
			break;
		default:
			return 0;
		}
	}
}

int fdt_memory(uintptr_t fdt, uint64_t *base, uint64_t *size)
{
	if (!fdt || be32(fdt) != FDT_MAGIC)
		return 0;
	uintptr_t p = fdt + be32(fdt + 8);
	uintptr_t strings = fdt + be32(fdt + 12);
	/* Root #address-cells/#size-cells default to 2 and 1 per spec. */
	int ac = 2, sc = 1, depth = 0, match = -1;
	uint32_t len = 0, nameoff = 0;
	uintptr_t val = 0;

	for (;;) {
		uint32_t tok = be32(p);
		p += 4;
		switch (tok) {
		case FDT_BEGIN_NODE:
			depth++;
			if (depth == 1) {
				ac = 2;
				sc = 1;
			}
			if (match < 0 && name_is((const char *)p, "memory"))
				match = depth;
			p = align4(skip_str(p));
			break;
		case FDT_END_NODE:
			if (depth == match)
				return 0;
			depth--;
			break;
		case FDT_PROP: {
			len = be32(p);
			nameoff = be32(p + 4);
			val = p + 8;
			const char *pn = (const char *)(strings + nameoff);
			if (depth == 1 && streq(pn, "#address-cells") && len == 4)
				ac = (int)be32(val);
			if (depth == 1 && streq(pn, "#size-cells") && len == 4)
				sc = (int)be32(val);
			if (depth == match && streq(pn, "reg")) {
				/* reg = <base(ac cells) size(sc cells)> */
				uint64_t b = 0, s = 0;
				int cells = 0;
				int need = ac + sc;
				int cell = 0;
				for (cell = 0; cell < need && (uint32_t)(cell * 4) < len; cell++) {
					uint64_t v = be32(val + cell * 4);
					cells++;
					if (cell < ac)
						b = (b << 32) | v;
					else
						s = (s << 32) | v;
				}
				if (cells == need && s) {
					*base = b;
					*size = s;
					return 1;
				}
			}
			p = align4(val + len);
			break;
		}
		case FDT_NOP:
			break;
		default:
			return 0;
		}
	}
}
