#pragma once
#include <stdint.h>

/* Read a u32/u64 (cell-size agnostic) property of /chosen. Returns 0 if absent. */
int fdt_chosen_u64(uintptr_t fdt, const char *prop, uint64_t *out);

/*
 * Generic FDT reads for the bring-up console. Each walks the blob and matches
 * the first node whose name equals `node` (a top-level name like "chosen" or
 * "memory"; no path syntax). Integer props accept 4 or 8 bytes; string props
 * copy up to `max` bytes (always NUL-terminated) and return the length.
 */
int fdt_node_u64(uintptr_t fdt, const char *node, const char *prop, uint64_t *out);
int fdt_node_str(uintptr_t fdt, const char *node, const char *prop, char *buf, int max);

/*
 * Find the first /memory node's base and size (cell-agnostic: 1 or 2 cells
 * each, per the blob's #address-cells / #size-cells).
 */
int fdt_memory(uintptr_t fdt, uint64_t *base, uint64_t *size);

/* True if the blob has a node whose name is `name` (any level). */
int fdt_has_node(uintptr_t fdt, const char *name);
