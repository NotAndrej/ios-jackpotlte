# Mali userland: where the reference's licence evidence is, and where it is not

The GPU plan (`docs/jackpotlte-mali.md` §1, §7) reuses the HakiOS reference's
Metal stack. What that stack's licence actually says was open; this file records
what each repository and each file says about itself, at a fixed commit, so the
decision rests on the evidence instead of on an assumption.

Checked 2026-11-11 over the public GitHub API and the raw files at the pinned
commits (`docs/jackpotlte-mali.md` §1). This is not legal advice and it does not
claim any particular use is permitted; it only reports what is written down.

## 1. The four sources

| Source | Commit | What the plan takes from it |
|---|---|---|
| `oosexgang/hakios-xnu-kernel`, branch `mtk` | `620c775` | `userland/src/redwoodmetal.m`, `userland/src/metal/*.cpp` (23 files), `tools/mesa/air/*` (17 files), `tools/mesa/*.patch`, `tools/mesa/redwood-drm/redwood_drm.c` |
| `oosexgang/hakios-mt6768` | `84b0252` | `kernel/bsd/dev/redwood_mali.c`, `mtkloader/mtkmali.h` — the SoC-side driver our `0152`/`0153`/`0154` already cite |
| `oosexgang/hakios-metal` | `d36f329` | nothing: despite the name this is the **Adreno** (`a6xx.hpp`) driver, not the Mali one |
| Mesa | `25.0.7` upstream | panfrost, which stays upstream and is not imported |

## 2. What the evidence shows

**The MTK snapshot carries no licence at all.** Branch `mtk` @ `620c775` is a
single commit — "HakiOS on MediaTek -- Infinix Hot 20 (X6826B), Helio G85, a
clean snapshot", 2026-11-06 — and its root holds no `LICENSE`, `NOTICE`,
`LEGAL` or `THIRD_PARTY_LICENSES` file. Every Mali file the plan names lives in
that snapshot. There is no earlier history in it to fall back on either: one
commit, everything at once.

**The repository's licence appeared later, on `main`, and does not name the
Mali userland.** `main` has `LICENSE` (APSL 2.0 for `xnu/hakios.patch`,
`xnu/files/` and `drivers/`; MIT for "the boot loader in `xnuloader/`, the
user-space code in `userland/src/`, the tools, the scripts, the documentation,
the device layer under `devices/`"), plus `LICENSE.APSL`, `LEGAL.md`,
`COMPLIANCE.md`, `NOTICE` and `THIRD_PARTY_LICENSES.md`. But `main` no longer
contains the Mali userland: the only path in the whole tree that matches
`redwood` is `docs/devices/redwood.md`.

**The AIR frontend's grant is inside its own files.** 16 of the 17 files in
`tools/mesa/air/` @`620c775` (1,365,675 B) open with
`SPDX-License-Identifier: MIT` and `Copyright © 2026 Darwin on redwood
contributors`; `air_metal.h`, the header of that same library, is the one file
without either. `main`'s
`THIRD_PARTY_LICENSES.md` lists `tools/mesa/air/` as "MIT | our own". This part
therefore does not depend on the snapshot's missing `LICENSE`.

**The Metal driver body does not.** All 23 files of `userland/src/metal/`
(216,576 B) @`620c775`, and `userland/src/redwoodmetal.m` (250,224 B, 5,938
lines), have no `SPDX` line, no copyright line and no mention of a licence.
`rwm.h`'s own header says only that the shape is `hakios-metal`'s. The one
statement that grants them is `main`'s directory-wide "user-space code in
`userland/src/` is MIT" — and `main` does not ship them.

**Upstream itself flags the plug-in as unsettled.** `main`'s
`THIRD_PARTY_LICENSES.md` and `COMPLIANCE.md` list the window-server Metal
plug-in `userland/src/HakiosMetalCA.m` (the successor of that file) as
**to verify**: it "began from vcvkk's" plug-in, whose repository "states no
license (read 2026-11-04)", so "the permission to carry and change this file is
not established". That is the reference's own note about its own plug-in, not an
inference of ours.

**The SoC-specific half is clean, and is a different repository.** The
`mtk`-branch copy of the Mali driver sits under `xnu/files/`, i.e. inside the
snapshot with no licence. The `hakios-mt6768` copy is the same driver in a
repository that does state its terms: `NOTICE` says `kernel/` is APSL 2.0
(because it is built into XNU) and everything else is MIT, and that "third-party
code: none is copied into this repository" — the MediaTek GPL-2.0 sources behind
it were read, not copied. `kernel/bsd/dev/redwood_mali.c` is 39,241 B there
against 42,131 B in the snapshot.

## 3. What matches what we already do

`xnu/patches/` is already declared APSL 2.0 in `README.md` ("derived from XNU"),
and `0152`, `0153` and `0154` already cite `redwood_mali.c` line ranges. Adapting
the `hakios-mt6768` driver into those patches therefore keeps one licence across
the kernel half — APSL 2.0 for a derivative of XNU — with no new obligation.

`tools/mesa/air/*` is MIT in the files that carry it. The Mesa patches
(`panfrost-darwin.patch`, `air-frontend.patch`, `air-panfrost.patch`,
`panfrost-osmesa-air.patch`, `panfrost-redwood-perf.patch`, ...) change MIT
Mesa and `main` lists `tools/mesa/*.patch` as MIT; they are diffs we would
re-derive anyway, because we keep Mesa upstream and write our own DRM face
(`userspace/libjackpotlte/jackpotlte_drm.h`, M4 step 2).

So the kernel half, the AIR frontend and the Mesa changes are all accounted for.

## 4. The one gap, and its size

What is left is the Metal side proper: `userland/src/redwoodmetal.m` (the
`_MTLDevice` subclass) plus the 23 files of `userland/src/metal/` that turn Metal
commands into panfrost jobs.

Two facts bear on how big a problem that is:

- The device class Metal requires is **small**. `redwoodmetal.m`'s own header
  records the measurement: `MTLAddDevice` wants an object that returns non-nil
  from `-initWithAcceleratorPort:`, conforms to `MTLDevice` and `MTLDeviceSPI`,
  and answers `-featureProfile`, `-maxBufferLength` and
  `-supportsVertexAmplificationCount:` — four methods; "everything else in the
  registration path, including the 64-field limits table and the 42 feature
  queries, `_MTLDevice` fills in itself once `-featureProfile` answers". A
  self-written plug-in can be that, and nothing more, and still make Metal
  register a device.
- The **driver body is not small**. 216 KB of C++ across 23 files is the actual
  Metal-to-panfrost translation, and it is the part that draws a triangle.

## 5. Options

1. **Take the AIR frontend and the SoC-side driver; write our own Metal side.**
   `tools/mesa/air/*` (per-file MIT) and `hakios-mt6768/kernel/` (APSL 2.0, as
   our XNU patches already are) come in as they are; `redwoodmetal.m` and
   `userland/src/metal/` do not, and we write the `_MTLDevice` subclass (small,
   per §4) plus our own Metal-to-`JackpotlteMali` translation.
   *Cost:* the 23-file body is real work, and it is the part with the least
   documentation to copy from — the reference's `docs/metal.md` describes it, but
   the code is where the behaviour is. *Risk:* none in licence; the GPU work gets
   longer.
2. **Take everything, with the gap recorded.** Import `redwoodmetal.m` and
   `userland/src/metal/` under `main`'s `userland/src/` MIT claim, and put the
   provenance in the file headers (origin repository, commit, "MIT per the
   repository's LICENSE on `main`; the pinned snapshot carries no licence file").
   *Cost:* nothing today. *Risk:* if the claim does not hold — and the
   reference's own `COMPLIANCE.md` says of the sibling plug-in that the
   permission "is not established" — the offending files are the Metal driver we
   would then have built the wallpaper work on, and they would have to be
   rewritten exactly as in option 1, later and under pressure.
3. **Take the frontend and the driver, write the plug-in object only.** A middle
   path: the device class (the 4-method part) is ours, so the *entry point* is
   ours, and the driver body is imported as in option 2. This does not change
   option 2's risk, it only moves where the unverified code sits.

Everything here is a decision for the owner. The open work list
(`TODO.md`, "GPU") carries it as **option 1**: the unverified piece is precisely
the piece the performance work depends on, and the reference does not claim it.

## Sources

- `api.github.com/repos/oosexgang/hakios-xnu-kernel` (licence `NOASSERTION`,
  public, default branch `main`), its `git/trees/620c775` and
  `git/trees/main?recursive=1`, and the raw files `LICENSE`, `LEGAL.md`,
  `NOTICE`, `THIRD_PARTY_LICENSES.md`, `COMPLIANCE.md` on `main`.
- Raw files at `620c775`: `userland/src/redwoodmetal.m`,
  `userland/src/metal/*`, `tools/mesa/air/*`, `tools/mesa/*.patch`,
  `tools/mesa/redwood-drm/redwood_drm.c`.
- `api.github.com/repos/oosexgang/hakios-mt6768` (licence `MIT`, public), its
  `git/trees/84b0252` and the raw `LICENSE` and `NOTICE`.
- `api.github.com/repos/oosexgang/hakios-metal` (licence `MIT`, public) and its
  description ("Moved into the kernel: ... hakios-metal/").
