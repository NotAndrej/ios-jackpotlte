# Mali userland: where the reference's licence evidence is, and where it is not

The GPU plan (`docs/jackpotlte-mali.md` §1, §7) reuses the HakiOS reference's
Metal stack. What that stack's licence actually says was open; this file records
what each repository and each file says about itself, at the pinned commits, so
the decision rests on the evidence instead of on an assumption.

Checked 2026-11-11 and re-checked 2026-11-13 over the public GitHub API and the
raw files at the pinned commits (`docs/jackpotlte-mali.md` §1). This is not
legal advice and it does not claim any particular use is permitted; it only
reports what is written down.

## 1. The four sources

| Source | Commit | What the plan takes from it |
|---|---|---|
| `oosexgang/hakios-xnu-kernel`, branch `mtk` | `620c775` (2nd of 7; root `3f31f34285` 2026-11-06, HEAD `a962798418fa` 2026-11-10) | `userland/src/redwoodmetal.m`, `userland/src/metal/*.cpp` (23 files), `tools/mesa/air/*` (17 files), `tools/mesa/*.patch`, `tools/mesa/redwood-drm/redwood_drm.c` |
| `oosexgang/hakios-mt6768` | `84b0252` | `kernel/bsd/dev/redwood_mali.c`, `mtkloader/mtkmali.h` — the SoC-side driver our `0152`/`0153`/`0154` already cite |
| `oosexgang/hakios-metal` | `d36f329` | nothing: despite the name this is the **Adreno** (`a6xx.hpp`) driver, not the Mali one |
| Mesa | `25.0.7` upstream | panfrost, which stays upstream and is not imported |

## 2. What the evidence shows

**The MTK branch carries no licence at all.** Branch `mtk` is a line of its own
history: its root commit `3f31f34285` ("HakiOS on MediaTek -- Infinix Hot 20
(X6826B), Helio G85, a clean snapshot", 2026-11-06) has no parent, and by
2026-11-10 the branch is 7 commits, HEAD `a962798418fa`. `620c775` is the second
of them. No commit on the branch holds a `LICENSE`, `NOTICE`, `LEGAL`,
`COPYING` or `THIRD_PARTY_LICENSES` file — not the root, not `620c775`, not
HEAD. Every Mali file the plan names lives in that snapshot. And the branch
shares no history with `main` (`compare` answers "No common ancestor between
main and mtk"), so `main`'s later licence cannot reach it through a merge
either: the two are independent snapshots.

**The repository's licence appeared later, on `main`, and does not name the
Mali userland.** `main` has `LICENSE` (added in `ea3fadaabc`, 2026-10-25; APSL 2.0 for `xnu/hakios.patch`,
`xnu/files/` and `drivers/`; MIT for "the boot loader in `xnuloader/`, the
user-space code in `userland/src/`, the tools, the scripts, the documentation,
the device layer under `devices/`"), plus `LICENSE.APSL`, `LEGAL.md`,
`COMPLIANCE.md`, `NOTICE` and `THIRD_PARTY_LICENSES.md`. But `main` no longer
contains the Mali userland: the only path in the whole tree that matches
`redwood` is `docs/devices/redwood.md`.

**The AIR frontend's grant is inside its own files, but its attribution is not
settled.** 16 of the 17 files in `tools/mesa/air/` @`620c775` (1,365,675 B, and
byte-identical at HEAD) open with `SPDX-License-Identifier: MIT` and `Copyright
© 2026 Darwin on redwood contributors`; `air_metal.h`, the header of that same
library, is the one file without either. `main`'s `THIRD_PARTY_LICENSES.md`
lists `tools/mesa/air/` as "MIT | our own". But `main` commit `09ebcf8a93`
(2026-11-01) says the opposite of "our own": "tools/mesa: vcvkk's AIR front end
and two ir3 fixes ... From vcvkk/darwin-redwood, branch hakios-air-frontend
(7cd38fb)". The same repository calls one directory both. The MIT grant travels
in the files; what is unresolved is *whose* files they are, and vcvkk's copy
cannot be checked (see above).

**The Metal driver body does not.** All 23 files of `userland/src/metal/` (21
`.cpp` plus `drawable.h` and `rwm.h`; 216,576 B, 5,822 lines) and
`userland/src/redwoodmetal.m` (250,224 B, 5,938 lines — that figure is that one
file's, not the directory's) @`620c775`, unchanged at HEAD, have no `SPDX` line,
no copyright line and no mention of a licence. `rwm.h`'s own header says only
that the shape is `hakios-metal`'s — and `hakios-metal` is the MIT *Adreno*
engine, so the Mali driver is modelled on HakiOS's own earlier work, not on any
third-party Mali project. The one statement that grants these files is the `mtk`
`README.md`'s "everything else is under MIT, as the file headers say" — a
sentence these files' headers do not bear out, on a branch with no `LICENSE`
file at all.

**Upstream itself flags the plug-in as unsettled, and that file ships an MIT
header anyway.** `main`'s `THIRD_PARTY_LICENSES.md` and `COMPLIANCE.md` list the
window-server Metal plug-in `userland/src/HakiosMetalCA.m` as **to verify**: it
"began from vcvkk's" plug-in, whose repository "states no license (read
2026-11-04)", so "the permission to carry and change this file is not
established". The file itself, however, carries `SPDX-License-Identifier: MIT`
in its header — the project's own words and the file's own header disagree.
(`HakiosMetalCA.m` is a `main`-only file, not the `mtk` Mali plug-in we were
looking at; the reference names it as that plug-in's successor.) That is the
reference's own note about its own file, not an inference of ours.

**The repository the reference points to cannot be checked.** `vcvkk/darwin-redwood`
does not exist today: the API and the web page both answer 404 (a rename would
redirect, so it was not renamed), commit `eb9fdf5` is unverifiable, no repository
of that name is on GitHub, and there is no Wayback snapshot. HakiOS's own record
of 2026-11-04 — "states no license" — is therefore the only evidence of its
terms, and cannot be confirmed by anyone now. *(This is why option 1 below does
not depend on it: option 1 takes only what has a grant we can read.)*

**No licence question has been raised in the reference's own trackers.** All
states of issues and pull requests across the three repositories were searched
for licence/reuse/attribution terms — 61 in `hakios-xnu-kernel`, none in the
other two — and none is licence-related; discussions are disabled on all three.
There is no public reuse request, re-licensing discussion or permission grant
anywhere in the projects (which is why a written request would start a new
thread, not join one).

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
the kernel half. The obligations that come with it, read from the licence text
(`hakios-mt6768/LICENSE.APSL`, byte-identical to Apple's, sha256
`e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b`): keep
Apple's notices and the licence's own notices (§2.1(a)); ship a copy of the
licence (§2.1(b)); put the Exhibit A notice in each modified file *and* a
prominent "changed by … on …" note beside it (§2.2(b)); make our modifications'
source available under the APSL (§2.2(c)); and, when shipping object code, say
where the source is (§2.3). APSL §4 expressly allows a Larger Work that mixes it
with other code, which is why per-file licensing inside one tree — our MIT files
beside the APSL-derived patches — is the arrangement both projects already use.
Two of these are ours to do explicitly rather than inherit: the driver sits in
`hakios-mt6768` with no Exhibit A notice at all, so a strict reading makes us
the first to add one, and the "changed by … on …" line is ours to write.

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

## 6. What is inconsistent, and what would settle it

Four statements inside the reference contradict each other. None is a licence
term; each is a reason a downstream reuser cannot resolve the question from the
repository alone.

1. `THIRD_PARTY_LICENSES.md` calls `tools/mesa/air/` "our own"; commit
   `09ebcf8a93` calls the same directory "vcvkk's AIR front end".
2. The `mtk` `README.md` says the terms are "as the file headers say"; the Mali
   files it covers have no headers.
3. `userland/src/HakiosMetalCA.m` carries an `SPDX-License-Identifier: MIT`
   header while `COMPLIANCE.md` says its permission is "not established".
4. `main`'s `LICENSE` names `userland/src/` but `main` is not the branch that
   holds the Mali userland, and that branch has no `LICENSE` at all.

A short written reuse request to the maintainer would settle all four and the
Mali userland's README-only grant, and it would start a new thread (there is no
open licence issue to add to, §2). The full evidence behind every claim here —
the exact URLs and the quoted lines — is in the research note
`mali-licence-subagent-report.md`, kept outside the repository.

## Sources

- `api.github.com/repos/oosexgang/hakios-xnu-kernel` (licence `NOASSERTION`,
  public, default branch `main`), its `git/trees/620c775` and
  `git/trees/main?recursive=1`, and the raw files `LICENSE`, `LEGAL.md`,
  `NOTICE`, `THIRD_PARTY_LICENSES.md`, `COMPLIANCE.md` on `main`.
- Raw files at `620c775`: `userland/src/redwoodmetal.m`,
  `userland/src/metal/*`, `tools/mesa/air/*`, `tools/mesa/*.patch`,
  `tools/mesa/redwood-drm/redwood_drm.c`.
- Branch `mtk`: `commits?sha=mtk` (7 commits, 2026-11-06 → 2026-11-10),
  `commits/3f31f34285` (`parents: []`), the root listing of
  `git/trees/a962798418fa` (no licence file), `compare/main...mtk` ("No common
  ancestor").
- `main` history: `commits/ea3fadaabc` (the `LICENSE`, added 2026-10-25),
  `commits/09ebcf8a93` (the vcvkk attribution for `tools/mesa/air/`),
  `commits?path=userland/src/metal&sha=main` (empty).
- Negative results: `repos/vcvkk/darwin-redwood`, its web page, its commit
  `eb9fdf5`, `users/vcvkk/repos`, repository search, and the Wayback API — all
  404/empty today.
- Issues and PRs searched in all three repositories for licence/reuse terms
  (0 licence-related; discussions disabled).
- APSL 2.0 canonical text: `apple/darwin-xnu`'s `LICENSE` (byte-identical to
  `hakios-mt6768/LICENSE.APSL` and `hakios-xnu-kernel/LICENSE.APSL`, sha256
  `e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b`).
- `api.github.com/repos/oosexgang/hakios-mt6768` (licence `MIT`, public), its
  `git/trees/84b0252` and the raw `LICENSE` and `NOTICE`.
- `api.github.com/repos/oosexgang/hakios-metal` (licence `MIT`, public) and its
  description ("Moved into the kernel: ... hakios-metal/").
