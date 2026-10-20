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

# Boots 3, 5, 6 and 7 hung right after the kernel version line, before
# pmap_startup's "init/release time"; boots 1, 2, 3b and 5b got past it with
# the same image. The physical aperture slide (1 GiB range, early_random) is
# the boot-to-boot variable there (gPicBase differs on every photo). Pin it
# on the S8 and mark vm_page_bootstrap's steps in the console.
edit('osfmk/arm64/arm_vm_init.c', [
    ('\tuint64_t physmap_slide = early_random() & ARM64_PHYSMAP_SLIDE_MASK & ~((uint64_t)PAGE_MASK);\n',
     '#if defined(DREAMLTE)\n'
     '\tuint64_t physmap_slide = 0;\n'
     '#else\n'
     '\tuint64_t physmap_slide = early_random() & ARM64_PHYSMAP_SLIDE_MASK & ~((uint64_t)PAGE_MASK);\n'
     '#endif\n', 1),
])

edit('osfmk/vm/vm_resident.c', [
    ('\tpmap_startup(&virtual_space_start, &virtual_space_end);\n',
     '#if defined(ARM64_BOARD_CONFIG_A5XELTE)\n'
     '\tkprintf("A5 vm: vm_page_bootstrap -> pmap_startup\\n");\n'
     '#endif\n'
     '\tpmap_startup(&virtual_space_start, &virtual_space_end);\n', 1),
    ('\tvm_pages = (vm_page_t) pmap_steal_freeable_memory(npages * sizeof *vm_pages);\n',
     '\tvm_pages = (vm_page_t) pmap_steal_freeable_memory(npages * sizeof *vm_pages);\n'
     '#if defined(ARM64_BOARD_CONFIG_A5XELTE)\n'
     '\tkprintf("A5 vm: npages=%u vm_pages=%p mem_sz=%#llx\\n", npages, vm_pages, (unsigned long long)mem_sz);\n'
     '#endif\n', 1),
    ('\t\tif (!pmap_next_page(&phys_page)) {\n\t\t\tbreak;\n\t\t}\n\n\t\tif (phys_page < max_valid_low_ppnum) {\n',
     '\t\tif (!pmap_next_page(&phys_page)) {\n\t\t\tbreak;\n\t\t}\n'
     '#if defined(ARM64_BOARD_CONFIG_A5XELTE)\n'
     '\t\tif ((i & 0x7fff) == 0) {\n'
     '\t\t\tkprintf("A5 vm: page %u ppn %#x\\n", i, phys_page);\n'
     '\t\t}\n'
     '#endif\n'
     '\n\t\tif (phys_page < max_valid_low_ppnum) {\n', 1),
    ('\tvm_page_pages = vm_pages_count; /* used to report to user space */\n',
     '\tvm_page_pages = vm_pages_count; /* used to report to user space */\n'
     '#if defined(ARM64_BOARD_CONFIG_A5XELTE)\n'
     '\tkprintf("A5 vm: %u pages initialized, releasing\\n", vm_pages_count);\n'
     '#endif\n', 1),
])

# ---- Galaxy S8 internal UFS: the SWEET driver with the Exynos 8895 quirks ----
# Proven by the loader probe (shim/dreamlte/ufs_probe.c, S8 boot 13): HCE
# reads 0 while running, UTRD sizes in bytes, 16-byte PRDT entries of at
# most 4 KiB, NEXUS_TYPE bit per SCSI slot, and the device stays NOT READY
# (04/02) until START STOP UNIT (power condition ACTIVE) to W-LUN 0xD0.
edit('bsd/dev/sweet_ufs.c', [
    ('#if defined(ARM64_BOARD_CONFIG_SWEET)\n#include <sys/param.h>\n',
     '#if defined(ARM64_BOARD_CONFIG_SWEET) || defined(ARM64_BOARD_CONFIG_A5XELTE)\n'
     '#include <sys/param.h>\n', 1),
    ('#define CAP_64AS        (1u << 24)              /* 64-bit DMA addressing */\n',
     '#define CAP_64AS        (1u << 24)              /* 64-bit DMA addressing */\n'
     '\n'
     '#if defined(ARM64_BOARD_CONFIG_A5XELTE)\n'
     '/*\n'
     ' * Galaxy S8 (Exynos 8895) host, per the Samsung 4.4 driver and the S8\n'
     ' * loader probe (shim/dreamlte/ufs_probe.c): UTRD offsets and lengths are\n'
     ' * in bytes (UFSHCD_QUIRK_BROKEN_DWORD_UTRD), PRDT entries are 16 bytes and\n'
     ' * cover at most 4 KiB (HCI_TXPRDT_ENTRY_SIZE 12), each SCSI slot needs its\n'
     ' * HCI_UTRL_NEXUS_TYPE bit, the standard HCE register reads 0 while the host\n'
     ' * runs (UFSHCD_QUIRK_USE_OF_HCE) and UTRLCLR is inverted\n'
     ' * (UFSHCD_QUIRK_BROKEN_REQ_LIST_CLR). S-Boot leaves the device stopped\n'
     ' * (sense 02/04/02): START STOP UNIT, power condition ACTIVE, to the UFS\n'
     ' * Device well-known LUN wakes it.\n'
     ' */\n'
     '#define EXYNOS_UFS              1\n'
     '#define VS_UTRL_NEXUS_TYPE      0x40\n'
     '#define UFS_WLUN_DEVICE         0xd0\n'
     '#define UFS_ADT_NODE            "/dreamlte-ufs"\n'
     '#define UFS_DEFAULT_PART        "USERDATA"\n'
     '#define PRDT_BYTES(field)       (field)\n'
     '#else\n'
     '#define UFS_ADT_NODE            "/sweet-ufs"\n'
     '#define UFS_DEFAULT_PART        "userdata"\n'
     '#define PRDT_BYTES(field)       ((field) * 16u)\n'
     '#endif\n', 1),
    ('#define PRDT_CHUNK      (256u << 10)            /* PRDT byte count field: 18 bits */\n',
     '#if defined(EXYNOS_UFS)\n'
     '#define PRDT_CHUNK      4096u                   /* HCI_TXPRDT_ENTRY_SIZE 12 */\n'
     '#else\n'
     '#define PRDT_CHUNK      (256u << 10)            /* PRDT byte count field: 18 bits */\n'
     '#endif\n', 1),
    ('\tvm_offset_t regs;\n',
     '\tvm_offset_t regs;\n\tvm_offset_t vs_regs;                    /* Exynos vendor HCI registers */\n', 1),
    ('\tutrd->resp = (512u / 4) << 16 | (512u / 4);\n',
     '#if defined(EXYNOS_UFS)\n'
     '\tutrd->resp = 512u << 16 | 512u;\n'
     '#else\n'
     '\tutrd->resp = (512u / 4) << 16 | (512u / 4);\n'
     '#endif\n', 1),
    ('\t\tutrd->prdt = (1024u / 4) << 16 | n;\n',
     '#if defined(EXYNOS_UFS)\n'
     '\t\tutrd->prdt = 1024u << 16 | (n * (uint32_t)sizeof(ucd->prdt[0]));\n'
     '#else\n'
     '\t\tutrd->prdt = (1024u / 4) << 16 | n;\n'
     '#endif\n', 1),
    ('\t\t\t    (utrd->prdt & 0xffff) * sizeof(ucd->prdt[0]));\n',
     '\t\t\t    PRDT_BYTES(utrd->prdt & 0xffff));\n', 1),
    ('\tufs_wr(REG_UTRLDBR, 1);\n',
     '#if defined(EXYNOS_UFS)\n'
     '\t*(volatile uint32_t *)(su.vs_regs + VS_UTRL_NEXUS_TYPE) |= 1;  /* slot 0: SCSI */\n'
     '#endif\n'
     '\tufs_wr(REG_UTRLDBR, 1);\n', 1),
    ('\t\t\tufs_wr(REG_UTRLCLR, ~1u);\n',
     '#if !defined(EXYNOS_UFS)\n'
     '\t\t\tufs_wr(REG_UTRLCLR, ~1u);\n'
     '#endif\n', 1),
    ('\tif (!(hce & 1) || !(hcs & HCS_DP) || !(hcs & HCS_UTRLRDY)) {\n',
     '#if defined(EXYNOS_UFS)\n'
     '\t(void)hce;\n'
     '\tif (!(hcs & HCS_DP) || !(hcs & HCS_UTRLRDY)) {\n'
     '#else\n'
     '\tif (!(hce & 1) || !(hcs & HCS_DP) || !(hcs & HCS_UTRLRDY)) {\n'
     '#endif\n', 1),
    ('\tchar want[64] = "userdata";\n', '\tchar want[64] = UFS_DEFAULT_PART;\n', 1),
    ('\tif (SecureDTLookupEntry(NULL, "/sweet-ufs", &node) != kSuccess ||\n'
     '\t    SecureDTGetProperty(node, "reg", (void const **)&reg, &size) != kSuccess ||\n'
     '\t    size < 2 * sizeof(uintptr_t)) {\n'
     '\t\tprintf("SWEET UFS: no /sweet-ufs node in the device tree\\n");\n',
     '\tif (SecureDTLookupEntry(NULL, UFS_ADT_NODE, &node) != kSuccess ||\n'
     '\t    SecureDTGetProperty(node, "reg", (void const **)&reg, &size) != kSuccess ||\n'
     '#if defined(EXYNOS_UFS)\n'
     '\t    size < 4 * sizeof(uintptr_t)) {\n'
     '#else\n'
     '\t    size < 2 * sizeof(uintptr_t)) {\n'
     '#endif\n'
     '\t\tprintf("SWEET UFS: no " UFS_ADT_NODE " node in the device tree\\n");\n', 1),
    ('\tsu.regs = ml_io_map(reg[0], reg[1]);\n',
     '\tsu.regs = ml_io_map(reg[0], reg[1]);\n'
     '#if defined(EXYNOS_UFS)\n'
     '\tsu.vs_regs = ml_io_map(reg[2], reg[3]);\n'
     '#endif\n', 1),
    ('\terror = ufs_take_over_host();\n',
     '\terror = ufs_take_over_host();\n'
     '#if defined(EXYNOS_UFS)\n'
     '\tif (!error) {\n'
     '\t\tstatic const uint8_t ssu_active[6] = { 0x1b, 0, 0, 0, 0x10, 0 };\n'
     '\t\tsu.lun = UFS_WLUN_DEVICE;\n'
     '\t\terror = ufs_scsi(ssu_active, sizeof(ssu_active), 0, false);\n'
     '\t\tsu.lun = 0;\n'
     '\t\tprintf("SWEET UFS: START STOP UNIT (ACTIVE) to the device W-LUN: %d\\n", error);\n'
     '\t}\n'
     '#endif\n', 1),
    ('\tsweet_sample_start();\n\t/* Input drivers (HID services in iokit/Kernel/arm/hid/SweetHID.cpp). */\n'
     '\tsweet_keys_init();\n\tsweet_touch_init();\n\tsweet_usb_probe();\n',
     '#if defined(ARM64_BOARD_CONFIG_SWEET)\n'
     '\tsweet_sample_start();\n\t/* Input drivers (HID services in iokit/Kernel/arm/hid/SweetHID.cpp). */\n'
     '\tsweet_keys_init();\n\tsweet_touch_init();\n\tsweet_usb_probe();\n'
     '#endif\n', 1),
])

# rd=ufsroot0 also on the A5XELTE machine config (the S8 build).
edit('iokit/bsddev/IOKitBSDInit.cpp', [
    ('#if defined(ARM64_BOARD_CONFIG_SWEET)\n//\n//\tSWEET: root on a GPT partition of the internal UFS (bsd/dev/sweet_ufs.c)\n',
     '#if defined(ARM64_BOARD_CONFIG_SWEET) || defined(ARM64_BOARD_CONFIG_A5XELTE)\n//\n'
     '//\tSWEET / Galaxy S8: root on a GPT partition of the internal UFS (bsd/dev/sweet_ufs.c)\n', 1),
])

# Console copy into sweet_ufs.c's ring (written to the end of the root
# partition every ufslogsecs when ufsrw=1): longer than pstore, no bit flips.
edit('osfmk/console/serial_console.c', [
    ('#if defined(ARM64_BOARD_CONFIG_SWEET)\nextern void sweet_log_append',
     '#if defined(ARM64_BOARD_CONFIG_SWEET) || defined(ARM64_BOARD_CONFIG_A5XELTE)\nextern void sweet_log_append', 1),
    ('#if defined(ARM64_BOARD_CONFIG_SWEET)\n\tsweet_log_append(c, size);',
     '#if defined(ARM64_BOARD_CONFIG_SWEET) || defined(ARM64_BOARD_CONFIG_A5XELTE)\n\tsweet_log_append(c, size);', 1),
])

# IOKit platform expert / CPU personalities: also match the S8 names.
edit('iokit/KernelConfigTables.cpp', [
    ("    \"     'IONameMatch'     = 'samsung,a5xelte';\"\n",
     "    \"     'IONameMatch'     = ('samsung,a5xelte', 'samsung,dreamlte');\"\n", 1),
])

# A5 touch probe maps Exynos7580 I2C/GPIO blocks: a bus error on the S8.
edit('bsd/dev/a5xelte_touch.c', [
    ('#include <pexpert/pexpert.h>\n',
     '#include <pexpert/pexpert.h>\n#include <pexpert/arm64/board_config.h>\n', 1),
    ('a5xelte_touch_probe(void)\n{\n\tint enabled = 1;\n',
     'a5xelte_touch_probe(void)\n{\n\tint enabled = A5_KEYS_DEFAULT; /* A5 board drivers: off on the S8 */\n', 1),
])

edit('bsd/dev/a5xelte_keys.c', [
    ('#include <pexpert/pexpert.h>\n',
     '#include <pexpert/pexpert.h>\n#include <pexpert/arm64/board_config.h>\n', 1),
    ('\tint enabled = 1;\n\tif (a5_keys_started) {',
     '\tint enabled = A5_KEYS_DEFAULT;\n\tif (a5_keys_started) {', 1),
])
