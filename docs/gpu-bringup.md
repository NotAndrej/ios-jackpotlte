# Galaxy A8 2018 (jackpotlte) GPU / Metal bring-up

GPU engineer worktree: branch `jackpotlte-gpu`, forked from main @ 523beb5.

Plan: `docs/jackpotlte-mali.md` (M0 read-only probe -> M1 power/clock -> M2 MMU+one job -> M3 RedwoodMali-shaped user client -> M4 Metal with Bifrost v6).

## Status

- 0144-jackpotlte-mali-probe.patch added: read-only PMU/CMU/GPU_id probe, gated by `jtmaliprobe=1` (default off). Addresses from `docs/jackpotlte-mali.md` sec. 3 ( Samsung Eureka tree), not confirmed on phone.
- Not yet built or flashed. Next: dispatch xnu CI on this branch, build boot image, flash BOOT, capture serial/screen.

## Device safety

Only BOOT may be flashed during bring-up. Never bootloaders/EFS (p3)/recovery (p11)/persist/GPT/USERDATA (requires explicit owner approval per destructive step).