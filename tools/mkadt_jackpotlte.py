#!/usr/bin/env python3
"""Build the Galaxy A8 2018 (SM-A530F, jackpotlte, Exynos 7885) ADT.

Stage 1: an entry-only tree. Every value is sourced from the mainline 7885
device tree (MAINLINE) and is NOT yet confirmed against a live readback from
the phone. Anything not sourced is left out, not guessed.

Sources: arch/arm64/boot/dts/exynos/exynos7885.dtsi and
exynos7885-jackpotlte.dts (mainline). See shim/jackpotlte/board.h.

The identity stays the donor iPhone X (D22AP), as on dreamlte/A5XELTE: the A8
has no Home key, so the no-home-button UI matches.
"""
import argparse
import os
from pathlib import Path

import adt
import mkadt

# MAINLINE exynos7885-jackpotlte.dts memory@80000000, high bank.
DRAM_BASE = 0x880000000
# 512 MiB, the size that boots on dreamlte/A5XELTE in the 39-bit 4K layout.
# TODO(PHONE): grow once the kernel VA layout is proven on jackpotlte.
DRAM_SIZE = 0x20000000
TIMEBASE = 26000000                      # Eureka/mainline oscclk 26 MHz
GICD, GICC = 0x12301000, 0x12302000      # Eureka gic@123001000
FB_PHYS = 0xec000000                     # Eureka fb_handover (exynos,dpu_fb_handover)
FB_W, FB_H = 1080, 2220                  # Eureka panel s6e3fa7 (1080x2220)
SOC_BASE = 0x01000000                    # /arm-io ranges base (unchanged from donor)


def set_prop(node, name, value):
    for i, (key, flags, _) in enumerate(node.props):
        if key == name:
            node.props[i] = (key, flags, value)
            return
    node.props.append((name, 0, value))


def child(node, name):
    for candidate in node.children:
        if candidate.prop('name') == mkadt.cstr(name):
            return candidate
    raise ValueError(f'missing donor ADT node {name}')


# MAINLINE exynos7885.dtsi cpus: 6x A53 (0x100-0x103, 0x200-0x201) then
# 2x A73 (0x0-0x1). Order here is the order XNU enumerates; MPIOR->physical.
CPUS = [
    (0, 0x100, 'a53'), (1, 0x101, 'a53'), (2, 0x102, 'a53'), (3, 0x103, 'a53'),
    (4, 0x200, 'a53'), (5, 0x201, 'a53'),
    (6, 0x000, 'a73'), (7, 0x001, 'a73'),
]


def cpu_node(logical, mpidr, kind):
    a53 = kind == 'a53'
    cluster = 0 if mpidr < 0x200 else 1      # A53 clusters 0/1
    if not a53:
        cluster = 2
    return [
        ('device_type', mkadt.cstr('cpu')),
        ('compatible', mkadt.cstr('arm,cortex-a53' if a53 else 'arm,cortex-a73') +
         mkadt.cstr('ARM,v8')),
        ('reg', mkadt.u32(mpidr)),
        ('cpu-id', mkadt.u32(logical)),
        ('cluster-id', mkadt.u32(cluster)),
        ('cluster-type', mkadt.u32(0)),
        ('die-cluster-id', mkadt.u32(cluster)),
        ('cluster-core-id', mkadt.u32(mpidr & 0xff)),
        ('l2-cache-id', mkadt.u32(cluster)),
        ('l3-cache-id', mkadt.u32(0)),
        ('state', mkadt.cstr('running' if logical == 0 else 'stopped')),
        ('timebase-frequency', mkadt.u32(TIMEBASE)),
        # TODO(SAM-K): Exynos7885 A53/A73 clock rates; placeholder only.
        ('clock-frequency', mkadt.u32(1600000000)),
        ('bus-frequency', mkadt.u32(100000000)),
        ('memory-frequency', mkadt.u32(TIMEBASE)),
        ('peripheral-frequency', mkadt.u32(TIMEBASE)),
        ('fixed-frequency', mkadt.u32(TIMEBASE)),
    ]


def build():
    root = mkadt.build(seed=os.urandom(64), qemu=True)
    # The kernel's SweetPlatformExpert personality matches a Samsung name
    # (KernelConfigTables.cpp); jackpotlte is added there by the device patch.
    set_prop(root, 'compatible', mkadt.cstr('samsung,jackpotlte') +
             mkadt.cstr('samsung,exynos7885') + mkadt.cstr('AppleARM'))
    chosen = child(root, 'chosen')
    set_prop(chosen, 'dram-base', mkadt.u64(DRAM_BASE))
    set_prop(chosen, 'dram-size', mkadt.u64(DRAM_SIZE))
    set_prop(chosen, 'firmware-version', mkadt.cstr('jackpotlte-entry'))
    set_prop(chosen, 'system-firmware-version', mkadt.cstr('jackpotlte-entry'))
    cpus = child(root, 'cpus')
    cpu0 = child(cpus, 'cpu0')
    for key, value in cpu_node(*CPUS[0]):
        if key != 'cpu-id':
            set_prop(cpu0, key, value)
    for logical, mpidr, kind in CPUS[1:]:
        cpus.children.append(mkadt.node(f'cpu{logical}', cpu_node(logical, mpidr, kind)))
    arm_io = child(root, 'arm-io')
    set_prop(arm_io, 'device_type', mkadt.cstr('jackpotlte-io'))
    set_prop(arm_io, 'compatible', mkadt.cstr('samsung,exynos7885-io') +
             mkadt.cstr('arm-io'))
    set_prop(arm_io, 'ranges', mkadt.u64(0, SOC_BASE, 0x10000000 - SOC_BASE))
    arm_io.children = [n for n in arm_io.children if n.prop('name') == mkadt.cstr('wdt')]
    arm_io.children.append(mkadt.node('gic', [
        ('compatible', mkadt.cstr('arm,cortex-a15-gic')),
        ('interrupt-controller', mkadt.cstr('master')),
        # MAINLINE timer: non-secure physical timer PPI 14 = INTID 30. The
        # kernel uses CNTP when A5_USE_PHYS_TIMER is set for the board.
        ('virtual-timer-intid', mkadt.u32(30)),
        ('reg', mkadt.u64(GICD - SOC_BASE, 0x1000, GICC - SOC_BASE, 0x1000)),
    ]))
    # Display service. The A5XELTE build's AppleCLCD personality matches
    # 'samsung,a5xelte-display' (xnu/patches/0109 + 0124 in
    # KernelConfigTables.cpp), so the A8 publishes that same compatible and
    # AppleCLCD binds to the S-Boot handover framebuffer at 0xec000000. The
    # panel is the s6e3fa7 at 1080x2220 (Eureka exynos7885-display-lcd.dtsi:
    # 173-193); IOMobileFramebuffer reads canvas-width/height from this node.
    arm_io.children.append(mkadt.node('disp0', [
        ('device_type', mkadt.cstr('display-subsystem')),
        ('compatible', mkadt.cstr('samsung,a5xelte-display')),
        ('canvas-width', mkadt.u32(FB_W)),
        ('canvas-height', mkadt.u32(FB_H)),
    ]))
    # The framebuffer is confirmed live: after the DECON trigger the loader's
    # banner appears at 0xec000000 (phone screen, 2026-11-08). The A5XELTE
    # serial driver looks for "a5xelte-fbcon" with reg/width/height/stride.
    root.children = [n for n in root.children if n.prop('name') !=
                     mkadt.cstr('sweet-ufs')]
    root.children.append(mkadt.node('a5xelte-fbcon', [
        ('reg', mkadt.u64(FB_PHYS, 0x01400000)),
        ('width', mkadt.u32(FB_W)),
        ('height', mkadt.u32(FB_H)),
        ('stride', mkadt.u32(FB_W * 4)),
    ]))
    # eMMC: Samsung exynos7885.dtsi dwmmc0@13500000 (the embedded device,
    # clock MMC_EMBD; dwmmc1 0x13510000 is SDIO, dwmmc2 0x13550000 is the SD
    # card). bsd/dev/a5xelte_emmc.c maps this window and reads the GPT on
    # emmcprobe=1 / rd=emmcroot0. Absolute physical address, as there.
    root.children = [n for n in root.children if n.prop('name') !=
                     mkadt.cstr('a5xelte-emmc')]
    root.children.append(mkadt.node('a5xelte-emmc', [
        ('reg', mkadt.u64(0x13500000, 0x2000)),
    ]))
    return root


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    blob = adt.serialize(build())
    args.output.write_bytes(blob)
    print(f'{args.output}: {len(blob)} bytes')
