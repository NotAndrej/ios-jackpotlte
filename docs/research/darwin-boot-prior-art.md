# Darwin / iOS on non-Apple ARM hardware: prior-art map

Research compiled 2026-11-08. **Read-only research document; no device, git, or CI actions were taken.**

Confidence labels used throughout:

- **verified source** — observed in a public repo, commit, CI log, tagged release, or documented patch.
- **claim** — stated in an article/video/social post and not independently verified by us.
- **inference** — our reading of the public material; not asserted by the project.

### Verifier's note (2026-11-08)

Checked against the live sources before this note was used:

- **Confirmed by fetch:** `dipa-studio/ios-sweet` (SM7150, iOS 16.7.16, 6/8 cores, CPU rendering, Hactivation), `ry-diffusion/Bergamota` (Poco F1 SDM845, iOS 18.7.8, SpringBoard, GPU *in progress*), `darwin-on-arm/xnu` (OMAP3530 / ARMPBA8 / S5L89xx machine configs), `alephsecurity/xnu-qemu-arm64` (iOS 12.1, launchd, unsigned binaries, SSH, textual framebuffer, KVM), and `unixnu.com` plus the guacamole bring-up repo (r15 reaches iOS userland on the panel).
- **Corrected:** the guacamole / OnePlus 7 Pro evidence lives in `xiaomi-haydn-dev/UniXNU-haydn4`, which is the guacamole bring-up imported into a haydn-named repo; the Pad 6S Pro is a different repo (`UniXNU/UniXNU-sheng`).
- **Not verified:** the `54 of 456` and `45,697` AIR numbers do not appear in the HakiOS docs fetched here — treat them as project-reported. HakiOS's own `docs/metal.md` concludes the **Adreno/Redwood** port does *not* get Metal and uses its OpenGL ES renderer, so the "Adreno Metal 100–118 fps" line below must be checked against the separate `hakios-metal` (PM4) generation before it is quoted.
- **Self-reference:** §7 cites `JACKPOTLTE_PROGRESS.md` for *our* jt6–jt8 history, and §5 cites our own `docs/jackpotlte-mali.md` for the panfrost v7 gate. Both are our own claims, not independent prior art.

This note exists so the A8 2018 / Exynos 7885 bring-up can tell “solved elsewhere” from “ours” from “known dead end.”

## 1. Summary table

| Project | Hardware / SoC | XNU / iOS gen. | Component demonstrated | Evidence | Confidence |
|---|---|---|---|---|---|
| winocm `darwin-on-arm` | ARM RealView PB-A8 (Cortex-A8); TI OMAP3530/3730; plus Apple S5L SoCs | XNU ~10-era, ARMv7 (2013) | Mounts root; user-mode `write()` syscall; starts `launchd`, then dies after fork | Repo `darwin-on-arm/xnu`; blog (2013-09-29) | verified source |
| community `darwin-on-arm` plans | Raspberry Pi 2, BeagleBone Black, Nokia N900, 96Boards/HiKey | Same series | Hardware support discussed; largely untested | Repo issue #25 | claim / inference |
| iEmu (winocm) | QEMU/emulators for A4-class Apple SoCs | Early iOS | Virtualization background; contrast only | Cited by HN comment | claim |
| alephsecurity `xnu-qemu-arm64` | QEMU `aarch64-softmmu`; virtual Apple board | iOS 12.1 | QEMU boot to interactive bash; `launchd` services; unsigned binaries; SSH tunnel; framebuffer; KVM acceleration | Repo + blog | verified source |
| Corellium CHARM | Host ARM servers; virtual Apple devices | Modern iOS/iPadOS | “Digital twin” iOS device in VM; full userland, app testing, research tools | Vendor site / docs / courts | vendor claim (independent fidelity not verified here) |
| Project Sandcastle (reverse direction) | iPhone 7 / 7+ (Apple A10); checkm8/pogoOS | Android 10 on Apple hardware | Android boots on iPhone; no audio/BT/cellular/camera/GPU | Project site, status table | verified claim |
| ios-sweet | Redmi Note 10 Pro (Qualcomm SM7150 / Snapdragon 732G) | XNU 8796 (iOS 16.7.16 / iPhone X) | **Stage 7 GUI**: `launchd`, system daemons, `backboardd`, SpringBoard, lock/home/Control Center, touch, keys, battery, Hactivation; CPU render 20–30 fps | Public repo README, patches, photos | verified source |
| Bergamota | Poco F1 (Qualcomm SDM845 / Adreno 630) | XNU 11215 (“Heartwood”, iOS 18.7.8 / iPhone XR) | SpringBoard + home screen, stock apps, USB Ethernet/SSH, real battery, Renegade PAC-stripping; GPU still in progress | Public repo README | verified source |
| HakiOS / HackiOS | POCO X5 Pro 5G (Qualcomm SM7325 / Adreno 642L); Infinix Hot 20 (MediaTek MT6768 / Mali-G52) | XNU 8796.121.2 (iOS 16.5 / iPhone X) | End-to-end `launchd` → `backboardd` → SpringBoard → apps; QEMU + phone; CPU 15–40 fps; Adreno Metal window server 100–118 fps; audio, touch, Wi-Fi on SM7325 | Public repo, docs/notes, website | verified source |
| UniXNU (general UEFI layer) | Many ARM64 boards; reference Xiaomi Pad 6S Pro (SM8550 / Adreno 740) and OnePlus 7 Pro (SM8150) | Profiles incl. XNU 8796 and XNU 10063; iOS userland | UEFI boot chain + DarwinBoot handoff; on OnePlus 7 Pro r15 reaches iOS userland on panel; SoC GPU is a separate blocker | `unixnu.com`; `xiaomi-haydn-dev/UniXNU-haydn4` (guacamole bring-up); `UniXNU/UniXNU-sheng` (Pad 6S Pro) | verified source for handoff; device maturity varies |
| UniXNU Metal (on SM8550-class Adreno 740) | Adreno 740 | Darwin 23 profile | Metal on Vulkan: AdrenoMetal, `metal2vulkan`, Turnip, own kernel driver with ring scheduling | UniXNU component docs, hakios comparison | source/claim mixture |
| TrungNguyen1909 (claim) | Apple A13 Bionic on non-Apple board (reportedly) | “modern iOS” | Claim of first modern iOS on non-Apple hardware | HN comment only | claim (unverified) |

## 2. XNU on non-Apple ARM SoCs

### 2.1 winocm `darwin-on-arm` (true non-Apple and Apple mixed hardware)

- Repo: `https://github.com/darwin-on-arm/xnu`
- Blog: `http://winocm.com/xnu/projects/research/2013/09/29/current-status-of-xnu/`
- Extensions: issue #25 for hardware boards (`https://github.com/darwin-on-arm/xnu/issues/25`)

What was actually shown (2013):

- XNU port booted past IOKit initialization and could start a user-mode process.
- Validation of a simple binary calling `write()` through standard UNIX syscalls succeeded.
- `launchd` could be started and could execute `dyld`, but `launchd` died after forking.
- Supported platforms were mixtures:
  - ARM RealView Emulation Baseboard / Platform Baseboard with Cortex-A8 (non-Apple),
  - Texas Instruments OMAP3530/3730 (BeagleBoard xM) (non-Apple),
  - Apple S5L89xx SoCs (iPhone/iPod-era Apple silicon, even when used on Apple boards).
- Targets discussed later: Raspberry Pi 2, BeagleBone Black, Nokia N900. These appear in issue #25 as plans; verify each entry before treating it as “booted.”

Confidence: **verified source** for the early ARMv7 status; **claim/inference** for later board targets.

### 2.2 Distinction from Apple-hardware projects

- **Project Sandcastle** runs Android on **Apple A10** iPhones/iPods using checkm8. It is the reverse direction (Android on Apple silicon), not iOS on a non-Apple SoC. It is useful only as a contrast for what can be done with Apple firmware in the boot chain.
- **Asahi Linux** runs Linux on Apple Silicon; again, opposite direction and not iOS userland on an Android board. Not prior art for our bring-up.
- **Corellium** runs iOS on virtual (not physical) non-Apple hosts; see §5.

## 3. iOS / Darwin userland on Android and non-Apple boards

### 3.1 ios-sweet (dipa-studio) — direct ancestor of this tree

- Repo: `https://github.com/dipa-studio/ios-sweet` (also a documented `upstream-README.md` in this repo)
- Hardware: Xiaomi Redmi Note 10 Pro, Qualcomm SM7150 / Snapdragon 732G
- OS: Apple XNU 8796 / iOS 16.7.16 userland (`iPhone10,3` identity)

Demonstrated (as of 2026-10-18, photos in repo/notes):

- Boot: stock ABL → temporary `fastboot boot` shim → XNU; nothing flashed to boot.
- 6 of 8 cores (A55s) through PSCI; GICv3 interrupts; 16K pages.
- Read/write HFS+ root inside `userdata`.
- `launchd` plus system daemons; `backboardd`, SpringBoard, lock/home/Control Center/notification center, Settings.
- Kernel IOSurface + IOMobileFramebuffer (`AppleCLCD`) services; CoreAnimation software render copied to ABL framebuffer; ~20–30 fps.
- Touch (Goodix GT9896), hardware keys, static battery service, device identity as iPhone X.
- “Hactivation”: native `mobileactivationd` path chosen through device-tree security status; no Apple servers.
- Daemons/bugs observed: daemons for unavailable hardware start and fail/relaunch; media path disabled; watchdogs disabled; AMFI and sandbox permissive; stub AppleKeyStore (no data protection).
- Specific failures: `mediaserverd` SIGSEGV loop in `VirtualAudio.plugin`; `mobileactivationd` relaunch loop; installcoordination/bluetooth/fairplay loops for missing hardware.

Confidence: **verified source** (repo + patch list + persistent-log notes in `NOTES.md`).

### 3.2 Bergamota — same idea on Snapdragon 845 and iOS 18

- Repo: `https://github.com/ry-diffusion/Bergamota`
- Hardware: Xiaomi Poco F1, Qualcomm SDM845 (Adreno 630, A55+A75, no PAC)
- OS: XNU 11215 (“Heartwood”) plus patches; iOS 18.7.8 userland (`iPhone11,8`)

Demonstrated:

- Boot chain: Qualcomm ABL → Enxerto bootloader → XNU → HFS+ root → `dyld` → `launchd` → daemons → `backboardd` → SpringBoard.
- Reaches SpringBoard and home screen; stock apps (Safari, Settings) registered.
- Renegade rewrites arm64e Mach-O and the dyld shared cache into plain arm64 (PAC stripping), plus ad-hoc re-signing.
- All 8 cores; 1080×2246 @3x panel; CPU rendering.
- Touch (Novatek NT36672A bit-banged I²C), power/volume keys, real fuel gauge/charger, off-mode charging in Enxerto.
- USB CDC-ECM Ethernet: SSH as root, live kernel log, Mac internet sharing.
- Not yet: Wi-Fi, cellular, Bluetooth, audio, camera, sensors, SEP/Face ID, GPU acceleration.
- An X/Twitter post claims “GPU compositing on Adreno 630, home screen swipes ~45 fps,” but the repo status table still says CPU rendering with GPU acceleration in progress. Treat the GPU claim as **unverified promotional claim**; the README is the stronger source.

Confidence: **verified source** for SpringBoard/CPU; **claim** for GPU.

### 3.3 HakiOS / HackiOS — Snapdragon 778G + MediaTek MT6768

- Repo: `https://github.com/oosexgang/hakios-xnu-kernel`
- Device repos: `hakios-sm7325`, `hakios-mt6768`
- Website/docs: `https://hackios-web.vercel.app/en`
- Comparison page: `docs/comparison.md`

What HakiOS explicitly documents as working (POCO X5 Pro 5G, Snapdragon 778G):

- UEFI/fastboot-boot path; RAM only; reboot returns Android.
- Unmodified iOS 16.5 (Darwin 16.5) userland on Qualcomm SC7280 family.
- `launchd` → `backboardd` → SpringBoard → apps; Settings, Clock, Calendar, Photos, Notes, app extensions/widgets.
- Multi-touch, gestures, power/volume buttons, fast big-core boot (~20s).
- Safari browsing over device Wi-Fi; screenshots/screen recording.
- Audio output and input through custom kernel audio path; camera in Camera.app; battery via Awinic audio path/charger? (documented in notes).
- `mediaserverd`-backed audio; Keychain in “unlocked-class” form.
- Software CoreAnimation render 15–40 fps.
- Metal-on-Adreno window server at ~100–118 fps page swipes.
- Metal app support is only partial: the frontend translates Apple AIR shaders; 54 of 456 probed AIR kernels ran on the Adreno (7 draw + 47 compute). 71 `air.*` ops matched LLVM across 45,697 cases; sampler state not decoded. *(These counts are project-reported; they were not found in the HakiOS docs fetched for verification.)*
- OpenGL ES draw path through Mesa/Freedreno is the default app render path.
- App Store UI renders, but `fairplayd.H2` dies immediately (exit 0) in a `KeepAlive` loop (26 times/run), so content tabs never load.
- Apple ID/iCloud fails due to absence of Secure Enclave / device-key generation (`mobileactivationd` cannot make the UIK).
- App data persistence through a `darwin`/`var_store` mechanism on SM7325.

MediaTek port (Infinix Hot 20, MT6768, Mali-G52) is also documented as HakiOS’s second target, using the same userland and Metal/plug-in machinery. This is the closest published comparator for a Mali GPU on Darwin, but it is Mali-G52 (Bifrost v7), not our G71 (v6).

Confidence: **verified source** for documented behavior; hardware measurements are project-reported.

### 3.4 UniXNU — UEFI generalist, not one device port

- Site: `https://unixnu.com`
- Device repo example: `https://github.com/xiaomi-haydn-dev/UniXNU-haydn4` — despite the haydn name, its README is the **guacamole (OnePlus 7 Pro, SM8150)** bring-up, imported from guacamole. The Pad 6S Pro (SM8550) is a separate repo, `UniXNU/UniXNU-sheng`.

What is demonstrated:

- A complete UEFI layer for non-Apple ARM64: BootShim → Mu-Silicium UEFI → UniXNU boot manager → DarwinBoot loader → patched XNU.
- `fastboot boot` of a temporary image; nothing written to device.
- DarwinBoot validates Mach-O, ADT, trust cache; builds `boot_args`; exits boot services; jumps into XNU EL1.
- 234 device entries in `devices.toml`, many third-party; maturity varies by device.
- On OnePlus 7 Pro, r15 is hardware-verified to reach “iOS userland on the panel.” Storage driver then publishes `OS` and `SystemOS`; XNU mounts by UUID and starts `launchd`.
- GPU on SM8550-class (Adreno 740) is advertised as Metal-on-Vulkan with a first-party kernel driver, but performance and SoC support are device-specific.

Confidence: **verified source** for the boot chain and OnePlus 7 Pro stage; **claim/mixture** for Metal numbers and device maturity.

## 4. Daemons / services shown working

Published, explicit mentions:

- `launchd`: started in all three phone ports; HakiOS and ios-sweet document it as the gateway to userspace.
- `backboardd`: shown in ios-sweet, Bergamota, HakiOS; SpringBoard depends on it for display/event plumbing.
- SpringBoard: home screen shown in ios-sweet, Bergamota, HakiOS, UniXNU r15.
- CoreAnimation software rendering: explicitly demonstrated; HakiOS baseline CPU renderer.
- `mediaserverd`: started in ios-sweet; crashes in VirtualAudio; HakiOS documents audio daemon/services (`mediaserverd`, audio HAL hooks) and measures throughput.
- `mobileactivationd`: Hactivation path works in ios-sweet without Apple servers; HakiOS documents its failure to create device key / Apple ID.
- `cfprefsd`: not specifically benchmarked in public notes; assumed running because userland boots, but we have no published per-daemon proof.
- `apsd`, `akd`, `configd`, `fairplayd`, `wifid`, `appstored`: detailed analysis in HakiOS notes; many App-Store-dependent services fail or loop.
- Touch/HID: `backboardd` receives HID digitizer events in ios-sweet and HakiOS; hardware button mapping documented.

## 5. Graphics: Metal / OpenGL ES on non-Apple GPUs

- **Mesa panfrost on Mali**: Panfrost supports Mali G31/G51/G52/G71/G72/G76 etc., including G71 (Bifrost v6) and G52 (v7). But the HakiOS Darwin port’s Meson gate compiles **only Bifrost v7** for Darwin (`panfrost_versions = ... ? ['7'] : ...`) and `pan_screen.c` returns NULL unless `dev->arch == 7`. Therefore **G71 on Darwin is an open porting task, not demonstrated prior art.** (inference from scoping doc + Mesa docs)
- **HakiOS Adreno 642L (POCO X5 Pro)**: OpenGL ES apps through Freedreno, Metal window server through RedwoodMetal plug-in, direct Metal frontend reads AIR and dispatches to Adreno; measured ~100–118 fps page swipes. Still only 54/456 probed AIR kernels. *(Conflict to resolve: HakiOS's `docs/metal.md` for the Redwood/Adreno port concludes Metal is not feasible there and CoreAnimation uses its OpenGL ES renderer; the 100–118 fps figure is project-reported and may belong to the `hakios-metal` PM4 driver or to the Mali path. Do not quote this line as settled.)*
- **UniXNU SM8550-class (Adreno 740)**: Metal on Vulkan via AdrenoMetal/metal2vulkan/Turnip with a first-party kernel driver (ring scheduling/preemption claimed). HakiOS comparison notes their numbers do not imply HakiOS perf. (source/claim mixture)
- **Bergamota Adreno 630**: README status says CPU rendering only and GPU acceleration is in progress; an X post says GPU compositing at ~45 fps. Treat as **in progress / claim**.
- **Asahi (M1/M2)**: Linux graphics stack for Apple GPUs, not iOS on non-Apple. Relevant only for Mesa/driver porting technique.
- **Metal apps**: on the non-Apple ports, Metal at app level is not fully functional; software-rendered CoreAnimation is the working baseline. `camtl=1`, QuartzCore shaders, shader translation, and no SEP all remain issues.

## 6. Virtualization and emulation routes (and what they prove)

- **Corellium CHARM**: ARM-on-ARM type-1 hypervisor presenting virtual Apple devices. Proves full iOS can run in a VM with native-fidelity ARM behavior, used for security research. It does **not** prove iOS on a non-Apple physical SoC, nor does it provide a reproducible open port. Confidence: vendor + court record.
- **alephsecurity `xnu-qemu-arm64`**: Proves iOS 12.1 can run under QEMU (including KVM acceleration) to interactive bash, launchd services, unsigned binaries, SSH, a textual framebuffer. Proves user-mode behavior under virtualized Apple hardware details; no real GPU/XNU-on-non-Apple-hardware claim. Confidence: verified source.
- **HakiOS QEMU (`virt`, aarch64)**: same ported XNU 8796 userland runs in QEMU on any Linux PC without a phone. Useful for kernel and userland bring-up, not evidence of particular phone SoCs.
- **Our local QEMU**: `tools/qemu-sweet.py` does the same for ios-sweet-derived trees. (Visible in repo.)
- Reverse direction contrast: QEMU/hackintosh on Apple hardware is often mistaken in headlines for “iOS on Android,” but it is not relevant to this board.

## 7. Published failure modes: entitlements, AMFI, signing, platform mismatch, activation

- **Wrong platform (critical):** Open XNU built as `XNU_TARGET_OS_OSX` rejects spawns of iOS-platform binaries unless the spawn carries the correct posix_spawn platform attribute; the kernel returns `OS_REASON_EXEC` / `EXEC_EXIT_REASON_WRONG_PLATFORM` (code 14). The standard workaround on these ports is `unentitled_ios_sim_launch=1` plus `cs_enforcement_disable=1`; ios-sweet also requires `launchd_unsecure_cache=1` so `launchd_cache_loader` can load its job cache without AMFI. This repo’s own A8 path hit exactly this: `jt6` failed at stage 6 with WRONG_PLATFORM; `jt7` cleared it and hit AMFI next; `jt8` added `launchd_unsecure_cache=1 ignition_level=1`. Source: Google Project Zero on XNU/macOS behavior; repo `JACKPOTLTE_PROGRESS.md`.
- **AMFI / entitlements:** Every port runs a permissive or stubbed security model. ios-sweet documents “Apple’s security model is mostly switched off” and `sweet-root-tweaks.py` removes enforcement. Apps that expect proper entitlement resolution (Store, FairPlay, tccd, appservices) fail.
- **Sandbox:** Missing kext/profiles cause `launchd` Sandbox restrictions and tccd “profile not found”; HakiOS/other notes show many Mach-lookup failures (`com.apple.fairplayd`, kvsd, location).
- **Activation:** Real activation needs Secure Enclave / device-key generation. HakiOS and ios-sweet both bypass this with a custom Hactivation path; Apple ID/iCloud still unavailable. HakiOS traces `mobileactivationd` inability to create the UIK (`0xe00002c7` unsupported).
- **FairPlay:** HakiOS shows App Store content stays empty because `fairplayd.H2` exits with 0 in a `KeepAlive` loop; likely wants a FairPlay IOKit user client (`com_apple_driver_FairPlayIOKitUserClient`) that the ports do not provide.
- **Data protection / keybag:** ios-sweet uses a stub AppleKeyStore; no SEP means no keybag-bound protection.
- **Platform identity:** Ports synthesize serials, ECID, MLB, and iPhone model identity (`iPhone10,3`, etc.) because XNU/device tree needs them for MobileGestalt; several HakiOS checks (`HasSEP`, etc.) return nil.
- **No hardware daemons:** For missing Wi-Fi/BT/audio/camera, daemons start and crash/relaunch; this is accepted bring-up behavior, not a crash-prevention success by itself.

## 8. What this means for Exynos 7885 / Mali-G71 (our bring-up)

Layers already demonstrated prior art (adapt, do not reinvent):

- XNU on non-Apple ARM64 SoCs and Android boot image chains (ios-sweet, Bergamota, HakiOS, UniXNU).
- iOS 16.5 unmodified userland from launchd to SpringBoard on non-Apple SoCs with CPU-only rendering (ios-sweet, HakiOS).
- Hactivation / activation bypass through device tree security status (ios-sweet).
- `unentitled_ios_sim_launch=1`, `cs_enforcement_disable=1`, `launchd_unsecure_cache=1` to clear platform and AMFI blockers (this repo confirms on A8).
- Metal-on-non-Apple-GPU architecture for the Qualcomm Adreno path: IOKit accelerator service, plug-in bundle, DRM shim, AIR frontend, Mesa user path (HakiOS). Transferable patterns, not transferable code unless we adopt that stack.
- UEFI/DarwinBoot handoff validation and staging for Android boot images (UniXNU). We currently use a simpler shim + SWEET boot path; UniXNU may become useful if we generalize.

Novel / device-specific for A8 2018 (not yet demonstrated):

- Exynos 7885 platform expert / ADT, eMMC root and partition publishing, display/DECON present path (we have A8-specific milestones, but kernel code is our own).
- Samsung Mali-G71 (Bifrost v6) on Darwin through an IOKit user client. Closest published is HakiOS on Mali-G52 (v7), which proves the DRM shim + Metal plug-in plumbing but not v6.
- Exynos power/clock/IRQ sequence for G3D (documented in our mali scoping; still only a register-level plan).
- ARMv8.2 Exynos CPU SMP and secondary core transitions in our shim path.

Known dead ends / likely ongoing gaps (do not spend bring-up pretending they are solved):

- No Secure Enclave → no real device key, no activation, no push tokens, no Apple ID/iCloud, no App Store content.
- No FairPlay IOKit client → App Store content does not load even if UI renders.
- Permissive code signing/sandbox must remain during bring-up; turning them on crashes or halts launch.
- Many hardware daemons (Wi-Fi/BT/cellular/audio/camera/sensors/SIM) remain relaunch/crash loops; porting each is separate.
- GPU performance via CPU memcpy present path is a bottleneck until a present/feed path exists; HakiOS/UniXNU Adreno numbers are not transferable to G71.

Bottom line: prior art has already crossed **XNU boot on non-Apple ARM64**, **iOS 16.5 to GUI on two non-Apple SoC families**, **permissive launchd/AMFI handling**, and **partial Metal on Adreno**. The genuinely novel parts for our board are the **Exynos 7885 platform/eMMC integration** and **Mali-G71 (Bifrost v6) on Darwin**; everything in the security/activation stack remains bounded by hardware we do not have.