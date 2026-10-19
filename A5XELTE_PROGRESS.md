# A5XELTE port progress

This file tracks work on the Samsung SM-A510F (a5xelte, Exynos7580). `NOTES.md` is the original SWEET donor's history. A local build, an observation from the phone, and a working iOS feature are different milestones.

## 2026-10-19 — first XNU entry on the phone

- **Device observation:** The Cortex-A53 reported `ID_AA64MMFR0_EL1=0x1122` from EL1. Its 16 KiB translation granule is unsupported. A separate 4 KiB MMU probe completed on the phone and left `A5XNU001` in the recovery log.
- **Local build:** Xcode 27.1 built a 4 KiB arm64 XNU kernel with the initial entry patch. That first binary still used `MACHINE_CONFIG=SWEET`; it is an entry experiment, not the final a5xelte board target. Kernel SHA-256: `f093a6bc0d8f9c060f6dc4429810f768e00ec3f093ca624f1cd2694bd9a9b444`.
- **Device observation:** The 19,990,544-byte Samsung BOOT candidate (SHA-256 `8dff262ad0934b71396900eefef2ce34b681f0574e5e37ba065fc7482cff5921`) was written to BOOT p10 in TWRP and read back with the same hash. After reboot the user saw the Samsung logo. The subsequent TWRP `/proc/last_kmsg` contained loader marker `A5XNU002` and XNU-owned entry marker `XNUELTE1` at the expected `+0x100` offset. This proves that execution entered XNU's `start_first_cpu`; it does not prove MMU setup, a kernel console, iOS userland, or a usable screen.
- **Partition scope:** The test wrote BOOT p10 only. Recovery, EFS and USERDATA were not written. The phone returned to TWRP. The working Linux BOOT checkpoint remains available separately; it was not restored at the user's request.

## Current A5XELTE build work

- Patches `0085`–`0088` introduce `MACHINE_CONFIG=A5XELTE`, a dedicated board header and ADT identity, GICv2 interrupt handling, virtual timer INTID 27, and guards around SWEET-only framebuffer and sampler code. The port temporarily shares donor `SWEET` compile-time facilities. This compatibility definition must not be taken as hardware equivalence.
- The first A5XELTE link exposed donor-only symbols from the framebuffer and sampler; patch `0088` excludes those paths. The next build **linked** `kernel.development.a5xelte.unstripped` (19 MB, SHA-256 `609163d2bded8bd3a4364ad067dd1a068fb9e4ecf833318bcf0b87fc696013c2`). The full install target then failed in `dsymutil` because the local shell could not open its `/dev/fd/62` process substitution (`Operation not permitted`). This is a packaging/debug-symbol failure after linking, not a successful end-to-end build.
- No A5XELTE-named kernel has yet been observed running on the phone. Exynos eMMC, display, touch, buttons, SMP and iOS root remain unimplemented or unverified in XNU.

## Next verified milestone

Inspect the linked A5XELTE Mach-O and finish or bypass the debug-symbol packaging step. Package it with the a5xelte ADT and Samsung loader, verify the BOOT header and image size offline, then make one device boot attempt and recover its marker or panic log from TWRP. Record the exact image hash and device result here. See `docs/a5xelte-bringup.md` for the hardware map and detailed evidence.
