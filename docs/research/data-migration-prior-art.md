# DataMigration prior art on XNU/iOS-on-Android ports

Research note. READ-ONLY web research; no device or repo changes. Created 2026-11-08.

Scope: public record of whether other XNU-on-Android/iOS-on-phone ports hit the
same endless DataMigration / stuck 100% progress-bar hang, and how they escaped
it. Claims are labelled:

- **verified** = I fetched the source and can quote it (URL + file/section given).
- **claim** = someone asserts it (README/progress note) but I did not run it.
- **inference** = my reasoning.

Primary constraint applied: no repo was assumed to exist; all were checked with
`gh api repos/<owner>/<repo>` first. UniXNU/* and device repos that never reached
userland are labelled as such and not padded.

---

## 1. Per project: did they hit the same DataMigration stall?

### dipa-studio/ios-sweet (donor of record)

- Repo: `https://github.com/dipa-studio/ios-sweet` (1320 stars, main).
- File: `NOTES.md`, sections "MILESTONE: FIRST iOS PIXELS..." / "BOOT 0054n" / "BOOT 0055n" / "v10 did NOT stop the migration" / "ROOT CAUSE of the endless migration" / "v11 still migrated". URL:
  `https://raw.githubusercontent.com/dipa-studio/ios-sweet/main/NOTES.md`.
- **verified**: bar reached 100% in ~60 min on cpus=1; SpringBoard blocked in
  `-[BKSSystemShellService _checkIn]` (`- FBSystemShellInitialize <- SBSystemAppMain`);
  backboardd drew the progress UI. Quote: "the iOS data-migration boot screen (Apple logo + progress bar) was shown and the bar advanced to full in ~60 minutes."
- Root cause actually identified: no `IOPlatformUUID` (XNU publishes gethostuuid only
  from `/chosen/unique-chip-id` + `/chosen/chip-id`), so CurrentHost pref writes
  failed and `lastBuildVersionPref` always read empty; plus missing `LastSystemVersion`
  in the plist actually read. Fix added those ADT properties (UUID
  `A9683613-3704-53F2-95D0-98A8FB574E78`) and `LastSystemVersion=20H392` in the
  plain `com.apple.migration.plist` (**verified in our local bring-up notes, but they
  are donor history, not Samsung results**).

### georgev-6974/s9plus-ios

- Repo: `https://github.com/georgev-6974/s9plus-ios` (0 stars, main) — **verified to
  exist** despite being reported as missing by a previous agent.
- File: `STAR2LTE_PROGRESS.md`, section "Why every root write makes the next boot
  slow" (2026-11-08). Quote: "after s9root/s9delta the first boot takes 15+ min
  (progress bar), later boots ~1 min."
- AKS note: sel 34 (`aks_get_extended_device_state`? actually listed as
  "sel 93, 34 (datamigrator), 31 (profiled)" unsupported) in `STAR2LTE_PROGRESS.md`.
- **inference**: it inherited the donor stall but is still an early fork with
  nothing fully running; no independent migration conclusion is recorded beyond the
  donor copy of `NOTES.md`. Its migration-related file content is literally the
  `dipa-studio/ios-sweet` notes verbatim.

### danstronics/ios-s9plus-exynos

- Repo: `https://github.com/danstronics/ios-s9plus-exynos` (24 stars, main).
- Evidence (verified by file fetch):
  - `core/userland/mkjobs.py`: `'datamigrator'` is in `DENY`, so the daemon is not
    launched.
  - `core/userland/var/mobile/Library/Preferences/com.apple.migration.plist` ships
    `LastSystemVersion = 20F66` plus `DMLastMigrationResults` (success, buildVersion
    `20F66`), labelled in-file: "without this record of the build that last ran the
    data migration, backboardd runs all the migrators (the progress bar under the
    logo) each time."
- Userland is iOS 16.5 supplied by the user (README), not 20H392. **claim**: UI is
  running on-device (README describes GLES, touch, battery, Wi-Fi) — so the
  migration bar avoided via a HakiOS-style deny + success plist; no `IOPlatformUUID`
  claim is recorded. URL of the migration plist:
  `https://raw.githubusercontent.com/danstronics/ios-s9plus-exynos/main/core/userland/var/mobile/Library/Preferences/com.apple.migration.plist`.

### MakrSas/ios-topaz

- Repo: `https://github.com/MakrSas/ios-topaz` (627 stars, branch `topaz`).
- **claim**: phone reaches the iOS home screen Oct 20 2026 (README).
- File: `tools/sweet-root-tweaks.py` (fetched verbatim). Quote:
  "migration       LastSystemVersion so DataMigration does not run for an hour
  on every boot". It writes `LastSystemVersion` **and** `DMUserDataDisposition =
  {SyntheticDidUpgrade: False, buildVersion: 20H392}` into plain
  `com.apple.migration.plist`, plus a seeded mobileactivationd data ark with
  `-DataMigrationCompleted: True` and `-PreSydroDataMigrationCompleted: True`.
  URL: `https://raw.githubusercontent.com/MakrSas/ios-topaz/main/tools/sweet-root-tweaks.py`.

### joranv1/ios-sailfish (Pixel 1, Adreno 530)

- Repo: `https://github.com/joranv1/ios-sailfish` (88 stars, main).
- **claim**: boots to the lock screen in ~20 s, all stock apps (README).
- File: `tools/sweet-root-tweaks.py` (fetched verbatim). Contrarian quote:
  "migration       /private/var/datamigrator/migrate sentinel: DataMigration runs
   once on first boot (it registers the app extensions; without it no wallpapers
   or widgets). Skipping it via LastSystemVersion left LaunchServices with 1 of
   524 appex."
- So **sailfish deliberately keeps migration running once by creating
  `/private/var/datamigrator/migrate`**. That is the single clearest public-record
  data point that the *migration must happen once* for full UI (wallpapers/widgets),
  and skipping it has a visible cost. It still skips Setup (`SetupDone` etc.) and
  seeds the activation data ark. URL:
  `https://raw.githubusercontent.com/joranv1/ios-sailfish/main/tools/sweet-root-tweaks.py`.

### oosexgang/hakios-xnu-kernel (HakiOS)

- Repo: `https://github.com/oosexgang/hakios-xnu-kernel` (2635 stars, main).
- **claim**: reaches GUI/Metal on Adreno (README news, 2026).
- Evidence (verified):
  - `userland/mkjobs.py`: `'datamigrator'` is in the `DENY` list, i.e. the daemon
    is stripped from `launchd.plist` entirely.
  - `userland/var/mobile/Library/Preferences/com.apple.migration.plist` is shipped
    with:
    ```
    <key>LastSystemVersion</key><string>20F66</string>
    <key>DMLastMigrationResults</key><dict><key>success</key><true/>
       <key>buildVersion</key><string>20F66</string></dict>
    ```
    Header comment in-file: "without this record of the build that last ran the
    data migration, backboardd runs all the migrators (the progress bar under the
    logo) each time."
  - Userland is iOS 15.5 (`20F66`), not iOS 16.7 20H392. URLs above.
- **inference**: HakiOS eliminates the stall two ways at once — the migration
  daemon never launches **and** the preference records a prior successful build.
  This matches the "shortest path" of skipping migration entirely.

### Floppikrob/ios-galaxy-s8 (S8 fork)

- Repo: `https://github.com/Floppikrob/ios-galaxy-s8` (44 stars).
- `NOTES.md` is verbatim donor history from `dipa-studio/ios-sweet` (same quotes
  as above); `tools/sweet-root-tweaks.py` writes `LastSystemVersion`. Reaches the
  lock screen on the S8 (README).

### UniXNU family

- `UniXNU/UniXNU`: build framework only; not a data-migration data point.
- `Droid-VM/UniXNU-droidvm`: iPadOS 27 guest; requires `--unique-chip-id` "seed XNU
  IOPlatformUUID" (`scripts/make-droidvm-adt.py`). Patch
  `ipados27/patches/xnu-12377/ipados27-bringup.patch` documents: a sealed
  `/private/var/staged_system_apps` directory is consumed by SystemAppMigrator on
  "first boot's data migration (takes over 20 minutes here)"; an owner Data seed
  "marks the migration done" and with `owner-staged-apps=1` the staged apps are
  presented through a bindfs union over `/Applications`. URL:
  `https://raw.githubusercontent.com/Droid-VM/UniXNU-droidvm/main/ipados27/patches/xnu-12377/ipados27-bringup.patch`.
- `MiLacy/UniXNU-marble`: stage 2 only verified; stage 3 unverified — no migration
  evidence; dropped (as requested).
- `Emilelu/UniXNU-mondrian`: reaches a first-frame progress bar only
  (`docs/BOOT-STALL-2026-10-31.md`) then `launchd` SIGSEGV. It is the only
  other source that explicitly quotes our donor chain. Quote:
  "没有 IOPlatformUUID，DataMigration 永远记不下 `LastSystemVersion`，而 backboardd
  会在 `-[BKSSystemShellService _checkIn]` 里扣住 SpringBoard 直到
  `DMPerformMigrationIfNeeded` 返回."
  Migration plist ships `LastSystemVersion`/`DMLastMigrationResults` at 20G81 (after
  `userland/build.py`). URL:
  `https://raw.githubusercontent.com/Emilelu/UniXNU-mondrian/main/docs/BOOT-STALL-2026-10-31.md`.
- `FoXLinne/UniXNU_marble`: data-seed tooling. Quote
  (`ios18/source/scripts/seed-data-skip-setup.py`):
  "com.apple.migration              LastSystemVersion (no data migration on every
  boot)"; `--build` "must equal it, or the system migrates its data on the first
  boot (the progress bar under the Apple logo)."
  So it directly corroborates the gate relationship. URL:
  `https://raw.githubusercontent.com/FoXLinne/UniXNU_marble/main/ios18/source/scripts/seed-data-skip-setup.py`.

### MC-XiaoXiao/ios_sagit

- Repo: `https://github.com/MC-XiaoXiao/ios_sagit` (14 stars).
- **claim** (README): device reaches the SpringBoard home screen with app icons and
  Dock on iOS 18 (SDM 835).
- Migration-related (verified code):
  - `tools/project/build/services.py` contains `patch_migration_unlock`: a
    byte-exact patch of the pinned datamigrator XPC at `0xd040` "Skip only
    negative MKB lock-state results"; manifest reports `hardware_tested: False`,
    `installed: False` despite README's stronger claim.
  - `tools/project/firmware/image.py` lists `"migration-services"` among optional
    firmware profile patches; `mkjobs.py` ... filtering not shown in this fetch.
  - `core` not inspected further (rate limit).
- **label**: claim unverified as to migration handling; it is iOS 18 ARM64e, a
  different scale than our iOS 16.7 path.

### lechaukha12/xnu-xzs (Sony Xperia XZs)

- Repo: `https://github.com/lechaukha12/xnu-xzs` (13 stars).
- **claim** (README): hardware-proven iOS 16.7.16 Home/Lock Screen.
- Migration evidence (verified `src/xzs-userland/inject.c`):
  - comment: "com.apple.migration.plist — prevent datamigrator from running on boot"
  - it writes `LastSystemVersion = 20H392` into three preference locations
    (mobile/root/Library/Preferences and `/Library/Preferences`), **and** it also
    disables the job by listing `"datamigrator"` (and `"purplebuddy"`) in
    `k_deny_substrings`, plus renames the framework XPC to/from
    `com.apple.datamigrator.xpc.disabled`.
  - ADT adds `/chosen/unique-chip-id = 0x585a534b45594101`.
  URL: `https://raw.githubusercontent.com/lechaukha12/xnu-xzs/main/src/xzs-userland/inject.c`.

---

## 2. Projects that reached Home Screen / Setup, and how (migration-wise)

- **MakrSas/ios-topaz** and **joranv1/ios-sailfish** inherited the `dipa-studio`
  donor tweaks; READMEs claim home/lock screen. Topaz's script skips migration
  entirely (`LastSystemVersion` + disposition) and skips Setup. Sailfish proves the
  *cost*: it deliberately runs the migration once via sentinel, because skipping
  it programmatically ("Skipping it via LastSystemVersion") left only 1 of 524 app
  extensions registered (no wallpapers/widgets). So the "shortest" home-screen
  path and the "complete" UI path are **different**: skip-to-boot vs run-twice.
- **oosexgang/hakios-xnu-kernel** and **danstronics** reach UI without any
  migration pass by (a) shipping `LastSystemVersion`/`DMLastMigrationResults`
  `success`, and (b) deleting the daemon job. This is the shortest *demonstrated*
  path to the SpringBoard UI but it is on iOS 15.5 (20F66), not iOS 16.7.
- **lechaukha12/xnu-xzs** reaches Home Screen on iOS 16.7 with the same skip
  strategy plus job denial and `unique-chip-id`.
- **Droid-VM/UniXNU-droidvm** reaches past SystemAppMigrator only by shipping a
  pre-migrated Data seed ("owner Data seed marks the migration done" — a one-time
  local build step, ~20 min avoided thereafter) rather than any runtime tweak.
- **Emilelu/UniXNU-mondrian** and **MiLacy/UniXNU-marble** do **not** currently
  demonstrate clearing the bar; Emilelu is the best second source for the
  mechanism quote (section 1).
- **MC-XiaoXiao/ios_sagit** claims a Home Screen with "development Setup
  configuration" on iOS 18 but its datamigrator patch is marked development-only
  and the build manifest reports `hardware_tested: False`; treat as a **claim**.
- **georgev-6974/s9plus-ios**: no home screen claimed ("nothing runs yet").

Bottom line of the record: **every project that reached the UI either (i) seeds
the success metadata so the landlord `isMigrationNeeded` returns false, or (ii)
denies the daemon entirely, or (iii) lets the migration run once via the sentinel
for appex registration.** No public port reached the Home Screen by *porting or
implementing* `com.apple.datamigrator`.

---

## 3. Apple-side minimum set for "migration not needed" on a fresh iOS 16 data volume

Label key: (V) verified in a source linked above, (I) inference/unverified,
(C) community-internal convention cited by ports but not Apple-documented.

- Preference domain actually read:
  - `com.apple.migration`, key `LastSystemVersion`, via
    `CFPreferencesCopyValue(..., kCFPreferencesCurrentUser, kCFPreferencesCurrentHost)` —
    i.e. per-host. SWEET donor's `DMEnvironment lastBuildVersionPref` (verified in
    donor NOTES). **V in our donor log, (C) for the exact domain path in public iOS.**
  - Ports seed this into the **plain** file `/var/mobile/Library/Preferences/com.apple.migration.plist`
    (HakiOS, Danstronics, Emilelu, FoXLinne, xnu-xzs, topaz, sailfish all do this)
    **V**. SWEET found the phone's CurrentHost redirection *landed* in that plain
    file (so the plain file became the gate). **V in our donor log.**
  - `DMUserDataDisposition` (`SyntheticDidUpgrade`, `buildVersion`) also lives in
    `com.apple.migration` — SWEET donor found it in the plain file. **V.**
  - `DMLastMigrationResults` (`success`, `buildVersion`) is a **community
    convention** for "migration already succeeded" used by HakiOS/Danstronics/Emilelu.
    I did not find an Apple source for it; treat as **(C)** / "worked in ports".
- `/chosen/unique-chip-id` (8 bytes) + `/chosen/chip-id` (4 bytes) on arm XNU
  produce `IOPlatformUUID`; `CFPreferencesCurrentHost` filenames use it. Convention:
  `Preferences/ByHost/com.apple.migration.<UUID>.plist`. SWEET published
  `A9683613-3704-53F2-95D0-98A8FB574E78`. **V** (SWEET ADT), **V** (Emilelu doc).
  Reality in ports: seeding the plain `com.apple.migration.plist` is sufficient on at
  least SWEET v12, HakiOS, Danstronics, FoXLinne; the ByHost file was a false start
  on our image. (I) The port world therefore treats UUID as prerequisite for
  CurrentHost resolution but not as the migration gate itself.
- Sentinel `/private/var/datamigrator/migrate`: presence is logged by datamigrator
  as "Migration sentinel present. Migration necessary." SWEET donor verified it is
  created/removed by datamigrator itself (`logs` + `NOTES.md`). Sailfish
  **deliberately creates it** to force one migration (for appex registration). **V.**
  When absent, migration may still be triggered by the build-version mismatch.
- `/private/var/db` / `/var/mobile/Library/...` markers:
  - mobileactivationd data ark
    `/private/var/containers/Data/System/<UUID>/Library/internal/data_ark.plist`
    with `-ActivationState`, `-BuildVersion`, `-DataMigrationCompleted`,
    `-PreSydroDataMigrationCompleted` — seeded by SWEET, topaz, sailfish. **V.**
  - "Sydro": only seen as a key name (`PreSydroDataMigrationCompleted`,
    `-PreSydroDataMigrationCompleted`) in the iOS 16 activation data ark and in
    SWEET's notes; no public documentation/expansion found. **(I)**
    Likely an Apple-internal stage marker inside mobileactivationd's activation
    ark (an internal build name per HakiOS-style naming). Which iOS versions use it
    is **not verified**; the only version seen is iOS 16.7 20H392. I did not find
    any source stating its meaning; honest empty result on "Sydro" definition.
- `DMPerformMigrationIfNeeded` (backboardd CoreFoundation call) and
  `checkInAndWaitForDataMigration` / `_checkIn` (BKSSystemShellService): backboardd
  blocks SpringBoard's check-in until migration returns. Verified in SWEET donor +
  Emilelu quote. **V.**
- `migrationpluginwrapper` / plugin migrators: SWEET donor notes it ran on v10
  without stopping the bar; not surfaced as the gate. **V (existence), (I) on its
  causal weight.**
- `SyntheticDidUpgrade`: DMUserDataDisposition key, written by the donor as
  `False`. Effect beyond what SWEET noted is not documented publicly; **(C)**.

---

## 4. Stalls that masquerade as a 100%-bar hang (per port record)

For each: does it block the bar or only a later daemon?

- **SpringBoard `-[BKSSystemShellService _checkIn]`**: waits for backboardd, which
  waits for `DMPerformMigrationIfNeeded`. It is the *symptom* that SpringBoard is
  held, not an independent root cause. Blocks SpringBoard UI, not kernel boot.
  **V** (SWEET sampler, Emilelu).
- **purplebuddy / Setup** (`SetupDone`, `SetupFinishedAllSteps`, `SetupVersion`,
  mini-buddy flags, `MCForceShowCloudConfigurationUI`): most ports seed these to
  skip Setup. On SWEET the Setup scene was killed by the scene-create watchdog
  because it blocked in CoreTelephony -> CommCenter; CommCenter job removal fixed
  it. Blocks later UI, not the 100% bar itself. **V** (SWEET, topaz script).
- **mobileactivationd / activation**: data ark + Hactivation branch, gated by ADT
  cert-production status etc. Ports seed `Activated` and rely on native Hactivation.
  Crash loop on our donor (~100/boot) did not gate the bar. **V.**
- **mediaserverd / installcoordinationd / lockdownd crash loops**: installcoordinationd
  failed on ACL-only system-group containers because `config_ext_resolver` asked
  for a resolver not running; SWEET patch 0057 removed it. mediaserverd crashed in
  VirtualAudio HAL (moved out of HAL). These are later-daemon issues, addressed via
  `DENY` job lists in HakiOS/Danstronics and jailbreak-style tweaks. Blocks audio,
  not the bar. **V** (SWEET), **V** (HakiOS/Danstronics `DENY` lists).
- **AKS selectors**: 34 (datamigrator), 35 (extended device state), 93, 101. SWEET
  hardened 35/101; georgev notes 34 unsupported. These are minor-daemon obstacles,
  not the bar gate. **V/C.**
- **HFS atomic-swap `ENOTSUP`** (`RENAME_SWAP`/`exchangedata` for `.kb` keybag
  writes): explicit ENOTSUP when usermanagerd wrote keybags. SWEET's RENAME_SWAP
  fallback fixed it. Blocks keybag persistence/activation, not the bar. **V**
  (SWEET 0052), **V** (HakiOS `exchangedata` refs in code search).
- **`.migrated` markers for logd's stores** (and logd uuidtext empty): I found
  `.migrated` references in the HakiOS tree (code search hits without fetch
  due to rate limit) but could not retrieve the exact file/semantics; treat as
  **not verified here**.
- **"volume not cleanly unmounted" / `fsck.hfsplus`**: we had to fsck before Linux
  would mount rw; operational, not a stall. **V** (SWEET notes).

---

## 5. Blunt verdict

- **Did any public port hit the same DataMigration stall?** Yes — the whole SWEET
  family (dipa-studio, Floppikrob, ports derived from it), georgev's star2lte
  progress bar (15+ min then ~1 min), Emilelu's first-frame stall notes, and the
  HakiOS family documents the same mechanism.
- **Single most reliable fix per the record**: seed `LastSystemVersion` (equal to
  `ProductBuildVersion`, e.g. `20H392`) into the **plain**
  `/var/mobile/Library/Preferences/com.apple.migration.plist`, accompanied by the
  ADT `/chosen/unique-chip-id` + `chip-id` so `IOPlatformUUID` can be published.
  For fully working UI (wallpapers/widgets), let the migration run once via the
  `/private/var/datamigrator/migrate` sentinel instead of skipping it
  (sailfish's finding); otherwise skip it and lose appex registration.
- **Did any port reach SpringBoard/Setup *without* migration work?** Yes:
  HakiOS/Danstronics (disable job + success plist, iOS 15.5),
  lechaukha12/xnu-xzs (same skip on iOS 16.7), and Droid-VM (pre-migrated Data
  seed). The sweet-family ports themselves "skip" via the same tweak set. The
  only port that deliberately spends the migration cost is sailfish
  (app-extension registration).
- **Which of our two known causes is the actual gate at 100%?**
  Per our donor record, **`LastSystemVersion` missing** in the file DM actually
  reads was the gate: ADT `unique-chip-id` + `chip-id` alone with a hand-written
  `ByHost` file did **not** stop migration (SWEET v11), and adding
  `LastSystemVersion` to the **plain** `com.apple.migration.plist` did (v12). The
  missing `IOPlatformUUID` was the originally hypothesized root cause and is
  required for CurrentHost preference routing (and therefore for ByHost filenames),
  but on our image CurrentHost lands in the plain file.
- **File written**: `/home/not_andrej/ios-jackpotlte/docs/research/data-migration-prior-art.md`.
- **Could not verify (honest empties / unverified)**:
  exact definition of "Sydro" (no public source found; only the key name
  `PreSydroDataMigrationCompleted` is visible); `.migrated` markers for logd's
  stores (code-search hits in HakiOS but file/semantics not fetched); whether
  topaz/sagit/georgev are truly hardware-verified past the bar (README/manifest
  disagreement for sagit); engineering-day counts for ports other than SWEET
  (the donor's migration issue was closed within roughly one working day, Oct 17,
  but exact person-hours are unmeasured); and any port beyond those listed
  matching these symbols.
