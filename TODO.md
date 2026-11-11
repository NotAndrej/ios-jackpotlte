# jackpotlte (Galaxy A8 2018) — open work

Small, ordered list of things that are known and not yet done. Device results
live in `JACKPOTLTE_PROGRESS.md`; this file is the "what's next" index.

Safety rules are unchanged: BOOT only during bring-up, USERDATA (the iOS root)
only for approved destructive steps, never bootloaders / EFS (p3) / recovery
(p11) / persist / GPT.

## Touch

- [x] ~~GPU-style verification of drags end to end: `moves=` climbing with no
      `lift watchdog, releasing` on a fast flick (jt37 result pending).~~
      Done on device in jt37: the About list scrolls by dragging.
- [ ] ~~Remove the `SWEET HID: clients buttons=... touch=...` spam.~~ Done in
      `0151` (gated behind `sweethidclients=1`); the boot-arg `sweethidtest`
      no longer emits it.
- [ ] I2C4 bus stall (`A8 TOUCH: bus busy stat=0x30, recovering`, SDA held
      low). Recovery works, but the stall itself is still unexplained. Only
      chase if a fast flick still drops events.

## Status bar

- [ ] ~~Present a real iPhone mode so the status-bar provider is found.~~
      **Dropped (owner, 2026-11-11).** The diagnosis is in
      `JACKPOTLTE_PROGRESS.md` ("Why the status bar reads"); nothing was ever
      flashed for it. For the record if it is ever wanted: `display-scale` is
      a u32, so the 12 mini's 2.88 is not expressible and only a 3x mode UIKit
      knows in both points and pixels works (1125x2436 = 375x812), which costs
      22 px of width and 8.9% of height on a 1080x2220 panel.
- [ ] `sweetfb_noconsole=1` does nothing on the A8: this board reads
      **`a5fb_noconsole`** (`0109`). `jt31`/`jt38`/`jt43` were all packed with
      the wrong name, so the console kept painting over the GUI in every one
      of them. **Confirmed on the device 2026-11-11** (lockscreen up, kernel
      text still drawn over it). Repack with `a5fb_noconsole=1`; nothing else
      has to change.

## Home screen shows only ~6 apps

- [x] Move the 32 bundles in `/private/var/staged_system_apps` (Maps, Music,
      Weather, Clock, Calculator, Files, Books, Notes, Mail, ...) into
      `/Applications`: iOS 16 installs them onto the App cryptex at first boot,
      which our single plain-HFS root cannot do, so LaunchServices never
      registers them. Host side is ready -
      `tools/jackpotlte/apply-root-tweaks.sh` (and the same step in
      `tools/mkiosroot.sh`).
      Flashed 2026-11-10 (owner approved) with the 32 apps promoted and the
      GPU probe installed. **All of them show on the home screen on the
      2026-11-11 boot** - no LaunchServices cache clearing was needed.

## Device identity

- [ ] **iOS identifies as an iPad in the Apple Account sign-in flow.** The
      About screen says "iPhone"/16.7.16, but the sign-in sheet calls the
      device an iPad.
      Root identity is set in `tools/mkadt.py` (root `compatible` =
      `D22AP, iPhone10,3, xiaomi,sweet, ...`, `model` = `iPhone10,3`,
      `target-type` = `D22`), so `hw.product` should be `iPhone10,3`, which
      MobileGestalt recognises.
      **But `research/` does not exist in either repo, so
      `research/ipsw-x/d22ap.adt` is absent and mkadt prints "no /product
      node"**: the ADT we boot has *no* `/product` node at all, so
      MobileGestalt gets no product properties to derive ProductType /
      device class / artwork from. That is the most likely cause.
      Fix: extract the real donor tree (Apple artifact, kept out of git) —
      `tools/remotezip.py URL DeviceTree.d22ap.im4p DIR` then
      `ipsw img4 im4p extract DIR/.../DeviceTree.d22ap.im4p -o
      research/ipsw-x/d22ap.adt` — and repack.
      Confirm at runtime first (`sysctl hw.machine hw.model hw.target
      hw.product` + the MobileGestalt cache
      `.../com.apple.MobileGestalt.plist`); the cache is keyed by build +
      UUID and stale values survived a reflash on the donor, so a clean
      reflash may be needed before a fix is visible.

## Performance (the phone is very laggy)

- [ ] Software compositing: no GPU driver, so SpringBoard draws everything on
      the CPU. This is the expected main cause and the Mali port is the fix
      (`docs/gpu-bringup.md`; M0/M1 are done on hardware, the M2 image is
      packed and waiting on a boot).
- [ ] Console cost: every `printf`/`IOLog` redraws the framebuffer console.
      `jt43` puts `sweetfb_noconsole=1` back (mutes the panel at iOS's first
      frame: the Apple logo and GUI stay clean, early text still draws), so the
      remaining step is to drop the `jtmali*` probe args once M2/M3 are read
      off a photo, and stop carrying `sweethidtest=5`.
- [ ] `exec %s` prints on every exec (`0042`/`0111`) — consider gating behind
      a boot arg like the other diagnostics.

## GPU

The GPU series is on `main` (the `jackpotlte-gpu` branch was merged and
deleted on 2026-11-10); build results and the image details live in
`docs/gpu-bringup.md`.

- [ ] Boot `/var/tmp/a8-xnu-jt39-gpum2.img` (sha256 `ae6a99e9...2379fde`,
      `jtmaliprobe=1 jtmalipwr=1 jtmalijob=1`) and read `word=0xc0ffee01` for
      both jobs — that closes M2.
- [ ] M3 (`0153` engine + `0154` user client, gated by `jtmalimetal=1`):
      ~~confirm the build contains `engine up, addresses from` and
      `JackpotlteMali:`~~ done — run `42871632907` is green and its real
      kernel carries both, and the image is
      `/var/tmp/a8-xnu-jt42-gpum3.img` (sha256 `536ab7ed...4a120901`).  What
      is left is the boot: read `JACKPOTLTE MALI: engine up` plus
      `JackpotlteMali: the GPU is there for programs to draw with`.
- [ ] M4: the `JackpotlteMetal` bundle (Metal plug-in + AIR frontend) and Mesa
      built with Bifrost **v6**.  Its first step is written: `0155` publishes
      the `IOAcceleratorES` service and, under boot-arg `jtmetal=1`, the two
      properties that name the plug-in (`docs/gpu-bringup.md`).
- [ ] M4 userspace, started: `userspace/jackpotlte-maliprobe` is the first tool
      that speaks the `0154` ABI from a Darwin client (`userspace/README.md`,
      built in CI by `.github/workflows/userspace.yml`, whose `--abi` step
      checks the structures and selectors against `xnu/patches/0154`).
      Not in the root image yet: the arm64 binary comes out of a CI run, and
      putting it at `/usr/local/bin/maliprobe` +
      `/System/Library/LaunchDaemons/com.jackpotlte.maliprobe.plist` belongs in
      `tools/jackpotlte/apply-root-tweaks.sh` so the root keeps one mount pass.
      Next after that: the `redwood_drm`-shaped shim (`RedwoodMali` ->
      `JackpotlteMali`), then Mesa panfrost v6, then the Metal plug-in.
- [ ] Open question for the owner: the reference userspace (shim + Metal
      plug-in + AIR frontend + Mesa patches) is a ~6k-line import whose licence
      the reference itself marks "to verify".  Import it or write ours?
