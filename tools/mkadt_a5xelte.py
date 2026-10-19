#!/usr/bin/env python3
"""Build an entry-only a5xelte ADT from the donor's minimal schema.

This includes the Exynos7580 GICv2 but omits storage until a native driver exists.
It can reach the XNU entry marker; it is not an iOS root/device tree.
"""
import argparse
import os
from pathlib import Path

import adt
import mkadt


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


def build():
    root = mkadt.build(seed=os.urandom(64), qemu=True)
    set_prop(root, 'compatible', mkadt.cstr('samsung,a5xelte') +
             mkadt.cstr('samsung,exynos7580') + mkadt.cstr('AppleARM'))
    chosen = child(root, 'chosen')
    set_prop(chosen, 'dram-base', mkadt.u64(0x80000000))
    set_prop(chosen, 'dram-size', mkadt.u64(0x20000000))
    set_prop(chosen, 'firmware-version', mkadt.cstr('a5xelte-entry'))
    set_prop(chosen, 'system-firmware-version', mkadt.cstr('a5xelte-entry'))
    cpus = child(root, 'cpus')
    cpu0 = child(cpus, 'cpu0')
    set_prop(cpu0, 'compatible', mkadt.cstr('arm,cortex-a53') + mkadt.cstr('ARM,v8'))
    set_prop(cpu0, 'reg', mkadt.u32(0))
    set_prop(cpu0, 'timebase-frequency', mkadt.u32(26000000))
    arm_io = child(root, 'arm-io')
    set_prop(arm_io, 'device_type', mkadt.cstr('a5xelte-io'))
    set_prop(arm_io, 'compatible', mkadt.cstr('samsung,exynos7580-io') +
             mkadt.cstr('arm-io'))
    set_prop(arm_io, 'ranges', mkadt.u64(0, 0x10000000, 0xf0000000))
    arm_io.children = [n for n in arm_io.children if n.prop('name') == mkadt.cstr('wdt')]
    arm_io.children.append(mkadt.node('gic', [
        mkadt.ph(),
        ('compatible', mkadt.cstr('arm,cortex-a15-gic')),
        ('interrupt-controller', b''),
        ('virtual-timer-intid', mkadt.u32(27)),
        ('reg', mkadt.u64(0x0c01000, 0x1000, 0x0c02000, 0x1000)),
    ]))
    root.children = [n for n in root.children if n.prop('name') not in
                     (mkadt.cstr('sweet-fbcon'), mkadt.cstr('sweet-ufs'))]
    return root


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    blob = adt.serialize(build())
    args.output.write_bytes(blob)
    print(f'{args.output}: {len(blob)} bytes')
