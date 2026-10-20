#!/usr/bin/env python3
"""Apply the Galaxy S8 (dreamlte, Exynos 8895) hardware layer to an
A5XELTE-patched xnu tree (tools-s8/prep-xnu-tree.sh output).

The A5XELTE machine config and its guards are reused unchanged. What
differs on the S8 is expressed as A5_* macros in A5XELTE.h, with the A5
values kept for a non-DREAMLTE build. Every replacement asserts its
expected match count. Usage: apply-dreamlte.py XNU_TREE
"""
import sys
from pathlib import Path

tree = Path(sys.argv[1])


def edit(rel, pairs):
    path = tree / rel
    text = path.read_text()
    for old, new, count in pairs:
        found = text.count(old)
        if found != count:
            raise SystemExit(f'{rel}: expected {count}x {old!r}, found {found}')
        text = text.replace(old, new)
    path.write_text(text)
    print(f'edited {rel}')


edit('pexpert/pexpert/arm64/A5XELTE.h', [(
    '#define A5XELTE_VTIMER_INTID         27\n',
    '#define A5XELTE_VTIMER_INTID         27\n'
    '\n'
    '/*\n'
    ' * Samsung Galaxy S8 (SM-G950F, dreamlte, Exynos 8895) build of this port.\n'
    ' * Same Samsung S-Boot handoff, GIC-400, 26 MHz timer and Cortex-A53 boot\n'
    ' * cluster as the A5; the board facts that differ are below.\n'
    ' */\n'
    '#define DREAMLTE                      1\n'
    '#if defined(DREAMLTE)\n'
    '#define A5_FB_PHYS                    0xcc000000  /* s3cfb.bootloaderfb */\n'
    '#define A5_FB_WIDTH                   1440\n'
    '#define A5_FB_HEIGHT                  2960\n'
    '#define A5_FB_STRIDE                  5760        /* bytes per row */\n'
    '#define A5_FB_2M_BLOCKS               9           /* 1440*2960*4 bytes */\n'
    '#define A5_ENTRY_MARKER_PHYS          0x920ff000  /* slot in the loader\'s pstore text */\n'
    '#define A5_BOOT_MPIDR                 0x100       /* first Cortex-A53 */\n'
    '#define A5_KEYS_DEFAULT               0           /* A5 GPA2 keys absent */\n'
    '#define A5_KLOG_PHYS                  0x92180000  /* TWRP ramoops pmsg zone */\n'
    '#define A5_KLOG_SIZE                  0x80000\n'
    '#else\n'
    '#define A5_FB_PHYS                    0x67000000\n'
    '#define A5_FB_WIDTH                   1080\n'
    '#define A5_FB_HEIGHT                  1920\n'
    '#define A5_FB_STRIDE                  4320\n'
    '#define A5_FB_2M_BLOCKS               5\n'
    '#define A5_ENTRY_MARKER_PHYS          0x469ff100\n'
    '#define A5_BOOT_MPIDR                 0\n'
    '#define A5_KEYS_DEFAULT               1\n'
    '#endif\n', 1)])

# Early assembly: entry/MMU/stage blocks are 36x21 px at x=240, rows 88..216.
stage_rows = {'0x6705d0c0': 88, '0x6707ecc0': 120, '0x670a08c0': 152,
              '0x670c24c0': 184, '0x670e40c0': 216}
pairs = [('MOV64\tx10, 0x469ff100', 'MOV64\tx10, A5_ENTRY_MARKER_PHYS', 1)]
for addr, row in stage_rows.items():
    pairs.append((f'MOV64\tx10, {addr}',
                  f'MOV64\tx10, (A5_FB_PHYS + {row} * A5_FB_STRIDE + 240 * 4)', 1))
pairs += [
    ('mov\t\tx15, #4320', 'mov\t\tx15, #A5_FB_STRIDE', 2),
    ('mov\tx15, #4320', 'mov\tx15, #A5_FB_STRIDE', 3),
    ('MOV64\tx0, 0x67000000\n\tmov\t\tx4, x0\n\tmov\t\tx5, #5\n',
     'MOV64\tx0, A5_FB_PHYS\n\tmov\t\tx4, x0\n\tmov\t\tx5, #A5_FB_2M_BLOCKS\n', 1),
    ('MOV64\tx10, 0x67000000', 'MOV64\tx10, A5_FB_PHYS', 1),
    ('mov\t\tx13, #5\nLa5_fb_attr:', 'mov\t\tx13, #A5_FB_2M_BLOCKS\nLa5_fb_attr:', 1),
    ('MOV64\tx4, 0x67000000\n\tmov\tx5, #4320',
     'MOV64\tx4, A5_FB_PHYS\n\tmov\tx5, #A5_FB_STRIDE', 1),
    ('mov\tx13, #8640', 'mov\tx13, #(2 * A5_FB_STRIDE)', 1),
    ('/* Five 2 MiB blocks map the 1080x1920 scanout buffer VA=PA.',
     '/* A5_FB_2M_BLOCKS 2 MiB blocks map the scanout buffer VA=PA.', 1),
]
edit('osfmk/arm64/start.s', pairs)

edit('osfmk/arm64/sweet_early.c', [
    ('#define A5_FB ((volatile uint32_t *)(uintptr_t)0x67000000)\n#define A5_STRIDE 1080u\n',
     '#define A5_FB ((volatile uint32_t *)(uintptr_t)A5_FB_PHYS)\n#define A5_STRIDE (A5_FB_STRIDE / 4u)\n', 1),
    ('#define A5_COLS (1080u / A5_COL_WIDTH)', '#define A5_COLS (A5_FB_WIDTH / A5_COL_WIDTH)', 1),
    ('(1920u / A5_ROW_HEIGHT)', '(A5_FB_HEIGHT / A5_ROW_HEIGHT)', 1),
    ('for (unsigned int x = 0; x < A5_STRIDE; x++) {', 'for (unsigned int x = 0; x < A5_FB_WIDTH; x++) {', 1),
])

edit('iokit/Kernel/arm/SweetFramebuffer.cpp', [
    ('#include <pexpert/pexpert.h>\n',
     '#include <pexpert/pexpert.h>\n#include <pexpert/arm64/board_config.h>\n', 1),
    ('\tfPhys = 0x67000000ULL;\n\tfWidth = 1080;\n\tfHeight = 1920;\n\tfRowBytes = 4320;\n',
     '\tfPhys = A5_FB_PHYS;\n\tfWidth = A5_FB_WIDTH;\n\tfHeight = A5_FB_HEIGHT;\n\tfRowBytes = A5_FB_STRIDE;\n', 1),
])

edit('iokit/Kernel/arm/SweetCPU.cpp', [
    ('\tuint32_t expected_mpidr = cpu->cpu_id < 4 ? cpu->cpu_id :\n\t    0x100U + cpu->cpu_id - 4;\n',
     '#if defined(DREAMLTE)\n'
     '\t/* Exynos 8895: logical 0..3 = A53 MPIDR 0x100..0x103 (boot cluster);\n'
     '\t * logical 4..7 = Mongoose M2 MPIDR 0..3. */\n'
     '\tuint32_t expected_mpidr = cpu->cpu_id < 4 ? 0x100U + cpu->cpu_id :\n'
     '\t    cpu->cpu_id - 4;\n'
     '#else\n'
     '\tuint32_t expected_mpidr = cpu->cpu_id < 4 ? cpu->cpu_id :\n'
     '\t    0x100U + cpu->cpu_id - 4;\n'
     '#endif\n', 1),
    ('\t/* The A5 ADT describes CPU0 as the boot CPU. */\n'
     '\tif (topology->boot_cpu->cpu_id != 0 || topology->boot_cpu->phys_id != 0) {\n'
     '\t\tpanic(CPU_PORT_NAME " CPU: expected boot MPIDR 0 and logical CPU 0");\n',
     '\t/* The ADT describes logical CPU 0 as the boot CPU. */\n'
     '\tif (topology->boot_cpu->cpu_id != 0 || topology->boot_cpu->phys_id != A5_BOOT_MPIDR) {\n'
     '\t\tpanic(CPU_PORT_NAME " CPU: expected boot MPIDR %#x and logical CPU 0", A5_BOOT_MPIDR);\n', 1),
])

edit('pexpert/arm/pe_serial.c', [
    ('static uint32_t fbcon_cols, fbcon_rows, fbcon_col, fbcon_row;\n',
     'static uint32_t fbcon_cols, fbcon_rows, fbcon_col, fbcon_row;\n'
     '\n'
     '#if defined(DREAMLTE)\n'
     '/*\n'
     ' * Copy of the console in TWRP\'s ramoops pmsg zone, in persistent_ram\n'
     ' * format (sig "DBGC", start, size, ring data), appended to the loader\'s\n'
     ' * text there. After a forced reboot TWRP shows loader + kernel log as\n'
     ' * /sys/fs/pstore/pmsg-ramoops-0. (The dump zone survives too but this\n'
     ' * TWRP does not expose it; the console zone header sits on RAM cells that\n'
     ' * lose the same bits on every forced reboot.)\n'
     ' */\n'
     '#define KLOG_CAP        (A5_KLOG_SIZE - 12)\n'
     'static volatile uint32_t *klog_hdr;\n'
     'static volatile uint8_t *klog_data;\n'
     'static uint32_t klog_start, klog_size;\n'
     'static void klog_putc(uint8_t c);\n'
     '\n'
     'static void\n'
     'klog_init(void)\n'
     '{\n'
     '\tklog_hdr = (volatile uint32_t *)ml_io_map(A5_KLOG_PHYS, A5_KLOG_SIZE);\n'
     '\tif (klog_hdr == NULL) {\n'
     '\t\treturn;\n'
     '\t}\n'
     '\tklog_data = (volatile uint8_t *)(klog_hdr + 3);\n'
     '\t/* Continue after the loader\'s text if its header is sane. */\n'
     '\tif (klog_hdr[0] == 0x43474244u && klog_hdr[1] == klog_hdr[2] &&\n'
     '\t    klog_hdr[2] < KLOG_CAP - 0x10000) {\n'
     '\t\tklog_start = klog_size = klog_hdr[2];\n'
     '\t} else {\n'
     '\t\tklog_hdr[0] = 0x43474244u;\n'
     '\t\tklog_start = klog_size = 0;\n'
     '\t}\n'
     '\tconst char *banner = "\\n--- XNU console ---\\n";\n'
     '\tfor (; *banner; banner++) {\n'
     '\t\tklog_putc((uint8_t)*banner);\n'
     '\t}\n'
     '}\n'
     '\n'
     'static void\n'
     'klog_putc(uint8_t c)\n'
     '{\n'
     '\tif (klog_data == NULL) {\n'
     '\t\treturn;\n'
     '\t}\n'
     '\tklog_data[klog_start] = c;\n'
     '\tklog_start = (klog_start + 1) % KLOG_CAP;\n'
     '\tif (klog_size < KLOG_CAP) {\n'
     '\t\tklog_size++;\n'
     '\t}\n'
     '\tklog_hdr[1] = klog_start;\n'
     '\tklog_hdr[2] = klog_size;\n'
     '}\n'
     '#endif\n', 1),
    ('fbcon_transmit_data(uint8_t c)\n{\n',
     'fbcon_transmit_data(uint8_t c)\n{\n'
     '#if defined(DREAMLTE)\n'
     '\tklog_putc(c);\n'
     '#endif\n', 1),
    ('\tfbcon_base = (volatile uint32_t *)ml_io_map_wcomb(reg[0], reg[1]);\n',
     '\tfbcon_base = (volatile uint32_t *)ml_io_map_wcomb(reg[0], reg[1]);\n'
     '#if defined(DREAMLTE)\n'
     '\tklog_init();\n'
     '#endif\n', 1),
])

# IOKit platform expert / CPU personalities: also match the S8 names.
edit('iokit/KernelConfigTables.cpp', [
    ("    \"     'IONameMatch'     = 'samsung,a5xelte';\"\n",
     "    \"     'IONameMatch'     = ('samsung,a5xelte', 'samsung,dreamlte');\"\n", 1),
])

edit('bsd/dev/a5xelte_keys.c', [
    ('#include <pexpert/pexpert.h>\n',
     '#include <pexpert/pexpert.h>\n#include <pexpert/arm64/board_config.h>\n', 1),
    ('\tint enabled = 1;\n\tif (a5_keys_started) {',
     '\tint enabled = A5_KEYS_DEFAULT;\n\tif (a5_keys_started) {', 1),
])
