# iOS boot sequence after the kernel (iOS 16.x)

Research notes for the jackpotlte / sweet port. Research only — no device/source changes.
Confidence labels: [V] = verified public source/open source/official Apple docs with URL;
[C] = claim from reverse-engineered write-ups or donor notes; [I] = inference.

Scope: iOS 16.x. Older-version statements are labelled.

### Verifier's note (2026-11-08)

The data-migration narrative here (datamigrator + `migrationpluginwrapper`, backboardd drawing the Apple-logo progress UI, SpringBoard blocked in `-[BKSSystemShellService _checkIn]`) is not invented: `NOTES.md` records the exact backtraces and the `SWEET SURF`/`SWEET FB` lines from the SWEET donor. But two limits apply:

- It is **donor** history (Redmi Note 10 Pro), which `AGENTS.md` says must not be read as Samsung behaviour. Whether the A8 at 70–75 % is in the same migration phase is an **inference**, not an observation — nothing in `JACKPOTLTE_PROGRESS.md` mentions migration yet.
- No public source maps progress-bar percentages to processes. The 70–75 % ↔ datamigrator statement is a donor-correlation hypothesis; the console grep list in §6 is the way to confirm it on the A8.

---

## 1. Order after the kernel

High-level: BootROM -> iBoot -> XNU -> /sbin/launchd (PID 1) -> launchd cache loader
-> early daemons -> backboardd/window server -> (data migration gate) -> SpringBoard -> optional Setup.app.

launchd is the only immortal process (BSD init); on iOS one system instance, no
per-user instance. IOS has no runlevels: launchd loads plists from
/System/Library/LaunchDaemons (and /Library/LaunchDaemons) and lets Mach ports
provide ordering rather than a fixed linear list. [V] newosxbook.com "The Alpha
and the Omega — launchd", ch.7; [V] Apple "Creating Launch Daemons and Agents".

launchd startup:
1. Kernel main thread spins bsdinit_task, names it init, calls load_init_program()
   -> execve("/sbin/launchd"). [V] newosxbook ch.7.
2. launchd reads all LaunchDaemon plists, registers all Mach ports/sockets, then
   launches jobs whose RunAtLoad/OnDemand policy requires it. [V] Apple launchd.
3. launchd_cache_loader (/usr/libexec/launchd_cache_loader) validates the dyld
   shared cache / unsecure-cache. Without launchd_unsecure_cache= (or a NOP of
   the cbz/cbnz near "unsecure_cache") modified plists/caches are rejected, so
   daemons never start. [C] vphone-cli patcher; [C] JACKPOTLTE —
   launchd_unsecure_cache=1 is what let SpringBoard exec on our tree.

Named-service entry order:
- logd/notifyd/configd-ish core: logging, notifications, network config. logd's
  migrate-dir crash was a wall in the donor port. [C] NOTES 0045n.
- runningboardd: process lifecycle/assertions; ios13+ replaces legacy
  assertiond. [V] eclecticlight (macOS analogue, same family) + [I].
- cfprefsd (com.apple.cfprefsd.xpc.daemon): CFPreferences backing store.
  [V] theiphonewiki.
- mobilegestaltd: device-property queries; wrong answers steer activation. [C] hanakim3945.
- mobileactivationd (as mobile): activation; hactivation branches via MobileGestalt.
  [V] theiphonewiki/shindan; [C] NOTES v12.
- securityd/keybagd/AKS: keybag; usersession.kb ENOTSUP on HFS+. [C] NOTES.
- mediaserverd: AVFoundation; crash loop on missing GL/media path. [C] NOTES.
- backboardd: window server / display / touch routing; holds
  BKSSystemShellService _checkIn until data migration finishes. [C] NOTES.
- SpringBoard: shell UI; blocked in synchronous XPC _checkIn. [C] NOTES, eShard.
- com.apple.datamigrator (process name truncated "com.apple.datami"): post-restore
  migration; gates SpringBoard. [C] NOTES, vphone-cli, eShard.
- Setup.app (com.apple.purplebuddy): Setup Assistant if not activated/done. [V] theapplewiki.
- CommCenter: baseband; crash-loops with no baseband and can gate finish. [C] eShard, NOTES.
- lsd/installd/etc.: launch services/app install. [C] NOTES.

Legacy assertiond: only older iOS; iOS 16 uses runningboardd. [I] based on
RunningBoard intro; [V] eclecticlight.

backboardd vs SpringBoard handshake (the concrete block): SpringBoard's main
thread is stuck in mach_msg2_trap <- dispatch_mach_send_and_wait_for_reply <-
xpc_connection_send_message_with_reply_sync <- -[BSXPCServiceConnectionMessage
_sendSynchronously:] <- -[BKSSystemShellService _checkIn] <- FBSystemShellInitialize
<- SBSystemAppMain. backboardd will not complete check-in until data migration
finishes. [C] NOTES 0055n sample.

---

## 2. On-screen progress bar mapping (honestly)

The Apple-logo + progress bar is the data-migration progress UI, drawn by
backboardd (IOSurface/CoreAnimation) while com.apple.datamigrator runs its
plugins; the last plugin is SpringBoard.migrator (waits on PosterBoard poster
migration). [C] NOTES v0053s (full in ~60 min on 1 CPU), v0055n (99% then
crawled), v12 (seeded record to skip).

Apple does not publicly document any percentage->daemon/stage mapping, nor which
"10/50/75/100 %" aligns with what. NO PUBLIC DOCUMENTATION FOUND for the
percentage semantics. Donor observations only: ~40% at capture (JACKPOTLTE
photo), ~50% promptly on 4 CPUs, 99% near end, ~60 min to full on 1 CPU.
[C] JACKPOTLTE_PROGRESS; NOTES.

Public (narrower) knowledge: eShard's iOS 14 QEMU work showed backboardd and
SpringBoard waiting on "data migration", and skipping datamigrator + patching
BackBoardServices to not wait moved boot forward. [V] eShard 2025-06-04.
FrontBoard logs the milestone name:
"[FBServiceFacilityServer] Milestone reached: FBSServiceMilestoneDataMigrationCompleted"
after which SpringBoard decides whether to run Setup.app. [C] vphone-cli
eebe6808 (iOS 26/27, empirically measured).

---

## 3. Known gates

A. Activation — mobileactivationd.
Runs as mobile; checks MobileGestalt production/SEP status. Hactivation branches
fire on dev-fused/fpga/internal/demoted. Otherwise stays Unactivated and
SpringBoard presents Setup.app. [C] hanakim3945; NOTES v12.
Workarounds: seed data_ark.plist (-ActivationState Activated), ADT
certificate-production-status etc, or patch -[DeviceType should_hactivate] to
YES. [C] NOTES v12; vphone-cli.
Desired log: "MobileActivation says the device is: [Activated]" (appears after
data migration milestone). [C] NOTES.

B. Setup.app vs SpringBoard.
SpringBoard reads com.apple.purplebuddy once at start and runs Setup while
SetupDone != true or SetupVersion < BYBuddyIOSCurrentVersion (11 on 26/27).
[V] theapplewiki; [C] vphone-cli.
Workaround: pre-write SetupDone=true, SetupFinishedAllSteps=true, SetupVersion=11
plus managedconfiguration flags. [C] NOTES.
Do not just delete Setup.app on iOS 13.3+: system-wide check panics on
unrecognised Setup.app. [V] theapplewiki.

C. AMFI / signing / entitlements.
Ported trees use a built-in MAC policy that gives every signed rootfs binary
CS_PLATFORM_BINARY and attaches entitlements to the writable copy. [C] NOTES.
Stock hardware uses static trust cache; missing entries -> exec failures.
WRONG_PLATFORM (exec, 0xe): execing an iOS binary from a non-iOS platform
without psa_platform==PLATFORM_IOS posix_spawn attribute. Workaround:
posix_spawnattr_set_platform_np(...PLATFORM_IOS...) + spawn suspended, patch
dyld, claim entitlement via xpc_copy_entitlements_for_self, then SIGCONT.
[V] Project Zero 2021-05-01.

D. launchd_unsecure_cache.
Gate in /usr/libexec/launchd_cache_loader; without the flag (or NOP of the branch
near "unsecure_cache") modified plists/cache rejected -> no daemons. [C] vphone-cli.

E. Data migration re-running every boot.
Decision uses com.apple.DataMigration.plist LastSystemVersion vs current build
plus sentinel /private/var/datamigrator/migrate and DMUserDataDispositionManager.
Mismatch -> migration repeats 60-180s every boot (worse on 1 CPU). [C] NOTES.
Workaround: pre-populate com.apple.migration (LastSystemVersion +
DMLastMigrationResults.buildVersion = current build) and/or patch datamigrator to
skip. [C] eShard; NOTES v12 confirms.

F. Hardware-absent daemons.
CommCenter crash-loops without a baseband and can gate SpringBoard finish;
workaround disable com.apple.CommCenter. [C] eShard; NOTES. mediaserverd crashes
on missing GL/media path; config tweaks. [C] NOTES.

---

## 4. Headless / jailbroken / non-Apple boot differences

- QEMU (public, iOS 14): patch com.apple.datamigrator to skip migration and
  patch BackBoardServices so backboardd does not wait; patch MobileKeyBag /
  SpringBoardFoundation / SpringBoard for absent SEP; disable CommCenter.
  SpringBoard reaches -applicationDidFinishLaunching:. [V] eShard.
- Hacktivation (checkm8, iOS 13-16): patch/rename mobileactivationd; or patch
  MobileGestalt ShouldHactivate; persists if signed. Older method deleted
  Setup.app, broken by 13.3. [V] phonlab, theapplewiki.
- Semi-tethered jailbreaks (palera1n): ramdisk dumps blob, fakefs, patches
  kernel + installs loader; mobileactivationd hactivation; re-exploit on reboot.
  [V] palera1n (public repo).
- Virtualized (vphone-cli): patches seputil, cache loader gate,
  mobileactivationd should_hactivate, launchd jetsam guard, datamigrator env,
  GPU bundle, etc. [V] vphone-cli.
- Our port: iOS on Android (Snapdragon/Exynos) config; launchd_unsecure_cache=1,
  seeded migration/activation/data ark/purplebuddy keys, in-kernel IOSurface/
  IOMobileFramebuffer display, samples SpringBoard/backboardd. [C] NOTES.

---

## 5. Table (order / process / does / failure / source)

| Order | Process | What it does | Typical failure | Source |
|---:|---|---|---|---|
| 1 | XNU | mounts root, IOKit | mount panic, kext | Apple security (V) |
| 2 | launchd | PID1 spawns all | wrong platform/cache | newosxbook (V), P0 (V) |
| 3 | launchd_cache_loader | validates cache | rejects modified cache | vphone-cli (C) |
| 4 | logd/notifyd | logging/notify | migrate-dir crash | NOTES (C) |
| 5 | runningboardd | process/assertion mgmt | missing assertions | eclecticlight (V, mac) |
| 6 | cfprefsd | pref store | nil prefs | theiphonewiki (V) |
| 7 | mobilegestaltd | device props | wrong model->activation | hanakim3945 (C) |
| 8 | mobileactivationd | activation | Unactivated->Setup | theiphonewiki (V), NOTES (C) |
| 9 | securityd/keybag/AKS | keybag | kb ENOTSUP | NOTES (C) |
| 10 | mediaserverd | media | GL prewarm NULL | NOTES (C) |
| 11 | backboardd | window server | SIG6 no display | NOTES (C) |
| 12 | datamigrator | migration, gates SB | 60-180s, re-runs | NOTES/vphone/eShard (C) |
| 13 | SpringBoard | shell UI | blocked in _checkIn XPC | NOTES (C) |
| 14 | Setup.app | setup | runs if not done | theapplewiki (V) |
| 15 | CommCenter | baseband | crash loop | eShard (V, iOS14) |

---

## 6. Reading our console at 70-75 %

Percentages are not authoritative; 70-75% on our port is mid-data-migration.
Grep the serial log (with OS_ACTIVITY_DT_MODE=YES and CFLOG_FORCE_STDERR=1 on
the jobs you care about) for:

- exec lines (patch-0042 style) and exec SpringBoard/backboardd/mediaserverd.
- com.apple.datami / com.apple.datamigrator: migration XPC service start;
  "[FBServiceFacilityServer] Milestone reached: FBSServiceMilestoneDataMigrationCompleted"
  means it finished.
- migrationpluginwrapper: each plugin; last is SpringBoard.migrator.
- SpringBoard blocked: BKSSystemShellService _checkIn,
  BSXPCServiceConnectionMessage _sendSynchronously, sweetsample traces.
- backboardd: window-server dispatch_once, IOMobileFramebuffer opens, IOSurface,
  swap lines.
- Activation: mobileactivationd "MobileActivation says the device is:",
  "DeviceType should_hactivate"; mobilegestaltd answers.
- CRASH <name> lines (patch 0043) for each crashing daemon.
- runningboardd, assertiond (if present), cfprefsd, mediaserverd, CommCenter,
  launchd_cache_loader.
- Migration state probe: read com.apple.migration for mobile:
  LastSystemVersion and DMLastMigrationResults.buildVersion; matching current
  build means finished (while datamigrator XPC still running during migration).
  [C] vphone-cli.

Rule of thumb: if at 70-75% you still see com.apple.datami running with periodic
migrationpluginwrapper execs, the bar is expected to keep moving. If datami is
gone but backboardd still swaps the same frame, the migration record likely
mismatched and re-ran — reseed com.apple.migration.

---

## Sources

- Apple Support, "Boot process for iPad and iPhone devices"
  https://support.apple.com/guide/security/boot-process-for-ipad-and-iphone-devices-secb3000f149/web
- Apple (archive), "Creating Launch Daemons and Agents"
  https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/CreatingLaunchdJobs.html
- Wikipedia, "Launchd" https://en.wikipedia.org/wiki/Launchd
- J. Levin, "The Alpha and the Omega — launchd", newosxbook.com
  https://newosxbook.com/articles/Ch07.pdf
- worthdoingbadly, "Tutorial – emulate an iOS kernel in QEMU up to launchd and
  userspace" https://worthdoingbadly.com/xnuqemu2/
- eShard, "Emulating an iPhone in QEMU – Part II" (2025-06-04)
  https://www.eshard.com/blog/emulating-ios-14-with-qemu-part2
- Project Zero, "Fuzzing iOS code on macOS at native speed" (2021-05-01)
  https://projectzero.google/2021/05/fuzzing-ios-code-on-macos-at-native.html
- theiphonewiki "Services" https://www.theiphonewiki.com/wiki/Services
- theapplewiki "Setup.app"
  https://theapplewiki.com/wiki/Filesystem:/Applications/Setup.app
- theapplewiki "Hacktivation" https://theapplewiki.com/wiki/Hacktivation
- hanakim3945, "Hactivation via MobileGestalt POC" (2025-05-01)
  https://hanakim3945.github.io/posts/gestalthax/
- OutrageousStorm/ios-security-research (2026-03-26)
  https://github.com/OutrageousStorm/ios-security-research
- phonlab, iCloud bypass guide (2021-06-18) https://phonlabtech.com/icloud-lock-bypass-guide/
- eclecticlight.co, "RunningBoard" (2019-11-07)
  https://eclecticlight.co/2019/11/07/runningboard-a-new-subsystem-in-catalina-to-detect-errors/
- shindan.io KB: mobileactivationd
  https://kb.shindan.io/kb/ios/processes/daemons/mobileactivationd/
  launchd https://kb.shindan.io/kb/ios/processes/daemons/launchd/
- vphone-cli (public): FirmwareGuestSystemPatchSet, cfw_patch_cache_loader.py,
  GuestFirstBootSettle.swift (+ commit eebe68083a)
  https://gitea.yiem.net/yiem/vphone-cli
- Local (read-only, donor): NOTES.md, JACKPOTLTE_PROGRESS.md, README.md in this
  repo. Claims from donor notes labelled [C]. Per AGENTS.md, NOTES.md is donor
  history, not Samsung device results.

No commit performed. Research document only.
