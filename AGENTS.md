# Working on the A5XELTE fork

> This repository is now the Galaxy A8 2018 (`jackpotlte`, Exynos 7885) port,
> derived from the S8 tree. For A8 work, read `JACKPOTLTE_PROGRESS.md` and
> `docs/jackpotlte-bringup.md` first. The device layer is `shim/jackpotlte/`
> and `tools/jackpotlte*` / `tools/mkadt_jackpotlte.py`. The S8 (`dreamlte`)
> and A5XELTE material below is the base we adapt and stays as reference.
> Same device rules apply: BOOT only during bring-up, USERDATA only for the
> iOS root once approved, never bootloaders/EFS/recovery; a boot stage counts
> as reached only with a photo, serial log or screen output from the phone.

- Keep private VM addresses, passwords, device dumps and downloaded Apple binaries out of git.
- Preserve recovery p11, EFS p3 and the existing Linux BOOT checkpoint. Current XNU trials may write only the explicitly authorized BOOT p10; the donor's Xiaomi `fastboot` and USERDATA instructions are not A5XELTE instructions.
- **NEVER flash `USERDATA` raw. It must be an Android sparse image, every time.**
  The iOS root is a 16 GiB raw `IOSRoot.img`; writing that raw to the phone OOMs
  the host (and the flash dies). Always: `img2simg RAW SPARSE`, verify with
  `tools/jackpotlte/verify-sparse.py RAW SPARSE`, then `fastboot flash USERDATA
  SPARSE`. `img2simg` needs the raw master *and* the ~12 GB sparse copy on disk
  at once (~28 GiB free), so check `df` before starting. This is a step that has
  been forgotten more than once — do not skip it, and do not "just try raw".
- Keep build success, recovered kernel markers, physical screen reports and iOS runtime behavior as separate claims. `NOTES.md` below is SWEET donor history; do not edit it to imply Samsung behavior.

## Original SWEET contributor instructions (donor reference)

- Read `NOTES.md` before starting; it contains the project goal, hardware facts,
  current blocker, local artifact locations and the next concrete action.
- The user switches agents frequently. Update the active handoff in `NOTES.md`
  after each milestone and before stopping. Record commits, CI run IDs, exact
  build/boot results and unfinished work. Distinguish observations from guesses.
- Commit completed changes. Use `gh` for GitHub Actions kernel builds and logs.
  Local single-file cross-compiles are useful before spending a CI run.
- Device bring-up uses temporary `fastboot boot` images. Preserve the Android
  installation and partition backups. The user approved (2026-10-15) using
  internal UFS for iOS and said Android may be deleted: the iOS root goes
  into the existing `userdata` partition (no GPT changes). Keep bootloaders,
  recovery, modem, persist and the backups intact, and confirm each
  destructive step with the user before running it.
- Keep device dumps, serial numbers and downloaded Apple artifacts out of git.
