# jackpotlte (Galaxy A8 2018) — open work

Small, ordered list of things that are known and not yet done. Device results
live in `JACKPOTLTE_PROGRESS.md`; this file is the "what's next" index.

Safety rules are unchanged: BOOT only during bring-up, USERDATA (the iOS root)
only for approved destructive steps, never bootloaders / EFS (p3) / recovery
(p11) / persist / GPT.

## Touch

- [ ] GPU-style verification of drags end to end: `moves=` climbing with no
      `lift watchdog, releasing` on a fast flick (jt37 result pending).
- [ ] ~~Remove the `SWEET HID: clients buttons=... touch=...` spam.~~ Done in
      `0151` (gated behind `sweethidclients=1`); the boot-arg `sweethidtest`
      no longer emits it.
- [ ] I2C4 bus stall (`A8 TOUCH: bus busy stat=0x30, recovering`, SDA held
      low). Recovery works, but the stall itself is still unexplained. Only
      chase if a fast flick still drops events.

## Device identity

- [ ] **iOS identifies as an iPad in the Apple Account sign-in flow.** The
      About screen says "iPhone"/16.7.16, but the sign-in sheet calls the
      device an iPad. `hw.machine`/`hw.model`/`hw.target`/`hw.product` come
      from the ADT root (`0059-sweet-iphone-identity.patch`,
      `tools/mkadt_jackpotlte.py`), and the donor `model`/`/product` node
      comes from `research/ipsw-x/d22ap.adt`, which is **not in git** and was
      missing when the last image was packed (mkadt warned "no /product
      node"). So MobileGestalt gets no usable `ProductType` and the sign-in
      UI guesses. Confirm at runtime (`sysctl hw.machine hw.model hw.target
      hw.product`, and the MobileGestalt cache
      `.../com.apple.MobileGestalt.plist`) before changing anything: the
      cached plist is keyed by build + UUID and stale values survived a
      reflash on the donor, so a clean reflash may be needed to see a fix.

## Performance (the phone is very laggy)

- [ ] Software compositing: no GPU driver, so SpringBoard draws everything on
      the CPU. This is the expected main cause and the Mali port is the fix
      (`docs/gpu-bringup.md`; M0/M1 are done on hardware, M2 is built and
      waiting on a boot).
- [ ] Console cost: every `printf`/`IOLog` redraws the framebuffer console.
      The test cmdline should stop carrying `-enable_kprintf_spam` and
      `sweethidtest=5` once the touch diagnosis is finished.
- [ ] `exec %s` prints on every exec (`0042`/`0111`) — consider gating behind
      a boot arg like the other diagnostics.

## GPU

- [ ] Boot the M2 image `/var/tmp/a8-xnu-jt34-gpum2.img`
      (`jtmaliprobe=1 jtmalipwr=1 jtmalijob=1`) and read
      `word=0xc0ffee01` for both jobs. Next milestone after that: MMU-backed
      rendering (M3).
