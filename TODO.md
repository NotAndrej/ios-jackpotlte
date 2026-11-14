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
- [x] ~~Silence the touch logging.~~ Done in `0158` (2026-11-14): every
      `A8 TOUCH:` / `A5XELTE TOUCH:` line in `bsd/dev/a5xelte_touch.c` and the
      `SWEET HID: touch ...` contact line in `SweetHID.cpp` route through one
      switch, `a8touchverbose=1`, off unless set.
- [ ] I2C4 bus stall (`A8 TOUCH: bus busy stat=0x30, recovering`, SDA held
      low). Recovery works, but the stall itself is still unexplained. Only
      chase if a fast flick still drops events.

## Status bar

- [ ] Present a real iPhone mode so the status-bar provider is found.
      **Re-requested by the owner 2026-11-11**, so the drop below is
      superseded: `0156` presents 1125x2436 on A5XELTE and the owner chose to
      squeeze the 2436 rows onto the 2220-row panel (not to crop a strip).
      Details and the phone check are in `JACKPOTLTE_PROGRESS.md`
      ("The rdar bar fix goes back in"). `sweetfb_native=1` restores the
      panel geometry from the cmdline alone.
      ~~Dropped (owner, 2026-11-11).~~ The diagnosis is in
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
      `jt43` carries `sweetfb_noconsole=1`, but this board reads
      **`a5fb_noconsole`** (see the status-bar item above), so that image mutes
      nothing and the console kept painting over the GUI. Repack with
      `a5fb_noconsole=1`; after that the remaining step is to drop the
      `jtmali*` probe args once M2/M3 are read off a photo, and stop carrying
      `sweethidtest=5`.
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
- [ ] M4: **route decided 2026-11-13 — GLES via Mesa panfrost.** The reference's
      own Metal analysis (`hakios-xnu-kernel/mtk docs/metal.md` §3/§4/§7) shows
      Metal is walled off by Apple's AIR shader format (no AIR reader exists; a
      Metal driver needs an AIR→NIR compiler) while CoreAnimation's window-server
      renderer falls back to EAGL/OpenGL ES — the path that actually draws the
      UI and wallpaper — and CoreImage's default backend already goes to GLES.
      So the CPU-rendering fix is Mesa panfrost v6 (below) plus an EAGLContext
      shim, not the Metal plug-in. The Metal plug-in (`0155`'s
      `JackpotlteMetal` bundle and the reference's AIR frontend) is deferred, and
      the licence question for its reuse is parked with it.
- [ ] M4 userspace: `userspace/jackpotlte-maliprobe` is the first tool that
      speaks the `0154` ABI from a Darwin client (`userspace/README.md`, built
      in CI by `.github/workflows/userspace.yml`, whose `--abi` step checks the
      structures and selectors against `xnu/patches/0154`). The arm64 binary
      comes out of a CI run; `tools/jackpotlte/apply-root-tweaks.sh` installs
      it at `/usr/local/bin/maliprobe` +
      `/System/Library/LaunchDaemons/com.jackpotlte.maliprobe.plist` (flashed
      2026-11-10, above), together with the GL libraries below. Written since:
      the drm stand-in (`userspace/libjackpotlte`), Mesa panfrost v6, and the
      EAGL shim; the Metal plug-in is deferred with the route (above).
- [ ] M4 Mesa (Bifrost v6), written and green in CI: `tools/mesa-jackpotlte/`
      is a four-patch series that builds Mesa 25.0.7's panfrost v6 for arm64
      iOS against `userspace/libjackpotlte` instead of libdrm, and
      `.github/workflows/mesa.yml` builds `out/libOSMesa.8.dylib` on
      `macos-15`. Run `43491401891` is green on `60aba9d` (artifact
      `mesa-osmesa-panfrost-v6`; `out/libOSMesa.8.dylib` 13,223,048 B, arm64,
      1296 exported `gl*`), so the patches apply and the library links. It has
      never been on the phone: the first real claim needs a boot and a photo.
- [ ] M4 GLES: the CoreAnimation path. On iOS the window server draws through
      `EAGLContext` (`QuartzCore`'s `EAGLServer::renderer()` tries Metal first,
      then EAGL), and `CoreImage` reaches GLES through
      `+[CIContext contextWithEAGLContext:]`. The shim replaces a handful of
      `EAGLContext` methods and backs them with Mesa (where the reference's
      `libredwood-gl.dylib` backs its own); its recorded obstacle — CoreImage
      stopping on `-[EAGLContext sharegroup]`, which it never replaced — is the
      kind of thing to expect. Written: `userspace/jackpotlte-gl/` builds
      `out/libjackpotlte-gl.dylib` (userspace CI `43489433732`, green on
      `069825f`) and installs ten `EAGLContext` methods. It sits in the root's
      `/usr/local/lib` and is on the phone as of 2026-11-14 (sparse `USERDATA`
      flash), but no frame has been drawn: that waits on the loader.
- [ ] M4 loader + first frame: `0157-jackpotlte-exec-inject.patch` adds the
      boot-arg `jtexecinject`, which the kernel applies in `exec_extract_strings()`
      (kernel CI `43496663209` green on `5d7765c`; the binary carries both
      `jtexecinject` and `DYLD_INSERT_LIBRARIES=`), and
      `tools/jackpotlte/apply-root-tweaks.sh` put the two dylibs into the master
      root's `/usr/local/lib` on 2026-11-13 (hash-matched to the CI copies), and
      the BOOT image is packed: `/var/tmp/a8-xnu-jt45-gles-inject.img` — the CI
      kernel, the Mali gates plus `a5fb_noconsole=1`, and the injection boot-arg.
      Both are on the phone since 2026-11-14: the master went to `USERDATA` as a
      **sparse** image (`img2simg` + `verify-sparse.py`, 47/47 chunks,
      `USERDATA rc=0`) and the image above to `BOOT` (`BOOT rc=0`). What is left
      is the phone: boot, photograph, read the console. Loaded is not drawn: the
      first draw claim is a frame on the panel.
- [x] M4 GLES, first piece: the OpenGL ES dispatch table of this phone's build
      (2026-11-13). `-[EAGLContext getMacroContextPrivate]` hands out a table
      of function pointers and iOS calls `entry(table[0], args...)`; the offsets
      are not in the SDK, so `tools/jackpotlte/eagl-slots.py` reads them off
      `OpenGLES.framework`'s own trampolines in the dyld shared cache. Generated
      for 20H392 (iOS 16.7.16, iPhone X) in place from the root image:
      `userspace/jackpotlte-gl/eagl_slots.txt`, 409 entries, 457 names. It
      matches the reference's 16.5 table exactly (no offset differs, same names
      per entry), so the layout held between the builds; ours is still the one
      generated from this build.
- [x] Route question settled (2026-11-13): GLES via Mesa panfrost. The licence
      work in `docs/mali-userland-provenance.md` stands as evidence, but the
      choice it fed (which reference Metal files to reuse) is deferred with the
      Metal plug-in: the GLES route takes only Mesa (upstream) and our own shim,
      so no reference Mali userland is needed for it. If the Metal plug-in is
      picked up later, **option 1 (take the granted frontend and driver, write
      our own Metal side) remains the recommendation**; the branch `mtk` carries
      no licence file and the reference's own `COMPLIANCE.md` lists its plug-in
      as "to verify".
