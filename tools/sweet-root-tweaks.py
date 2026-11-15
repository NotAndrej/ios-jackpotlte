#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Apply the SWEET bring-up changes to an iOS root made by tools/mkiosroot.sh.

  sudo sweet-root-tweaks.py MOUNTED_ROOT

MOUNTED_ROOT is the HFS+ image mounted read-write (Linux: run
`fsck.hfsplus -fy` on the image, then `mount -t hfsplus -o loop,force`).
Run as root (it writes root/mobile owned files). Idempotent. This is what
turned the plain mkiosroot.sh output into the images that reach the home
screen (image versions v8-v14 in NOTES.md):

  launchd.plist   _PanicOnCrash -> _PanicOnCrasX (a crashing daemon must not
                  panic the system); CommCenter jobs removed (no baseband: it
                  crash-loops and Setup blocks on its CoreTelephony XPC)
  containers      /private/var/containers/{Bundle,Data/System,Shared/SystemGroup,Temp}
                  (on iOS they live on the data volume)
  migration       LastSystemVersion so DataMigration does not run for an hour
                  on every boot
  audio           VirtualAudio HAL plug-in disabled (mediaserverd crash)
  MediaToolbox    StartRecorderServer=false (camera filter prewarm calls a
                  NULL OpenGL function without a GPU -> mediaserverd crash loop)
  SpringBoard     BKNoWatchdogs (one slow CPU path, no scene-create watchdog
                  kills), no cloud-configuration UI
  Setup           skipped: SetupDone, SetupFinishedAllSteps, SetupVersion 11,
                  mini-buddy flags off
  activation      mobileactivationd data ark seeded; the actual Activated state
                  comes from its Hactivation branch, selected by the /chosen
                  security-status properties in tools/mkadt.py
  mobilegestalt   com.apple.MobileGestalt.plist in the mobilegestaltcache
                  SystemGroup: CacheExtra answers that make the device an
                  iPhone X everywhere userspace asks MobileGestalt, and that
                  pass SpringBoard's Dynamic Island gate (see the block comment
                  above mobilegestalt() for the measured evidence)
"""
import glob, os, plistlib, shutil, sys

BUILD, VERSION = '20H392', '16.7.16'
MOBILE = 501


def load(path):
    with open(path, 'rb') as f:
        return plistlib.load(f)


def save(path, data, uid, gid, mode, fmt=plistlib.FMT_BINARY):
    with open(path, 'wb') as f:
        plistlib.dump(data, f, fmt=fmt)
    os.chown(path, uid, gid)
    os.chmod(path, mode)


def merge_pref(root, name, values):
    """Merge keys into /var/mobile/Library/Preferences/NAME.plist (mobile, 0600)."""
    path = os.path.join(root, 'private/var/mobile/Library/Preferences', name + '.plist')
    data = load(path) if os.path.exists(path) else {}
    data.update(values)
    save(path, data, MOBILE, MOBILE, 0o600)
    print('pref', name, sorted(values))


def launchd(root):
    path = os.path.join(root, 'System/Library/xpc/launchd.plist')
    st = os.stat(path)
    plist = load(path)
    jobs = plist['LaunchDaemons']
    renamed = 0
    for job in jobs.values():
        if '_PanicOnCrash' in job:
            job['_PanicOnCrasX'] = job.pop('_PanicOnCrash')
            renamed += 1
    removed = [k for k, j in jobs.items() if j.get('Label') == 'com.apple.CommCenter']
    for k in removed:
        del jobs[k]
    save(path, plist, st.st_uid, st.st_gid, st.st_mode & 0o7777)
    print(f'launchd.plist: {renamed} _PanicOnCrash renamed, {len(removed)} CommCenter jobs removed')


def containers(root):
    base = os.path.join(root, 'private/var/containers')
    for sub, uid in (('Bundle', 33), ('Data/System', 0), ('Shared/SystemGroup', 0), ('Temp', 0)):
        d = os.path.join(base, sub)
        if not os.path.isdir(d):
            os.makedirs(d)
            os.chown(d, uid, uid)
            os.chmod(d, 0o755)
            print('mkdir', d)


def find_container(root, kind, identifier):
    for meta in glob.glob(os.path.join(root, 'private/var/containers', kind, '*',
                                       '.com.apple.mobile_container_manager.metadata.plist')):
        if load(meta).get('MCMMetadataIdentifier') == identifier:
            return os.path.dirname(meta)
    return None


def activation(root):
    c = find_container(root, 'Data/System', 'com.apple.mobileactivationd')
    if c is None:
        print('warning: no mobileactivationd system container in the image; skipped data ark')
        return
    d = os.path.join(c, 'Library/internal')
    os.makedirs(d, exist_ok=True)
    os.chown(d, MOBILE, MOBILE)
    os.chmod(d, 0o755)
    save(os.path.join(d, 'data_ark.plist'), {
        '-ActivationState': 'Activated', '-BrickState': False,
        '-BuildVersion': BUILD, '-ProductVersion': VERSION,
        '-DataMigrationCompleted': True, '-PreSydroDataMigrationCompleted': True,
    }, MOBILE, MOBILE, 0o600)
    print('data ark', d)


def audio(root):
    src = os.path.join(root, 'Library/Audio/Plug-Ins/HAL/VirtualAudio.plugin')
    dst_dir = os.path.join(root, 'Library/Audio/Plug-Ins/HAL.disabled')
    if os.path.exists(src):
        os.makedirs(dst_dir, exist_ok=True)
        shutil.move(src, dst_dir)
        print('VirtualAudio.plugin disabled')
    for conf in glob.glob(os.path.join(root, 'System/Library/Frameworks/MediaToolbox.framework/*/SubServerConfiguration.plist')):
        st = os.stat(conf)
        data = load(conf)
        data['StartRecorderServer'] = False
        save(conf, data, st.st_uid, st.st_gid, st.st_mode & 0o7777, plistlib.FMT_XML)
        print('StartRecorderServer off', os.path.relpath(conf, root))


# --- MobileGestalt: iPhone X identity and Dynamic Island --------------------
#
# Everything below was measured on *this* tree (16.7.16 / 20H392) by reading
# the shipping caches in place, not taken from another iOS version.
#
# Where MobileGestalt keeps its cache (libMobileGestalt.dylib, dyld cache
# subcache .09, image base 0x19c555000):
#
#   read path  the constant C string at VA 0x19c5bbead:
#              /private/var/containers/Shared/SystemGroup/
#              systemgroup.com.apple.mobilegestaltcache/Library/Caches/
#              com.apple.MobileGestalt.plist
#              On the read side the prefix is the fixed literal above; only
#              the write path asks container_system_group_path_for_identifier
#              (its only call site is 0x19c58e6fc), so a plain file at that
#              path is what every reader loads.  The legacy cache
#              /var/mobile/Library/Caches/com.apple.MobileGestalt.plist is
#              deleted outright by __MGCacheValid (0x19c558b00) and is not a
#              substitute.
#
#   validity   __MGCacheValid's own log strings (MobileGestaltCache.c) say
#              what it compares: "can't validate cache version since
#              _kCFSystemVersionBuildVersionKey is missing", "cache %@
#              doesn't match OS %@", "can't validate cache since uuid is
#              missing", "cache UUID %@ doesn't match build UUID %@".  So
#              CacheVersion is the ProductBuildVersion (20H392) and CacheUUID
#              is the build UUID __MGWriteCache (0x19c58e28c) writes, the
#              constant 42F6FA57-C5A1-4134-9BF4-AB00F962EAE8.  This matters:
#              /System/Library/DataClassMigrators/MobileGestaltMigrator
#              calls __MGCacheValid on every boot and rebuilds the cache when
#              it is false ("Migrator rebuilt MGCache in %lf seconds"), which
#              would discard CacheExtra.  Matching both keys is what keeps
#              these overrides across boots.
#
#   overrides  CacheExtra is the per-question override dictionary; the loader
#              validates every key against the 767-entry question table
#              ("cache extra has invalid question %@") and then logs "Cache
#              loaded with %zu pre-cached in CacheData and %ld items in
#              CacheExtra."  An empty CacheData therefore only means "nothing
#              pre-cached": each answer is computed live and then overridden.
#
# The keys are the mangled questions MG itself embeds; the names come from
# the __copy_X getters that convert the answers to C strings, so the override
# values are strings.  They were cross-checked against the identities the
# kernel halves of the fix produce (xnu/patches/0169 + tools/mkadt_jackpotlte.py
# put D22AP first in root compatible and model=iPhone10,3), so userspace and
# kernel agree; this step is what makes userspace right even on an image built
# from an older kernel.
#
# Dynamic Island: the gate is *not* the iOS 17.4+ question
# YlEtTtHlNesRBMal1CqRaA (DeviceSupportsDynamicIsland) -- it has zero
# references in this cache, so setting it (as tools for newer iOS do) can do
# nothing here.  On 16.7.16 the gate is the SpringBoard feature flag Jindo
# (already FeatureComplete in /System/Library/FeatureFlags/Domain/
# SpringBoard.plist on this root) *and* an artwork subtype of 2556 or 2796:
# _SBUIIsSystemApertureEnabled (SpringBoardUIServices) -> SBFEffectiveArtworkSubtype
# (SpringBoardFoundation) / _SBSIsSystemApertureAvailable reads
# MGCopyAnswer("oPeik/9e8lQWMszEjbPzng"), CFDictionaryGetValue(..,
# "ArtworkDeviceSubType").  A D22AP advertises the iPhone X's 2436, which the
# gate rejects (2556 and 2796 are the 14 Pro/15 Pro classes), so the subtype is
# the only thing missing; the dict carries just that one key because it is the
# only key the gate reads.  The A8 panel is 1080x2220 at scale 3 = 360x740 pt,
# which can host the island's ~126 pt centred presence; the island's layout on
# that panel is the display lane's problem, not this step's.
MG_CACHE = ('private/var/containers/Shared/SystemGroup/'
            'systemgroup.com.apple.mobilegestaltcache/Library/Caches/'
            'com.apple.MobileGestalt.plist')
MG_BUILD_UUID = '42F6FA57-C5A1-4134-9BF4-AB00F962EAE8'   # __MGWriteCache constant
MG_EXTRA = {
    'h9jDsbgj7xIVeIQ8S3/X3Q': 'iPhone10,3',   # productType   (sysctl hw.product)
    '/YYygAofPDbhrwToVsXdeA': 'D22AP',        # hwModelStr    (sysctl hw.target)
    'ivIu8YTDnBSrYv/SN4G8Ag': 'iPhone X',     # productName   (About / Apple Account)
    'Z/dqyWS6OZTRy10UcmUAhw': 'iPhone X',     # marketingNameString
    # Dynamic Island: ArtworkDeviceSubType 2556 is the smallest subtype the
    # gate accepts, so it is the least disruptive claim for a 360x740 panel.
    'oPeik/9e8lQWMszEjbPzng': {'ArtworkDeviceSubType': 2556},   # artworkTraits
}


def mobilegestalt(root):
    path = os.path.join(root, MG_CACHE)
    data = load(path) if os.path.exists(path) else {}
    if not isinstance(data.get('CacheExtra'), dict):
        data['CacheExtra'] = {}
    if not isinstance(data.get('CacheData'), dict):
        data['CacheData'] = {}
    # CacheVersion/CacheUUID are what __MGCacheValid checks; leave an existing
    # valid value alone, but never ship a cache that fails the check.
    data['CacheVersion'] = BUILD
    if not data.get('CacheUUID'):
        data['CacheUUID'] = MG_BUILD_UUID
    data['CacheExtra'].update(MG_EXTRA)      # set-style: same keys, same values
    if not os.path.exists(path) or load(path) != data:
        d = os.path.dirname(path)
        for p in (os.path.split(d)[0], d):    # Library, Library/Caches
            if not os.path.isdir(p):
                os.makedirs(p)
        for p in (os.path.split(d)[0], d):
            os.chown(p, MOBILE, MOBILE)       # MobileGestaltHelper runs as mobile
            os.chmod(p, 0o755)
        save(path, data, MOBILE, MOBILE, 0o644)
        print('mobilegestalt', MG_CACHE)
    else:
        print('mobilegestalt', MG_CACHE, 'already set')
    back = load(path)
    bad = [q for q, v in MG_EXTRA.items() if back.get('CacheExtra', {}).get(q) != v]
    if back.get('CacheVersion') != BUILD:
        bad.append('CacheVersion')
    if back.get('CacheUUID') != MG_BUILD_UUID:
        bad.append('CacheUUID')
    if bad:
        sys.exit('error: MobileGestalt cache read-back mismatch in %s: %s'
                 % (path, ', '.join(bad)))


def main():
    if len(sys.argv) != 2 or not os.path.isdir(os.path.join(sys.argv[1], 'System/Library/xpc')):
        sys.exit(__doc__)
    root = sys.argv[1]
    launchd(root)
    containers(root)
    activation(root)
    audio(root)
    mobilegestalt(root)
    # DMLastMigrationResults is the extra key that HakiOS, danstronics and
    # lechaukha12/xnu-xzs (iOS 16.7.16, same as ours) ship; the in-file comment
    # on theirs reads "without this record of the build that last ran the data
    # migration, backboardd runs all the migrators (the progress bar under the
    # logo) each time." The donor's own proven set for 20H392 is
    # LastSystemVersion + DMUserDataDisposition; this key is additive, so drop
    # it here if a run ever misbehaves and we want the donor set exactly.
    merge_pref(root, 'com.apple.migration', {
        'LastSystemVersion': BUILD,
        'DMUserDataDisposition': {'SyntheticDidUpgrade': False, 'buildVersion': BUILD},
        'DMLastMigrationResults': {'success': True, 'buildVersion': BUILD}})
    merge_pref(root, 'com.apple.springboard', {
        'BKNoWatchdogs': True,
        'FBLaunchWatchdogExceptions': {'com.apple.purplebuddy': 100.0},
        'MCForceShowCloudConfigurationUI': False})
    merge_pref(root, 'com.apple.purplebuddy', {
        'SetupDone': True, 'SetupFinishedAllSteps': True, 'SetupVersion': 11,
        'daemonizeNeedsToRun': False})
    merge_pref(root, 'com.apple.purplebuddy.notbackedup', {
        k: False for k in ('ShouldRunDiagnosticsMiniBuddy', 'ShouldRunHSA2MiniBuddy',
                           'ShouldRunKeychainSyncMiniBuddy', 'ShouldRunMesaMiniBuddy',
                           'ShouldRunPaymentMiniBuddy', 'ShouldRunStandardMiniBuddy',
                           'SilentICDPUpgrade')})
    merge_pref(root, 'com.apple.managedconfiguration.notbackedup', {
        'LockdownActivationIndicatesCloudConfigurationAvailable': False})


if __name__ == '__main__':
    main()
