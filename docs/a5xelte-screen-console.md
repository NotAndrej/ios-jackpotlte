# A5XELTE early screen console

The visible Samsung logo is not proof that CPU writes to the bootloader's framebuffer address reach the panel. On the SM-A510F boot command line, `s3cfb.bootloaderfb=0x67000000`. TWRP reports a 1080x1920, 32-bit display with 4320-byte stride, but that describes TWRP's framebuffer after its display driver has started.

On 2026-10-19 a temporary Samsung loader wrote white text into physical RAM at `0x67000000`, cleaned those cache lines, and a temporary XNU entry routine wrote a green rectangle at the corresponding screen offset. The user saw only the Samsung logo. TWRP `/proc/last_kmsg` (SHA-256 `3be3f1cae0b2204c01a8f57642da748eb647f22f7e444d23c2493ac554a4059d`) contains loader marker `A5XNU002` at `0x19b1e1` and XNU-owned `XNUELTE1` at `0x19b2e1`. The CPU ran both stages; the direct framebuffer writes were not visibly presented. That temporary code was removed from the ongoing loader and patch series.

## What the working Linux port does

The verified native driver is `mainline/drivers/exynos7580-drm-handoff.c` in the Linux port. It uses DECON at `0x14830000`, DSIM at `0x14800000`, display SysMMU at `0x14860000`, the display power-domain status at `0x10464084`, and six display clocks from the native clock driver. Its primary scanout register is DECON `FB_START` at offset `0x880`. The address written there is the DMA address of a newly allocated XRGB8888 buffer; it is not a fixed `0x67000000` mapping. The Linux record contains user-verified physical kernel text and color bars after this handoff.

The driver's `pipe_enable()` waits for the current command-mode frame, stops and resets DECON, disables only the display SysMMU after DMA stops, sets window format/stride/size/trigger registers, prepares DSIM while retaining firmware panel power, restores the panel's DCS column and page ranges, sets the scanout DMA address, then starts DECON and the software trigger. It checks the power domain and DSIM state before touching the display. The Samsung logo can remain in panel GRAM while there is no active scanout; this is an inference from the failed RAM-write test and the driver sequence.

## Next implementation boundary

First capture the boot handoff's DECON `VIDCON0`, `FB_START`, display SysMMU control, DSIM status/configuration and power-domain status into the existing reserved RAM log, without modifying display registers. Compare those values with the Linux driver's checks. An early XNU console must then use a bounded Exynos7580 DECON/DSIM handoff with a known physical scanout buffer and the actual clock state. Keep display support independent of the iOS userland and record physical screen confirmation separately from build and RAM-marker evidence.
