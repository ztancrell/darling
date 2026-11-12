# League of Legends installer under Darling: baseline, 2026-10-04

## Result

The installer loads and starts, then crashes about 0.4 seconds after launch with a segmentation
fault inside the Objective-C runtime. No window was captured. All three runs agree.

Stage reached: **runtime start**. Install into the prefix, binary load, and library and symbol
resolution all succeeded. It did not reach window, render, or network.

## What was tested

| Item | Value |
|---|---|
| App | `Install League of Legends na.app`, from the Homebrew cask `league-of-legends` URL, unmodified |
| Zip sha256 | `074f5120a91f411e28eec458ad97cae2c4d65b93d2cd1d98899a7d12f72edbec` |
| Executable | `Contents/MacOS/RiotClientServices`, universal (x86_64 + arm64), min macOS 10.15, SDK 13.1 |
| Darling | branch `local/league-baseline`, commit `1ef728c18`, clean tree, RelWithDebInfo |
| Difference from master | xnu `4d77d4b` (local, unmerged): sigexc guard moved from `__thread` to a TSD slot. Master `5a49c8fba` does not link dyld on this host. |
| Host | x86_64, Linux 7.2.8 (CachyOS), X11 session, RADV, clang 23.1.1 |
| Runtime | DESTDIR install, build tree's non-setuid launcher, reported macOS 26.0 |
| Command | `darling shell "/Applications/Install League of Legends na.app/Contents/MacOS/RiotClientServices"` |
| Prefix | fresh per run: `~/.darling-league-run1`, `-run2`, `-run3` |
| Server log level | `DSERVER_LOG_LEVEL=info` |

## Static scan (before launch)

18 linked libraries, none absent. 509 imported symbols, all exported by a present linked library.
Done with a small x86_64 checker, because the repo's `scan-imported-apps.py` reads arm64 slices only.
It does not follow two-level namespaces or weak imports precisely. A clean scan is not a launch.

## The three runs

| | Run 1 | Run 2 | Run 3 |
|---|---|---|---|
| Container boot | exit 0 | exit 0 | exit 0 |
| Staging into /Applications | exit 0 | exit 0 | exit 0 |
| App exit code | 139 | 139 | 139 |
| Signal | SIGSEGV, si_code SI_TKILL | same | same |
| Fault type | page fault, user read, not-present (trap 0xE, err 0x4) | same | same |
| Fault address (CR2) | 0x18 | 0x18 | 0x18 |
| Fault RIP | 0x7FA4A3D980DD | 0x7F42EC1B40DD | 0x7FB5D78830DD |
| Coredumps | 1, the app only | 1 | 1 |
| Shutdown | exit 0, no server left | same | same |

The RIPs differ only by load address; the low 12 bits match, so it is the same instruction.
`SI_TKILL` is Darling's handler re-raising the signal after emulating the default action; one
process dying alone, so not a process-group kill.

## Where it crashes

Using the file map in run 1's coredump: the RIP is in `/usr/lib/libobjc.A.dylib` at offset 0x400dd,
which is `objc_msgSend + 0x1d`. A read at address 0x18 there means a message was sent to an object
whose class pointer is NULL (a nil receiver would have returned early instead).

Not yet known: which object, and which call in the app sent the message. The coredump's stack shows
return addresses inside the app binary (0x100ce0c6a, 0x100cc621b, 0x100b54fd4); they have not been
resolved to names.

## What the app printed

stdout empty. stderr, identical in all three runs apart from the pid and addresses:

    objc: Class NSConstantArray is implemented in both CoreFoundation and Foundation. One of the two will be used. Which one is undefined.
    objc: Class NSConstantDictionary is implemented in both CoreFoundation and Foundation. ...

A possible lead, not a conclusion: a duplicated constant-collection class and a NULL class pointer
are both about object class identity. Nothing here shows they are connected.

## What the server saw (app process, run 1)

- 57 x `TODO: DarlingServer::Call::PthreadCanceled::processCall()`
- 5 x `stub: shim___pthread_testcancel`
- 5 x `stub: proc_get_effective_thread_policy: unimplemented flavor`
- 2 x `stub: shim_uthread_is_cancelled`

No "fault inside the handler" line in any run, so the sigexc re-entry guard did not fire.

## Limits of this baseline

- No screenshot: the app died before the 30-second capture point. "No window" is inferred from a
  0.4-second lifetime, not observed.
- Three clean boots do not show container startup is reliable; it is known to fail intermittently.
- Run from a local branch with one unmerged fix, not from master.
- x86_64 only. The arm64 slice was not tested.

## Evidence

`run1/`, `run2/`, `run3/` beside this file: `meta.txt`, `app.out`, `app.err`, `boot.*`, `stage.*`,
`dserver.log`, `coredumps.txt`, `coredumps-info.txt`, `sw_vers.txt`. `static-scan.txt` at top level.
Run 1's core is also extracted in the session scratch directory (temporary).

---

# Diagnosis, 2026-10-04 (after the baseline)

## Cause of the crash

`CTFontManagerRegisterGraphicsFont` in Darling is a stub
(`src/external/cocotron/CoreText/CTFontManager.m:9`): it prints `STUB ...` and returns false
without writing the `CFErrorRef*` out-parameter. The app then passes its own uninitialized error
variable to `CFErrorCopyDescription`, which sends `localizedDescription` to it and crashes.

## Evidence chain

1. Fault-time registers (run 1 `dserver.log`): `rdi` (receiver) = `0x5A35B1ECB`, not a heap
   pointer; `rsi` (selector) points at the string `localizedDescription` in CoreFoundation.
2. Top of stack at the fault: return address in `CoreFoundation` `CFErrorCopyDescription + 0x26`,
   then the app at `0x100ce0c6a`.
3. Disassembly of the app function at `0x100ce0a70`: `CGDataProviderCreateWithCFData` ->
   `CGFontCreateWithDataProvider` -> `CTFontManagerRegisterGraphicsFont(font, &error)`; if it
   returns false, `CFErrorCopyDescription(error)` with `error` read from an uninitialized stack slot.
4. Rerun A (stdout on a terminal, `DYLD_PRINT_LIBRARIES=1`): the last line before the crash is
   `STUB _Bool CTFontManagerRegisterGraphicsFont(CGFontRef, CFErrorRef *)`. In the baseline runs
   stdout was a file, so this line was still in the buffer when the process died.
5. Rerun B (`xtrace`): the last file opened is `Contents/Resources/Fonts/fonts.wad`, followed by
   reads from it, then the crash.
6. The app's own log (`run1/Launcher.log`) stops 43 ms in, after "Launcher Build CL:4786".

## What this rules out

The duplicate `NSConstantArray` / `NSConstantDictionary` warning is not the cause of this crash.
It is still a real warning and may matter later.

## Not established

Whether the font would register correctly if the function were implemented, and what the app does
after its fonts load. `CGFontCreateWithDataProvider` returned something, but its value was not checked.

Diagnostic evidence: `diag/` (`a.typescript`, `b.xtrace.out`, `dserver.log`). These reruns reused
prefix run1 and are not baseline runs.

---

# After the font-registration change, 2026-10-04

Darling `local/league-baseline` at `20173f7e4` (cocotron `02d9472b`, xnu `4d77d4b`; both local,
unmerged). `CTFontManagerRegisterGraphicsFont` now logs and reports success without registering.
Three runs, reusing the baseline prefixes. Evidence in `after-font/run1..3`.

## Result

3 of 3: exit 134 (SIGABRT, si_code SI_TKILL) about 1.5 s after launch, from an uncaught exception:

    -[NSImage drawInRect:]: unrecognized selector sent to instance ...

raised under `-[NSWindow makeKeyAndOrderFront:]` -> `-[NSView displayRectIgnoringOpacity:]`, i.e.
while drawing the splash window for the first time.

Stage reached: **window, first draw**. Darling's `NSImage` has `drawInRect:fromRect:operation:fraction:`
variants but no plain `drawInRect:` (`src/external/cocotron/AppKit/NSImage.m`).

## What the app got through first (its own log, `after-font/run1/Launcher.log`)

- 22 calls to `CTFontManagerRegisterGraphicsFont`, each logged; every font had no PostScript name
  available from `CGFontCopyPostScriptName`.
- Splash screen initialised, window sized 392x488 and centred.
- Local remoting server started; an installer agent process was launched and connected over
  local HTTPS.
- A config request to `clientconfig.rpg.riotgames.com` was issued (result not checked).
- "Splash screen now visible", then the exception.

## Not established

- Whether a window was actually visible on screen (the app died before the 30 s capture).
- Whether the config request succeeded.
- The app log reports `osVersion: 10.10.10` while `sw_vers` in the prefix reports 26.0. Unexplained.
- Fonts: nothing is registered, so any by-name lookup of Riot's fonts gets a fallback.

---

# After adding -[NSImage drawInRect:], 2026-10-04

Darling `local/league-baseline` at `77c40b8fb` (cocotron `cc0eafbf`, xnu `4d77d4b`; local, unmerged).
Three runs reusing the baseline prefixes, 180 s limit. Evidence in `after-drawinrect/run1..3`.

| | Run 1 | Run 2 | Run 3 |
|---|---|---|---|
| Window drawn | yes | yes | yes |
| Hostname lookups | failed (10 x "Could not resolve host") | worked | worked |
| Screen reached | "NO INTERNET FOUND", Retry / Exit | install screen: Install, Advanced Options | install screen |
| How it ended | still running at 180 s, shut down | still running at 180 s, shut down | window closed at 44 s, then SIGSEGV during quit |
| Coredumps | 0 | 0 | 1 |

Stage reached: **network, and the installer's first interactive screen** in 2 of 3 runs. In those
runs the app fetched its config, downloaded from Riot's CDN, loaded a release manifest and built an
update plan. Captures at 30 s show Riot's artwork, logo, text and buttons rendered.

## Two new findings

1. **Intermittent name resolution.** Same build and settings; run 1 could not resolve
   `clientconfig.rpg.riotgames.com` or `data.riotgames.com`, runs 2 and 3 could. Guest
   `/etc/resolv.conf` lists 8.8.8.8 and 8.8.4.4; host uses systemd-resolved at 127.0.0.53.
   Cause not investigated.
2. **Crash on quit (run 3).** After `WM_DELETE_WINDOW` and the app's `OnQuit`, SIGSEGV in
   `objc_msgSend` sending `respondsToSelector:` to an object at a heap address whose class pointer
   is not valid (fault address 0x1AA0080018). Looks like a message to an object that had already
   been freed; not confirmed. Who closed the window is not recorded.

## Not tested

The Install button and everything after it. Font fidelity: nothing is registered, so the text seen
may be a fallback typeface.

---

# Installer completes; Riot Client fails to load, 2026-10-04

Darling `local/league-baseline` at `cbc3b6013` (cocotron `39deb0d4`: theme frame forwards an
unhandled mouse-down to the window; background dragging removed). Evidence in `install-attempt2/`.
`install-attempt1/` is the run before that change, where clicks only moved the window.

## Installer: works end to end (one run)

Install button responded. The installer downloaded and wrote `/Users/Shared/Riot Games/Riot
Client.app` (1.2 GB, release 770A7E41C9B7D36D), logged "Self update succeeded" and "Success!
Exiting", exit code 0, no coredump. One error on the way: "Failed to create URI Handler: Failed
launching lsregister: Executable path does not exist" (non-fatal). Some CDN requests over IPv6
failed and were retried. Hostname lookups failed early in this run too, then recovered.

## Riot Client: launches, then cannot load its main library

The installer started the installed client. It showed its splash, validated the install, then:

    Could not load .../Contents/Frameworks/libRiotClientFoundation.dylib: Symbol not found:
    _LSSetDefaultHandlerForURLScheme, referenced from libdiscord_partner_sdk.dylib, expected in CoreServices

and exited. No crash.

## Static scan of the installed Riot Client (`install-attempt2/riot-client-static-scan.txt`)

23 x86_64 Mach-O files. The client's UI is an Electron (Chromium) app. About 157 imported symbols
are not exported by Darling, 153 of them needed by `Electron Framework`; two weak-linked frameworks
are absent (MediaPlayer, QuickLookThumbnailing). Largest groups: CoreGraphics 24, Carbon events and
input 19, Vision 17, CoreMedia 16, MediaPlayer 12, AppKit/Foundation 11, Metal 10, LaunchServices 8.
The scanner also listed basic libc symbols for the "Riot Client Helper" binaries; those look like a
scanner limitation, not real gaps, and are excluded from the count. Weak imports are not separated.

---

# Riot Client backend loads, then crashes swapping the app delegate, 2026-10-04

Darling `local/league-baseline` at `18fb68253` (adds a placeholder `LSSetDefaultHandlerForURLScheme`
that logs and reports success). Evidence in `riot-client-run1/` (one run).

- `libRiotClientFoundation.dylib` now loads: log shows `foundation_loaded` and "Launching foundation".
- Then SIGSEGV (exit 139, si_code SI_TKILL), about 7 s after launch.
- Fault: `objc_msgSend` sending `respondsToSelector:` (argument `applicationWillFinishLaunching:`)
  to an object whose class pointer is not valid (CR2 0x18A0080018).
- Stack: `libRiotClientFoundation` -> `-[NSApplication setDelegate:]` -> `-[NSApplication
  unregisterDelegate]` -> `objc_msgSend`.
- Cause, from the code (`src/external/cocotron/AppKit/NSApplication.m:360`): `unregisterDelegate`
  asks the old delegate which methods it implements before removing its notification observers.
  `_delegate` is a plain unretained pointer, and the launcher's delegate has already been freed by
  the time the backend installs its own.
- Same signature as the installer's crash on quit recorded earlier (same message, same kind of
  invalid receiver); likely the same defect. Not separately confirmed.
- Darling's `NSNotificationCenter` removes observers by pointer comparison only
  (`src/external/foundation/src/NSNotificationCenter.m:182`), so removal itself does not need to
  message the old delegate.

---

# Weak NSApplication delegate; backend starts, then a missing AVCaptureDevice class method, 2026-10-04

Darling `local/league-baseline` at `27a09f2cd` (cocotron `febdba36`: NSApplication holds its delegate
through objc_storeWeak/objc_loadWeak and delivers the four launch/activate/terminate notifications
to it directly instead of registering it as an observer). Evidence in `riot-client-run2/` (one run).

`riot-client-run2-stale-runtime/` is an invalid run: the container had been left running across the
reinstall, so the prefix still served the previous AppKit (confirmed from the coredump's file map
and the stack, which contained the removed `unregisterDelegate`). Shut down, relaunched.

Result of the valid run:
- No crash in `setDelegate:`. The backend ran about one second of startup and wrote 373 lines to
  its own log (`RiotClient.log`): SDK plugins initialising, settings, status, voice chat.
- Then exit 134: uncaught exception `+[AVCaptureDevice authorizationStatusForMediaType:]:
  unrecognized selector`, thrown from RiotGamesApi on a background thread (voice chat checking
  microphone permission).
- Darling's `AVCaptureDevice` (`src/frameworks/AVFoundation/src/AVCaptureDevice.m`) is a generated
  stub class that forwards unknown *instance* messages to a logging stub; class methods are not
  covered. The backend also uses `requestAccessForMediaType:completionHandler:`.
- Also in the backend log: `nw_path_monitor_create` failed ("Unable to create"), handled by the app.

Not checked yet: whether the installer's crash on quit is gone with this change.

---

# Microphone-permission answers; backend runs ~20 s, then a missing NSMenuItem method, 2026-10-04

Darling `local/league-baseline` at `b2e974652` (AVFoundation: `+[AVCaptureDevice
authorizationStatusForMediaType:]` returns NotDetermined; `+requestAccessForMediaType:
completionHandler:` logs and answers NO on a global queue). Evidence in `riot-client-run3/` (one run).

- Backend ran about 20 s and wrote 779 log lines. It reached Riot's services and received patchline
  metadata, including `riot_client.live` and `league_of_legends.pbe`.
- Exit 134: uncaught exception `-[MenuItem setView:]: unrecognized selector`, on the main thread,
  from libRiotClientFoundation. `MenuItem` is Riot's subclass of `NSMenuItem`; the backend is
  building a status-bar (tray) menu.
- Darling's `NSMenuItem` has no `view` / `setView:`; `NSStatusItem` has no `button`. The other
  status-bar methods the backend uses exist (`systemStatusBar`, `statusItemWithLength:`,
  `removeStatusItem:`, `setMenu:`, `setImage:`, `-[NSImage setTemplate:]`).
- Backend log, three times: `vanguard: Failed getting installed version of Vanguard: Unsupported
  operating system`. Not known whether a real Mac logs the same line.
- Some `clientconfig.rpg.riotgames.com` fetches failed in this run (the intermittent lookups).

---

# Tray-menu methods; client runs 60 s and launches its Electron UI, 2026-10-04

Darling `local/league-baseline` at `58270ac0f` (cocotron `8f66c126`: `NSMenuItem` stores a custom
view; `-[NSStatusItem button]` returns nil). Evidence in `riot-client-run4/` (one run).
Method scan of the backend: `riot-backend-method-scan.txt` (selector-level; cannot see a selector
that exists on another class).

- Backend ran 60 s, 802 log lines, no abort during startup. 15 "Could not resolve host" errors
  (data.riotgames.com, clientconfig.rpg.riotgames.com, auth.riotgames.com, riotcdn hosts); the
  same names resolve on the host. Some requests succeeded (patchline metadata arrived).
- At 60 s the backend launched its UI: `.../Frameworks/Riot Client.app/Contents/MacOS/Riot Client`
  (Electron). It died at load: `dyld: Symbol not found: _kCVImageBufferTransferFunction_sRGB`
  (CoreVideo), referenced from Electron Framework.
- The backend then aborted: `-[NSMenu _setHasPadding:onEdge:]: unrecognized selector` (system tray
  setup; this selector was in the scan's list).
- A third coredump: a `RiotClientServices` process, SIGTRAP at 16:31:01, 30 s into the run, inside
  libRiotClientFoundation. Not investigated.

Electron Framework, required symbols missing from Darling, split by its bind tables
(`electron-at-load.txt`, `electron-on-call.txt`): 33 must resolve when the library loads (32 named
constants and one class, `NSScrubberImageItemView`); 66 are functions bound lazily, which fail only
if called.

---

# Load-time batch; Electron loads and runs, crashes in power-monitor setup, 2026-10-04

Darling `local/league-baseline` at `d0cf1fae5`. Since the previous section:
- `6121a1a38` (+ cocotron `edc3cc56`, foundation `abca9cf35`): four NSMenu / NSRunningApplication
  methods, 32 named constants and an empty `NSScrubberImageItemView`. Constants whose values are
  unverified (named after themselves) are marked in comments: the four `kCGDisplayStream*` keys, the
  four ImageCaptureCore strings, `kMACaptionAppearanceSettingsChangedNotification`.
- `d0cf1fae5`: LaunchServices re-exports `NSUserActivity` from Foundation (64-bit slices), because
  macOS binaries expect that class in CoreServices. The first scan missed this: it checked whether
  any linked library exported a symbol, not the library the binary names. `tools/scan2level.py`
  checks the way dyld does; `electron-scan-2level.txt` now shows 0 required symbols missing at load
  for Electron Framework and for the other UI binaries, and 65 lazily bound functions still missing.

Runs: `riot-client-run5` (before the NSUserActivity re-export: Electron stopped on that symbol),
`riot-client-run6` (after).

Run 6:
- Backend: no abort in tray setup; its app delegate receives applicationWillFinishLaunching (so the
  weak-delegate delivery path works). Ran about 75 s.
- Electron UI process (`.../Frameworks/Riot Client.app/Contents/MacOS/Riot Client`): loads, enters
  `ElectronMain`, then SIGSEGV in `CFRunLoopAddSource + 0x4d` reading address 0x8.
  Disassembly of the caller: `IORegisterForSystemPower(...)`, then
  `CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(port), modes)`.
  The source argument is NULL. `IORegisterForSystemPower` (Apple's code, IOKitUser
  `pwr_mgt.subproj/IOPMLib.c:264`) fails when the IO registry has no `IOPMrootDomain` entry and
  leaves the caller's port unset; Chromium does not check for failure.
- Backend then exits 133 (SIGTRAP on a worker thread) about 8 s after its UI died.
- A backend child dies 9 s in: `FATAL spawn_subprocess.cc:221] posix_spawnp: Operation not
  permitted` (Chromium's crashpad starting its handler). Seen in runs 4, 5 and 6. Not investigated.
- Also logged: `Failed to load root certificates: -50`.

---

# Guest processes cannot reach any launchd service in the non-root launcher mode, 2026-10-04

Darling `local/league-baseline` at `25d247087` (IOKitUser `e63ef13`: `IORegisterForSystemPower`
hands back its notification port when there is no IOPMrootDomain). Run: `riot-client-run7`.

Run 7: Electron passes the first `CFRunLoopAddSource` and crashes at the next one, 0x3e bytes later
in the same function: `CFRunLoopAddSource(..., IOPSNotificationCreateRunLoopSource(cb, NULL), ...)`
with a NULL source.

Probes (sources in `tools/probes/`, built against the local runtime, run with `darling shell`):

- `pstest`: `notify_register_mach_port` and `notify_register_check` both return 9
  (`NOTIFY_STATUS_SERVER_NOT_FOUND`); `IOPSNotificationCreateRunLoopSource` returns NULL.
  `notifyd` is running in the container at the time.
- `bstest`: `bootstrap_port` is non-null (0x707), and `bootstrap_look_up` fails with 1105
  (`BOOTSTRAP_NO_MEMORY`) for every name tried: notification_center, iokit.registry,
  launchservicesd, SecurityServer, opendirectoryd.libinfo, configd, cfprefsd, dnssd, distributed
  notifications.
- `pidtest`: `proc_pidinfo` works for the calling process (comm `mldr`). There is no PID namespace
  in this mode: launchd is host pid 513883, and pid 1 is the host's systemd.

Context: all runs in this report use the build tree's non-setuid launcher, i.e. the launcher's
non-root mode (`g_nonroot`, src/startup/darling.c:1780), in which the prefix's `/proc` is a symlink
to the host procfs. Fork issue #838 (closed 2026-09-23) records the same 1105 from launchd's
`job_mig_intran` in root mode, caused there by launchd being unable to identify the caller.

Not established: why the lookups fail in non-root mode; whether root mode (the setuid launcher, or
the build-tree launcher run as root) behaves differently on this machine.

Likely consequences already seen in this report, each unproven as a consequence:
`IOPMrootDomain` not found (iokitd unreachable), "Failed to load root certificates: -50"
(securityd), `nw_path_monitor_create` failing, and possibly the intermittent hostname lookups.
The `IORegisterForSystemPower` change in `e63ef13` may be working around this rather than a
missing registry entry.

---

# Root cause of the failed service lookups: launchd rejects PIDs of 100000 and above, 2026-10-04

`src/launchd/src/core.c:1739`, in `job_new_anonymous`:

    if (anonpid >= 100000) {
        /* The kernel current defines PID_MAX to be 99999 ... */
        launchd_syslog(LOG_WARNING, "Did PID_MAX change? Got request from PID: %d", anonpid);
        errno = EINVAL;
        return NULL;
    }

launchd creates an "anonymous job" for any caller it did not start itself; when that fails,
`job_mig_intran` answers BOOTSTRAP_NO_MEMORY (1105), which is what every lookup returned.

Evidence:
- Host `kernel.pid_max` is 4194304 and current PIDs are around 515000.
- Non-root mode has no PID namespace (`pidtest`: launchd is host pid 513883, pid 1 is systemd), so
  guest PIDs are host PIDs.
- With `DARLING_LAUNCHD_STDERR=1`, launchd writes `<prefix>/launchd_dbg.log`. After one `bstest` run
  it contains nine lines "Did PID_MAX change? Got request from PID: 515734", one per lookup the
  probe made.

So on this machine every guest process in non-root mode is above the limit, and none can reach a
launchd service. A host whose PIDs are still below 100000, or root mode with its own PID namespace,
would not show it. Not checked: whether anything else in Darling assumes PIDs below 100000.

---

# launchd PID limit raised; services reachable; Electron stops on a missing method, 2026-10-04

Darling `local/league-baseline` at `2dc75710a`: `job_new_anonymous` accepts caller PIDs up to
4194304 under `#ifdef DARLING` (src/launchd/src/core.c). Run: `riot-client-run8`.

Probes, three fresh container boots, identical each time:
- `bstest`: `bootstrap_port` 0x707; lookups succeed for notification_center, launchservicesd and
  SecurityServer. The other six names return 1102 "Unknown service name" (launchd answered; Darling
  registers no service under those names). Before the change all nine returned 1105.
- `pstest`: `notify_register_mach_port` status 0; `IOPSNotificationCreateRunLoopSource` non-NULL.
- launchd's debug log shows "Created PID ... anonymously" in place of the PID_MAX warning.
One earlier run, the first after the reinstall, showed `bootstrap_port = 0x0` and all sends failing
with "invalid destination port"; it did not recur in seven later runs. Unexplained.

Run 8:
- Backend exits 0 (first clean exit). 1081 log lines. `RiotClientCrashHandler` now starts: the
  crashpad `posix_spawnp: Operation not permitted` abort is gone. "Failed to load root
  certificates" is gone. The backend's SIGTRAP exit is gone. Hostname lookup failures: 7 (was 13-15).
- Still logged: `vanguard: Failed getting installed version of Vanguard: Unsupported operating system`.
- Electron UI process started 28 s in (was 60 s) and aborted: uncaught exception
  `-[NSProcessInfo thermalState]: unrecognized selector`.

Method scan of Electron Framework (`electron-method-scan.txt`, selector-level): 2615 selectors
referenced, 488 implemented nowhere in Darling; 49 system classes it references are hollow stubs
in Darling, among them CAContext and CALayerHost; 12 referenced classes have no methods at all.
Most of the 488 belong to features a launcher will not use (Bluetooth, payments, printing, game
controllers, Wi-Fi, speech); which ones the client reaches is only known by running it.

---

# Discovery mode for unrecognized selectors; an intermittent boot fault, 2026-10-04

Darling `local/league-baseline` at `ad7df3053` (corefoundation `14fb4ab`): with
`DARLING_TOLERATE_UNRECOGNIZED_SELECTORS` set in the guest, `doesNotRecognizeSelector:` logs each
class/selector pair once and returns, and `___forwarding___` answers zero in the integer return
registers. Off by default. Probe `tools/probes/seltest`: default run raises the usual exception;
with the variable set, both an instance and a class message return 0 and are logged once each.

Intermittent boot fault (non-root mode), cause unknown: on some container boots, shell-spawned
processes have no usable bootstrap port for the life of that container (one boot showed
`bootstrap_port = 0x0`; `riot-client-run9-discovery` showed the symptoms: crashpad spawn EPERM,
"Failed to load root certificates", Electron's NULL power-source run loop source). Seen on 2 of
about 12 boots. Not caused by the discovery variable or the `env` wrapper (checked on a fresh
boot, all four combinations fine). `tools/rc-run.sh` now runs `bstest` before each run, reboots the
container if services are unreachable, and records each bad boot in `meta.txt`.

`riot-client-run10-discovery` (boot check passed first time), results are leads, not proof:
- Backend exit 0, 1073 log lines, 2 hostname lookup failures.
- Electron reached four missing methods before stopping:
  `-[NSProcessInfo thermalState]`,
  `-[X11Screen maximumPotentialExtendedDynamicRangeColorComponentValue]`,
  `-[X11Screen maximumExtendedDynamicRangeColorComponentValue]`, `-[X11Screen colorSpace]`
  (X11Screen is Darling's NSScreen subclass).
- Then abort: `dyld: lazy symbol binding failed: Symbol not found: _CGDisplayUsesForceToGray`, one
  of the 64 lazily bound C functions Electron imports that Darling lacks (`electron-on-call.txt`).
  The selector mode cannot help with those: a missing C function aborts in dyld when first called.

---

# Electron runs past all missing symbols and stops at Mach shared memory in darlingserver, 2026-10-04

Darling `local/league-baseline` at the commit after `a6bb118c9` (configd placeholders moved into the
compiled stubs file). Since the previous section: real answers for `-[NSProcessInfo thermalState]`
(nominal), NSScreen EDR values (1.0) and `colorSpace` (sRGB), `CGDisplayUsesForceToGray` (false);
`vDSP_vsadd` fixed (it added a vector where the API adds a scalar) and added to the build;
a plain-copy `__atomic_load` exported from libsystem_sandbox; and 61 local discovery placeholders
that log "DARLING unimplemented function called: <name>" and return zero. `scan2level.py` on
Electron Framework: 0 required symbols missing at load, 0 when called.

`riot-client-run11-discovery` (discovery mode on):
- Boot check: first boot had no reachable services, second was fine (third bad boot of about 14).
- Electron ran far enough to emit Chromium's own log lines: "Could not get initial network
  connection type, assuming online" and "DNS config watch failed to start".
- One more unrecognized selector tolerated: `-[X11Screen localizedName]`.
- One placeholder called, six times: `CGColorSpaceCopyICCData`.
- `Stub called: setDelegate: in CLLocationManager` (a hollow stub class swallowing a message).
- Then `darlingserver` itself aborted (SIGABRT, 20:14:05). Stack: `MachMsgOverwrite` ->
  `ipc_kobject_server` -> `_X_mach_make_memory_entry` -> `_mach_make_memory_entry` ->
  `dtape_stub_log` -> `abort`. Its last log line: `stub (unsafe): _mach_make_memory_entry`, from the
  Electron process. `_mach_make_memory_entry` (darlingserver duct-tape/src/memory.c:557) is an
  "unsafe stub": calling it brings the server down.
- With the server gone, every guest process aborted within four seconds (SIGILL from
  `__simple_abort` in libsystem_kernel): opendirectoryd, launchservicesd and the Riot backend
  (exit 132). Those three are consequences, not separate faults.

`mach_make_memory_entry` is how a Mach task creates a handle to a region of memory that another
task can map: Mach shared memory. Chromium uses it for communication between its processes.

## 2026-10-04: memory entries for new shared regions (run12-memory-entry)

Change (local only): darlingserver 018a3e0 on branch local/league-memory-entry; superproject c63dc605d.
`_mach_make_memory_entry` with MAP_MEM_NAMED_CREATE now creates a memfd-backed named entry;
`mach_vm_map_external` maps an entry into the caller, or allocates plain memory when given no entry
(it used to report success and map nothing). Entries for existing memory, parent entries and
private-copy mappings are refused with a logged error instead of aborting the server.

Check: tools/probes/memtest, 10 of 10 checks pass on one boot (create, two mappings, write seen through
the other, out-of-range refused, mappings outlive the handle, null-handle map, existing-memory flavour
returns kr=5, server still answers). Not covered: passing the handle to another process.

Run 12 (discovery mode, good boot first try, one run):
- darlingserver stayed up for the whole run. Backend exited 0.
- Electron's JavaScript main process ran for the first time: its own log shows startup, "Static server
  started on port 33273", then reading RiotClientSettings.yaml.
- The crash dump shows two mappings of `/memfd:darling-memory-entry`, so the new path was used.
- At 20:35:45.438 the server refused two requests from Electron: entries for EXISTING memory,
  permission=0x1 (read only), no parent: size=0x20000 at 0x7f19a4ab9000 and size=0x28 at 0x7f19ca666000.
  Neither address is one of the memfd mappings, so both are ordinary private memory.
- Electron then died alone with SIGTRAP (20:35:46), which is how Chromium ends itself on a failed
  check. Cause inferred from timing; the trap's stack was not decoded.
- Newly seen: `+[NSColor controlAccentColor]` tolerated; `-[NSBitmapImageRep getPixel:atX:y:]`
  unimplemented (x2); CGColorSpaceCopyICCData placeholder x6 again.
Known ceiling: a mapped entry's memfd is kept until the task dies (guest unmaps are not reported).

## 2026-10-04: live memory entries for existing memory (run13-live-entry)

Change (local only): darlingserver a4b692b (branch local/league-memory-entry); superproject 5cf3bb914.
An entry made without MAP_MEM_NAMED_CREATE shares the caller's existing memory live. The private-to-
memfd conversion moved out of mach_vm_remap_external_shared into dtape_map_share_region (both use it);
ranges inside an already shared region reuse its memfd; stale recorded entries are dropped; partial
overlap is a logged error instead of an abort. Still refused: MAP_MEM_VM_COPY, MAP_MEM_ONLY, parents.

Check: tools/probes/memtest 21 of 21 on one boot, including "a later write by the owner shows through
the mapping" and "writable mapping of a read-only entry is refused". Not covered: two processes.

Run 13 (discovery mode, good boot first try, one run):
- No refused memory requests, no unsafe stubs; darlingserver stayed up; backend exited 0.
- Electron got further: Sentry setup, "Creating socket..", RCSWatchdog started.
- Chromium helper processes were spawned for the first time: "Riot Client Helper (GPU)"
  (--type=gpu-process, pid 613305) and "Riot Client Helper" (network service, pid 613308).
  Each printed "Failed to initialize sandbox." and aborted (SIGABRT, 21:08:44 and :45).
- The main Electron process then died with SIGSEGV at 21:08:45 (stack not decoded; may be a
  consequence of losing its helpers).
- Lead, unverified: the helper imports sandbox_init_with_parameters; Darling's (src/sandbox/sandbox.c)
  returns 0 but also sets *errorbuf to "Not implemented". On macOS the error buffer is only set on
  failure. Chromium's source (from memory, not checked in this binary) treats a non-null error as failure.

## 2026-10-04: sandbox_init clean success (run14-sandbox) - hypothesis disproved

Change (local placeholder): superproject 92a567279, src/sandbox/sandbox.c: the three sandbox_init
variants return 0, leave *errorbuf NULL and log "DARLING <fn>: no sandbox is implemented...".
Run 14 (bad boot on attempt 1, caught; good on attempt 2; one run):
- "Failed to initialize sandbox." still printed; GPU helper still aborted (SIGABRT 21:24:15).
- The new DARLING log line appeared 0 times, so the helper never reached sandbox_init_with_parameters.
  The error-buffer explanation was wrong. The change stays as a contract fix but did not help.
- The helper binds sandbox_init_with_parameters from libSystem (the function changed), and is started
  with --seatbelt-client=134: it reads its sandbox profile from that descriptor first. Its detailed
  messages ("failed to read buffer length", "ParseFromString failed", ...) go to os_log only.
- os_log in Darling goes to ASL (libtrace os_log.c) and nothing in the prefix stores it: no
  system.log, no asl directory. Those messages are lost.
- tools/probes/spawntest: a child spawned with POSIX_SPAWN_CLOEXEC_DEFAULT plus addinherit_np or
  adddup2 does receive and read a pipe on fd 134/140. So plain descriptor inheritance works.
- Main Electron process died with SIGSEGV at 21:24:14, one second BEFORE the helper aborted and
  earlier in its own log than in run 13 (20 lines vs 31). So that crash is not caused by the helper
  dying, and it varies between runs. Not decoded.
- 4 bad boots in ~17 now.

## 2026-10-04: run14 coredumps read (no code change)
- Helper (620107) memory contains the formatted message "SeatbeltExecServer: failed to read buffer
  length." and none of its sibling messages: the helper's first read of its sandbox rules failed.
- Main Electron process (620063) died first: SIGSEGV in objc_msgSend+0x29, selector `retain`,
  receiver 0x7f4836716820 (not mapped), return address +[NSAppearance setCurrentAppearance:]+0x24.
- cocotron AppKit/NSAppearance.m: currentAppearance returns the static sCurrentAppearance without
  retaining; setCurrentAppearance: releases the old value. A caller that saves the current
  appearance, sets another, then restores the saved one hands back a freed object.
- Inference: the helper's failed read is end-of-file on the pipe because its parent had died.

## 2026-10-04: NSAppearance lifetime fix (run15-appearance)

Change (real fix, local): cocotron c93f48c9; superproject d4332433c. +currentDrawingAppearance (and so
+currentAppearance) returns [[x retain] autorelease]. tools/probes/appeartest: FAIL before
("remembered appearance is still alive after switching away"), PASS after (2 of 2).

Run 15 (good boot first try, one run):
- "Failed to initialize sandbox." is gone. "DARLING sandbox_init_with_parameters: no sandbox is
  implemented..." logged twice: both helpers read their rules and passed the sandbox call. So run 14's
  failed read was the dead parent, and the sandbox placeholder is now exercised.
- Main Electron process: SIGSEGV at 21:36:41, a new place. Fault in Electron Framework code with
  rax=0; rdx is the selector "contentRectForFrameRect:"; nearest return address on the stack is
  AppKit -[NSWindow initWithContentRect:styleMask:backing:defer:]+0x2fa. Electron is creating its
  window; Darling's NSWindow init appears to call back into Electron's window subclass, which then
  reads a null pointer. Not yet read in source.
- GPU helper and network helper: SIGTRAP at 21:36:41 and :42 (Chromium ends itself this way on a
  failed check or when its parent goes away). Not decoded; same second as the main crash.
- No refused memory requests, no unsafe stubs, server up, backend exit 0.

## 2026-10-04: NSWindow initializer fix (run16-window-init)

Change (real fix, local): cocotron d7b10d34; superproject 1de98c313. -[NSWindow
initWithContentRect:styleMask:backing:defer:] uses the style-mask class methods for both conversions
instead of the overridable instance methods. tools/probes/wintest: FAIL before (2 override calls during
setup), PASS after (0; frame 400x324 and content 400x300 unchanged).

Run 16 (good boot first try, one run):
- Electron created its window: ElectronNSWindow receives AppKit messages; its log shows "Displaying
  splash screen from default-splash.html" and "Loading page from http://127.0.0.1:43427/index.html".
- Renderer helpers spawned for the first time (2). Six helper processes passed the sandbox placeholder.
- All helpers ended with SIGTRAP (GPU x2, network x2, renderer x2), the first GPU helper at 21:49:41,
  before the main process. Main logged "GPU process exited unexpectedly: exit_code=133" and "Network
  service crashed, restarting service". GPU helper trap is in Electron Framework code early in its
  main; no Darling frame names the reason. Not diagnosed.
- Main process: SIGSEGV at 21:49:42 on a secondary thread in strlen(NULL), called from Electron code.
  The placeholder SCErrorString (returns 0) was called twice in this run; a NULL from it being passed
  to strlen is the likely cause (inference, call site not decoded).
- New unimplemented/tolerated: BridgedContentView sortSubviewsUsingFunction:context: (x16),
  ElectronNSWindow setShowsResizeIndicator:/keyViewSelectionDirection/isOnActiveSpace/
  setTitleVisibility:/occlusionState, NSSpellChecker dismissCorrectionIndicatorForView:,
  +[NSColor currentControlTint], NSAccessibilityRemoteUIElement (2), +[CATransaction
  addCommitHandler:forPhase:], -[CALayer setAutoresizingMask:], isFullKeyboardAccessEnabled.
- No refused memory requests, no unsafe stubs, server up, backend exit 0.

## 2026-10-04: real SCErrorString (run17-scerror), and the helper trap decoded

Change (real, local): configd 05756b5; superproject a088cf876. SCErrorString and its message table
copied from SystemConfiguration.fproj/SCD.c into the built stubs.c; placeholder removed.
tools/probes/scerrtest: 5 of 5 codes return text.

Run 17 (bad boot on attempt 1, caught; one run):
- Main process no longer dies in strlen(NULL). It now logs "SCDynamicStoreCreate failed with Error:
  1001 - Failed!" (x15, retrying each second), confirming the placeholder was the cause.
- Main stayed up about 6 s. Every helper trapped at startup: 23 helper coredumps (GPU, network,
  renderer), exit code 133. Chromium then gave up: "GPU process isn't usable. Goodbye." (its own
  deliberate fatal exit, SIGTRAP in the main process). Electron's log: "Maximum attempts reached (3)
  for window 2. Quitting Riot Client."
- Helper trap decoded by disassembly of Electron Framework (x86_64 slice): the trap at 0x6941ab is in
  the function that names itself "ContentMainRunnerImpl::Initialize"; it is reached when the call to
  0x4b8f5b0 returns false, and that function calls sandbox_check. So the helper checks "am I
  sandboxed?" after its sandbox call and ends itself on "no". Darling's sandbox_check always returns 0.
- 5 bad boots in ~19.

## 2026-10-04: direction change
The learner chose to build a real sandbox before continuing the Electron bring-up, instead of having
sandbox_check agree with the placeholder sandbox_init. Electron work is paused at run17. Chromium's
embedded profiles: 2, both deny-by-default, about 144 rule lines each set (see progress notes for the
operations and filters used). No design chosen yet.

## 2026-10-04: sandbox step 1, profile reader (no run of the Riot Client)

Change (local): superproject 92515ba89. src/sandbox/profile.c + profile.h evaluate a sandbox profile
(a Scheme program) with its parameters into a flat rule list. TinyScheme 1.42 vendored unmodified in
src/sandbox/tinyscheme (sha256 of the archive in README.darling). Built into libsystem_sandbox for
i386 and x86_64 with hidden visibility: 0 new exported symbols, 23 sandbox_* exports unchanged.
Nothing calls the reader yet; no behaviour change.

Check: src/sandbox/test/profile_test.c on the host, 18 of 18, also clean under ASan/UBSan (the only
reports were the test's own unfreed error strings, since fixed).
Chromium's real profiles (common + each process-specific blob, guessed parameters): common alone gives
68 rules; 9 of 11 combinations evaluate (68-121 rules). The 2 that fail do so with "string-length:
argument 1 must be: string": one needs a parameter I did not supply, the other is a fragment that is
probably not combined with the common profile this way. Not reader bugs as far as I can tell.
Allowed file rules in the common profile by filter: path 15, subpath 11, require-all 4, regex 3.
Runtime sanity after install: boot ok on attempt 2 (bad boot 1), memtest and wintest PASS.
Deviation from the checkpoint: an unknown word in call position is not evaluated at all (so rules
written inside it are dropped), and any rule with an argument the reader does not understand is
dropped whole. The checkpoint only said "logged and skipped".

## 2026-10-04: sandbox step 2+3, Landlock enforcement (run18-landlock)

Change (local): superproject 8df0dc853. src/sandbox/landlock.c applies a profile's allow file rules
(path, literal, subpath) as a Landlock ruleset to the whole process, plus seven logged entries Darling
itself needs (/usr/lib, /usr/share, /System/Library, host /proc, /sys, /dev, /dev/shm).
sandbox_init / sandbox_init_with_parameters call the reader then Landlock and fail with a real error
otherwise; sandbox_check(getpid(), NULL) answers from what actually happened. Named profiles keep the
logged placeholder. A path with extra conditions (require-all) is allowed by its path, flagged.
Not enforced and reported per process: non-file rules, deny rules, regex/other filters, single
directories, paths that do not exist.

Check: tools/probes/sbtest 13 of 13 on one boot: allowed file reads; file outside the profile, write
to a read-only-allowed file, and file creation outside are refused with EACCES; a thread started
before the sandbox and a forked child are confined; broken profile fails with a message and confines
nothing; "am I sandboxed?" goes no -> yes; process still reaches darlingserver.

Run 18 (good boot first try, one run, about 3 minutes):
- 24 processes applied a real Landlock sandbox from Chromium's profiles ("ABI 10, all threads").
  No helper trapped on the sandbox check any more.
- GPU helper: now reaches graphics start-up and fails there: "EGL display query failed with error
  EGL_BAD_PARAMETER", "eglInitialize Default failed with error EGL_BAD_DISPLAY", "Exiting GPU process
  due to errors during initialization" (x17), then SIGSEGV (x3 dumps). Whether any of this is caused
  by files the sandbox refuses is NOT known; there is no unsandboxed run at this stage to compare.
- Network helper: "[FATAL:udp_socket_posix.cc(164)] Check failed: Function not implemented (78)":
  a socket call Darling does not implement (ENOSYS). Also "DNS hosts watch failed to start".
- Renderer helpers: SIGABRT x4, not decoded. "Failed to allocate IOSurface" x2 in the main process.
- Main process stayed up about 3 minutes, then "Splash screen is closed" and a deliberate exit.
- 18 processes logged the unknown profile word "debug".

## 2026-10-04: graphics start-up and the network helper, investigation (no code change)

Run 19 (per-rule sandbox logging): the high count of skipped "missing path" rules is explained; they
are macOS paths that do not exist in a Darling prefix (/System/Cryptexes, /dev/dtracehelper,
/Library/Managed Preferences, ...). The app bundle, including Electron's Libraries folder, is allowed
for reading. Not a translation fault.

Network helper: Electron imports change_fdguard_np and guarded_close_np. Darling's syscall table has
441 guarded_open_np, 442 guarded_close_np, 443 guarded_kqueue_np, but not 444 change_fdguard_np, so it
returns ENOSYS (78), matching "[FATAL:udp_socket_posix.cc(164)] Check failed: Function not implemented".
(That Chromium guards its UDP socket there is from memory of its source.)

Graphics, tools/probes/eglprobe (loads Electron's bundled ANGLE libEGL/libGLESv2 by path):
- They load, and EGL lookups resolve to Electron's own libraries (an earlier guess that Darling's
  Mesa wrapper was answering was wrong).
- ANGLE lists backends: metal, opengl, vulkan, swiftshader.
- default and opengl: display created, eglInitialize fails (EGL_NOT_INITIALIZED).
- vulkan: display created, eglInitialize fails.
- swiftshader (software): works, EGL 1.5, "ANGLE 2.1.19731".
- metal: no display.
tools/probes/metalprobe: MTLCreateSystemDefaultDevice returns nil, 0 devices. Cause:
src/external/metal/src/Metal/MTLDevice.mm gates Metal behind DARLING_ENABLE_METAL=1 on purpose
("still under active development"). Metal is built in this tree (libindium, libmslc present).
With DARLING_ENABLE_METAL=1: default Metal device = "AMD Radeon RX 6650 XT (RADV NAVI23)", 1 device.
ANGLE's metal backend then throws: -[MTLDeviceInternal supportsFamily:] unrecognized selector
(called from EGL_GetPlatformDisplay). MTLDevice.mm has neither supportsFamily: nor supportsFeatureSet:.
Apps see macOS 10.16.0 from NSProcessInfo while sw_vers prints 26.0.

## 2026-10-04: what ANGLE's Metal backend asks of the device (experiment in the probe only)

tools/probes/eglprobe can add -supportsFamily: to MTLDeviceInternal at run time (PROBE_FAMILIES), inside
the probe process only; no Darling code changed.
- ANGLE asks exactly one family question: supportsFamily:2002 (MTLGPUFamilyMac2).
- "No" (also what discovery mode gives): ANGLE creates no Metal display.
- Yes to Common1 (3001) and Mac1 (2001) only: same, no display. So a baseline-only answer does not
  open the Metal path.
- Yes to Mac2: display created; next stop is -[MTLDeviceInternal registryID] unrecognized selector.
The fork's Metal header (src/external/metal/include/Metal/MTLDevice.h) documents that
-supportsFamily: is deliberately not declared: Vulkan has nothing that maps to Apple's GPU families
and any answer would be a claim about hardware that indium cannot back.
Existing guard syscalls (441-443) ignore guards entirely: guarded_open_np is open, guarded_close_np
is close, and both write 0 into the caller's guard value.

## 2026-10-04: change_fdguard_np (run20-fdguard)

Change (local): xnu 004c0f4 (branch fix/sigexc-no-tlv-in-dyld); superproject b8b386122. Syscall 444
added beside 441-443; like them it tracks no guards. Returns EBADF for a closed descriptor and
reports the descriptor flags. tools/probes/guardtest: FAIL before with errno 78, PASS after (6 of 6).
Run 20 (good boot, one run, Metal not enabled):
- "[FATAL:udp_socket_posix.cc(164)] Check failed" no longer appears.
- Network helper still dies, now SIGSEGV x2 ("Network service crashed, restarting service" x2). Not decoded.
- Renderer helpers: SIGSEGV x3, SIGABRT x1 (exit 139 reported by Electron). Not decoded.
- GPU helper unchanged, as expected with Metal off: EGL initialise fails, x25.
- Electron: "Maximum attempts reached (3) for window 2. Quitting Riot Client."

## 2026-10-04: ANGLE's Metal backend initialises on the AMD GPU (probe experiment, no Darling change)

eglprobe with DARLING_ENABLE_METAL=1, the probe answering yes to supportsFamily:2002 only, and
DARLING_TOLERATE_UNRECOGNIZED_SELECTORS=1:
  get display: ok; initialize: ok, EGL 1.5; vendor "Google Inc. (AMD)"; ANGLE 2.1.19731.
ANGLE also asked supportsFamily for 1003, 1005, 2001, 1002, 1001, 1006 (all answered no).
Missing methods tolerated on the way: -[MTLDeviceInternal registryID],
-isDepth24Stencil8PixelFormatSupported, -newLibraryWithSource:options:completionHandler:.
No rendering attempted yet. The tolerated async shader compile never calls its handler back.

## 2026-10-04: Metal step 1, ANGLE starts on Darling's Metal for real

Change (local): indium 8752574, metal 341a133 (branches local/league-metal-angle), superproject 86bc68c21.
MTLDevice gains registryID (Vulkan vendor<<32 | device ID), isDepth24Stencil8PixelFormatSupported
(vkGetPhysicalDeviceFormatProperties), newLibraryWithSource:options:completionHandler: (synchronous
compile, then the handler), and supportsFamily: as a labelled local experiment: NO unless
DARLING_METAL_CLAIM_MAC2=1, then YES for MTLGPUFamilyMac2 only.

eglprobe, no discovery mode, no probe-side help:
- DARLING_ENABLE_METAL=1 alone: ANGLE creates no Metal display, and no exception is raised.
- plus DARLING_METAL_CLAIM_MAC2=1: display ok, initialize ok (EGL 1.5), off-screen surface ok, context
  ok, make current ok. Renderer string:
  "ANGLE (AMD, ANGLE Metal Renderer: AMD Radeon RX 6650 XT (RADV NAVI23), 10.16)", OpenGL ES 2.0.
- First draw call (clear) stops on -[MTLTextureInternal iosurface] unrecognized selector.
- With that one tolerated (answers nil), the probe dies with SIGSEGV straight after. Not decoded:
  that manual run did not have DSERVER_LOG_LEVEL=info set, so the fault registers were not logged.
Milestone 1 (ANGLE starts on Metal on the AMD GPU) reached. Milestone 2 (a frame drawn and read
back) not reached.

## 2026-10-05: Metal milestone 2, first half, and a shader-compile hang that is not Metal's

Changes since the last entry (local, branches local/league-metal-angle): metal a3dc512 (render-pass
attachments without a texture are unused), 9207844 (texture-to-buffer copy with options), b54feb5 +
indium (synchronizeResource: for buffers via vkInvalidateMappedMemoryRanges); superproject 2c8df342e.
eglprobe on ANGLE's Metal backend (DARLING_ENABLE_METAL=1, DARLING_METAL_CLAIM_MAC2=1):
  "ok   clear to red, read back: 255 0 0 255" - the first pixel through Darling Metal -> Vulkan ->
  RADV on the AMD Radeon RX 6650 XT and back.
Then the probe's shader compile never returned. Run 1 lasted 14 min before I aborted it: one ANGLE
worker thread at 100% CPU inside libGLESv2 with Darling's libsystem_m __exp10 as the leaf; main
thread waiting; indium's poll thread in vkWaitSemaphores (normal). Disassembly: the frame returning
into exp10 is ANGLE's text-to-float parser (minus sign, exponent clamp at 38, "Float overflow"
nearby); its caller sits in the GLSL lexer region ("Illegal use of reserved word").
Same shader on ANGLE's SwiftShader backend (no Metal involved): clear ok, then the same hang (killed
by a 60 s watchdog). So the hang is in ANGLE's shader front end under Darling, not in the Metal path,
and it would stop Chromium's GPU helper on any backend. Cause not yet found.
Tool notes: no perf on this host; gdb cannot attach (ptrace_scope 1); kill -ABRT on the guest probe
plus eu-readelf -n on the core gives per-thread registers, which corelib.py then names.

## 2026-10-05: root cause of the shader-compile hang: Darling's __exp10 is an infinite loop

Installed libsystem_m.dylib (x86_64), symbol ___exp10 at 0x5b20: push rbp; mov rbp,rsp; pop rbp;
jmp ___exp10. ___exp10f converts to double and calls ___exp10, so both hang.
tools/probes/exp10test: pow(10,2)=100 prints, then __exp10f(2) spins at 100% CPU (killed at 15 s).
ANGLE's NumericLexFloat32OutOfRangeToInfinity (chromium/5359 util.cpp) ends with pow(10, exponent),
which clang on Darwin targets emits as a call to __exp10; so every shader float literal hangs.
Chromium's GPU helper would hit this on every backend (SwiftShader run hung identically).
Likely cause (to confirm in source): exp10 implemented as pow(10.0, x) and the compiler folding that
pow(10, x) back into __exp10, producing a self tail-call.

## 2026-10-05: exp10 fixed (superproject 9a066e021); shaders compile; the next wall is mslc

Fix: src/libm/Source/exp10.c built with -fno-builtin (as sincos.c is) and __exp10f uses powf.
tools/probes/exp10test 5 of 5; installed ___exp10 now jumps to _pow.
eglprobe after the fix:
- Metal backend (AMD, DARLING_METAL_CLAIM_MAC2=1): clear ok; vertex and fragment shaders compile;
  program link FAILS: "mslc could not compile the source: unexpected character '?' at offset 199".
  ANGLE's MSL uses the ternary operator, #include <metal_stdlib>, #define/#if, #pragma, metal::array
  templates and attributes. mslc (src/external/metal/deps/mslc) is 4,755 lines, a lexer with about
  24 token kinds, no preprocessor, no include, no templates. MTLCompileOptions is a hollow class.
- SwiftShader backend (software, no Metal): clear ok, shaders ok, link ok, triangle pixel-exact.
  Milestone 2 reached on the software path; on the Metal path it is blocked by mslc.
- ANGLE's own Vulkan backend: display created, eglInitialize fails; ANGLE's bundled Vulkan loader
  looks for ICDs via VK_ICD_FILENAMES / vulkan/icd.d; Darling ships no Vulkan dylib; the host has
  RADV (radeon_icd.json). Darling's wrap_elf (src/native) wraps host ELF libraries as Mach-O dylibs.
Display path note: run 18 logged "Failed to allocate IOSurface of size 1920x1032"; Chromium presents
frames (software or GPU) through IOSurface + CALayer, so that is a wall of its own after shaders.

## 2026-10-05: route B, step 1: the AMD driver is reachable from Mac code; ANGLE wants VK_EXT_metal_surface

Change (local): superproject 326b738fb: src/native wraps libvulkan_radeon.so as
/usr/lib/native/libvulkan_radeon.dylib (wrap_elf) and installs usr/share/vulkan/icd.d/radeon_icd.json.
tools/probes/vkicdtest: loads the wrapped driver, negotiates loader interface v7, vkCreateInstance ok,
2 physical devices: "AMD Radeon Graphics (RADV RENOIR)" and "AMD Radeon RX 6650 XT (RADV NAVI23)",
Vulkan 1.4.354. The driver advertises 23 instance extensions (xcb/xlib/wayland/headless surfaces,
no metal surface).
eglprobe, ANGLE vulkan backend, with EGL_KHR_debug listening: "Internal Vulkan error (-7): A requested
extension is not supported, RendererVk.cpp initialize:1594". ANGLE's Mac display (DisplayVkMac.mm)
names VK_EXT_metal_surface as its window-system extension and RendererVk requires it at instance
creation; RADV has no such extension.
Run 21 (exp10 fixed, Metal off): unchanged outcome; GPU helper fails EGL init (Default backend ->
no Metal device), Chromium falls back to swiftshader-webgl, GPU helper SIGSEGV x3, renderers SIGABRT
x4, Electron ends with window-all-closed after ~3.5 min. Not decoded.

## 2026-10-05: MILESTONE: ANGLE renders on the AMD GPU through Darling via Vulkan (route B)

Changes (local): superproject fd67abf6d (src/native/vulkan/icd_shim.c: driver shim advertising
VK_EXT_metal_surface, manifest points at it), 1b91bf637 (shim withholds VK_KHR_fragment_shading_rate),
12e5e4ee2 (shim resolves real functions against the loader's instance).
eglprobe, ANGLE vulkan backend, no Metal involved:
  renderer: ANGLE (AMD, Vulkan 1.4.354 (AMD Radeon RX 6650 XT (RADV NAVI23) (0x000073EF)), radv-26.2.4)
  GL version: OpenGL ES 3.1.0; clear red read back 255 0 0 255; vertex+fragment compile ok; link ok;
  triangle middle 0 255 0 255 and corner 255 0 0 255, pixel-exact.
Two obstacles on the way, both now understood:
1. ANGLE's Mac Vulkan display requires VK_EXT_metal_surface at instance creation (DisplayVkMac.mm,
   RendererVk.cpp VerifyExtensionsPresent); RADV lacks it. The shim advertises it and strips it
   before the driver; Apple-platform surfaces get VK_ERROR_SURFACE_LOST. Labelled pretence, offscreen only.
2. ANGLE (this Electron's build) crashed calling a null function pointer after instance creation. The
   crashing slot is in libGLESv2's __common at image+0xfa0cc0, adjacent to the slots the binary fills
   for vkCmdSetFragmentShadingRateKHR (+0xfa0cc8) and vkGetPastPresentationTimingGOOGLE (+0xfa0cd0),
   i.e. vkGetPhysicalDeviceFragmentShadingRatesKHR; the call is guarded by
   pipelineFragmentShadingRate == VK_TRUE. The loader does provide that function (vkloadertest); the
   binary's loading routine called right before stores a *different* name into a different slot
   (vkGetPhysicalDeviceExternalSemaphorePropertiesKHR -> +0x99e890), so ANGLE fills one variable and
   calls through another. Any driver offering VK_KHR_fragment_shading_rate triggers it; MoltenVK and
   SwiftShader do not, so macOS never saw it. Workaround: the shim withholds that extension; ANGLE's
   loader view shows 228/244 device extensions, fragment_shading_rate absent.
Tools: vkicdtest (wrapped driver directly), vkloadertest (through ANGLE's exported loader),
eglprobe --debug callback (EGL_KHR_debug). ANGLE's libGLESv2 exports its built-in Khronos loader
1.3.231 (vkGetInstanceProcAddr, vkCreateInstance, ...), which made the loader testable in isolation.
Sandbox: own-needs grown with host /usr/lib, /usr/lib64 (read+exec), /usr/share, /etc (read) for the
wrapped host libraries (local commit after this entry).

## 2026-10-05: run22 (ANGLE steered to Vulkan) and the GPU helper's sandbox crash

Run 22 (RC_GUEST_ENV ANGLE_DEFAULT_PLATFORM=vulkan VK_LOADER_DEBUG=error,warn; env confirmed present in a
helper's core): two populations of GPU helpers.
- 6 helpers (pids 781665..781840) died with SIGSEGV inside libsystem_sandbox: TinyScheme opexe_0
  (OP_EVAL) <- Eval_Cycle <- scheme_load_string(init.scm) <- evaluate_once, pass 2, before any graphics.
- Other helpers (781730..781827, no cores) logged "eglInitialize Default failed with error
  EGL_NOT_INITIALIZED" x21, then "Exiting GPU process". Those were Chromium's fallback launches
  (--use-angle=swiftshader-webgl appears on 3 of 6 cores). Not yet explained; eglprobe with the same
  env and ANGLE's "default" backend renders the triangle on the RX 6650 XT.
Root cause of the sandbox crash, found on the host with the extracted GPU profile: data-dependent
(BUNDLE_ID "x" crashed, the real id did not; any single missing parameter is a clean error instead),
only with two evaluation passes, gone under MALLOC_PERTURB_. scheme_init_new() mallocs the
interpreter unzeroed and scheme_init() leaves fields unset, so pass 2 inherited pass 1's freed
memory. Fix: calloc + scheme_init (superproject, after 5fab68c60). Host -O2 test: 3/3 clean, suite PASS.
Also learned: Chromium applies its GPU sandbox after GL initialisation on macOS, but these helpers
never printed EGL errors, so with the crash gone the hardware helper's EGL result is still unknown.

## 2026-10-05 (morning): runs 23-31, Electron stays up; the wall is now IOSurface

Start: run23 (f871132a8, analysed this morning). End: superproject 43f518955, all local, build
complete (`ninja -n`: no work), installed from a clean tree each time.

Fixed, each found from a core or a trace and re-run:
| # | Symptom | Cause | Fix | Check |
|---|---|---|---|---|
| 1 | Renderers abort x3, Electron quits (run23) | Electron calls 4 weak imports unchecked; first hit `CGFontRenderingGetFontSmoothingDisabled` | cocotron 6973140c (that one says "disabled": Onyx2D draws grayscale text only; `CGPreflightScreenCaptureAccess` no), CoreAudio `AudioDeviceDuck` unsupported, CarbonCore `_CSCheckFixDisable` no; superproject 11359b6ea, 227f0dbd7 | scan2level: weak-when-called 4 -> 0; run24 no renderer aborts |
| 2 | Network helper SIGSEGV (run23) | `guarded_close_np/open_np/kqueue_np` wrote 0 through the caller's `const` guard; Chromium's guard is in read-only memory | xnu 13d28ad | guardtest: exit 139 before, 7/7 after |
| 3 | Main Electron process SIGILL (run24) | `fileport_makeport`/`fileport_makefd` still called the removed kernel module (deliberate trap) | darlingserver: server keeps the descriptor behind an IKOT_FILEPORT port using XNU's own ipc_misc.c, closes it on no-senders; xnu calls it | fptest 8/8 (incl. release closes the file) |
| 4 | Main Electron process SIGSEGV (run25) | `accept(fd, NULL, NULL)` dereferenced the NULL address (libuv) | xnu, superproject 1be4f4537 | run26 passes that point (no separate probe) |
| 5 | Backend (RiotClientServices) SIGSEGV (run26) | queued mouse-moved event delivered to its splash window after `orderOut:`; handler uses an object destroyed with the splash. Core: window `_isVisible` 0, `_acceptsMouseMovedEvents` 1 | cocotron: `-[NSWindow sendEvent:]` drops mouse-moved for a window not on screen; superproject fb27cb272 | run27-31: no backend crash (the trigger depends on where the mouse is, so this is weak evidence) |
| 6 | Every GPU helper: "eglInitialize Default failed" | Not ANGLE at all. Chromium looks `egl*` up through libGLESv2's handle first; that search includes dependencies -> Darling's OpenGL.framework -> wrapped HOST libEGL. strace: helper opened /etc/glvnd/egl_vendor.d, scanned DRM, was refused the X authority file | OpenGL.framework loads the host EGL privately (dlopen RTLD_LOCAL) instead of linking it; 88457944e | eglwho: libGLESv2 handle -> host libEGL before, "not found" after (as on macOS); cgltest unchanged before/after |
| 7 | iokitd SIGTRAP every run | `IOSurfaceRoot::createSurface` did `CFRelease(NULL)` when the request did not parse | iokitd (branch local/league-iosurface-crash), superproject 43f518955 | NOT exercised yet: after fix 6 no run has reached an IOSurface request |

Where it stands (runs 27-31): backend, Electron main, crash handlers, network helper, renderers and
one GPU helper all stay up; the page loads (`ux_launch_to_electron_loaded` ends); splash closes
normally; in run27 the main window lived 4.5 min until closed. No coredumps in runs 29-31.
Screenshots (rc-shot.sh): main window 1536x864 = the client's background colour #121212 only, and an
unpainted strip at the bottom; splash 600x576 = black. No page content reaches a window.

Corrections to earlier entries:
- `ANGLE_DEFAULT_PLATFORM=vulkan` never applied to Chromium. With ANGLE actually reached (run29), the
  GPU helper asks for display types "OpenGL" (ANGLE on CGL: "Could not create the CGL context") and
  "OpenGLES" (EGL_NO_DISPLAY), never "default". The Vulkan route B result holds for eglprobe only.
- "Chromium applies its GPU sandbox after GL initialisation": not supported by anything; ignore.
- The EGL failures in runs 18-28 were the host EGL, so earlier readings of them say nothing about ANGLE.

The wall: frames reach a Mac window as IOSurfaces set on CALayers.
- Before fix 6 the software-fallback GPU helper logged "Failed to allocate IOSurface of size
  1536x864" / 600x600 (runs 27, 28).
- Darling's IOSurface.framework is a client of an `IOSurfaceRoot` service in iokitd. That service is
  a 2020 skeleton: create builds an object and returns nothing; lock, unlock, lookup, use counts and
  both Mach-port methods return "not supported". iosurftest: IOSurfaceCreate -> NULL ("Cannot obtain
  IOSurfaceRoot!").
- cocotron QuartzCore: layer-backed views render through CALayerContext/CARenderer (OpenGL) and only
  understand CGImage contents; CAContext and CALayerHost (what Chromium's GPU path uses to show
  layers from another process) are stubs that log.
- Darling's own CGL: CGLChoosePixelFormat ok, CGLCreateContext fails 10004 in cgltest (before and
  after fix 6). ANGLE-on-CGL needs that too.

Unknown: what mode the surviving GPU helper is in after fix 6 (no "GL is disabled" line, no IOSurface
requests in 45-75 s); why SocketManager in the UI logs "Socket closed" every 30 s (run27); a
launchservicesd SIGILL seen once while running iosurftest (not looked at).

Tools added: tools/decode.sh (crash -> registers + stack), tools/rc-shot.sh (bounded run, window
screenshots, process list, optional strace of the whole container), probes fptest, eglwho, cgltest,
iosurftest; guardtest extended; rc-run.sh honours RC_ALLOW_RUNNING.

## 2026-10-05 (later): runs 32-54, the Riot Client's page reaches its window

Start: superproject 43f518955. End: 0e4791db8 (clean tree, build complete, installed). All local.

Result: stock Electron 22.3.27 shows its page in a Darling window (electron-t3 software path,
electron-t5-hw-nowait default path: a solid red page, pixel colour correct). In the Riot Client
(run54) the 1536x864 main window appears, the GPU helper creates surfaces of 600x576, 600x600 and
1536x864, the browser looks them up from their ports and sets them as layer contents; 182 frames
handed over in 120 s. The window shows only the client's background #121212 (2 colours in the
screenshot): the page runs but its WebSocket to the backend fails ("[WebSocket] event: ERROR",
"CLOSED (1006)", "SocketManager onReconnectFailed"). Not investigated.

What was wrong, in the order found:
| # | Symptom | Cause | Fix | Check |
|---|---|---|---|---|
| 1 | No surface could exist | IOSurface.framework was a client of iokitd's unfinished IOSurfaceRoot | A surface is one Mach memory entry (header page + pixels); the entry's port is the surface's port; the opener copies and checks the header once and relies on the map being refused when the entry is smaller than claimed (af6f33a8b) | iosurftest 24/24, incl. lying header refused |
| 2 | Layers could not show a surface | CARenderer drew CGImage contents only | BGRA surface uploaded as the layer's texture on every draw (cocotron ff306a17) | electron-t3 screenshot |
| 3 | Electron said its shown window was not visible; nothing asked for a frame | `-[NSWindow occlusionState]` missing; then present but exactly 2: Electron's isVisible() treats "== Visible" as occluded, and macOS always adds bit 8192. No NSWindowDidChangeOcclusionStateNotification either | cocotron c4729db6, aab26f78, 465ae3fd | test app: `visible=false` -> `visible=true`, ready-to-show fires |
| 4 | Chromium's second GPU helper hung at start, browser waited for it forever (default, non-software path) | libkqueue's Mach-port filter read the kqchan notification with a blocking recvfrom while holding the kqueue lock; two threads woke for one notification, the loser blocked for good and everyone else queued on the lock. Core of the aborted helper (run45, pid 191913): one thread in evfilt_machport_copyout, another in psynch_mutexwait under kevent64_impl | read no longer waits, event dropped when there is nothing (libkqueue 458101b, branch local/league-machport-nowait) | electron-t4-hw: `loading=true` forever; t5-hw-nowait: page loads. Riot run52 -> run53: main window appears |
| 5 | Without the discovery variable Electron died on missing methods, one per run (runs 32-40) | missing in cocotron | NSScreen localizedName (RandR output name), NSColor controlAccentColor (system blue) and currentControlTint (blue), CATransaction addCommitHandler:forPhase:, NSApplication isFullKeyboardAccessEnabled (NO), NSAccessibilityRemoteUIElement (no tokens), CALayer autoresizingMask (real, proportional), NSWindow titleVisibility (stored only) | each run got one call further |

Corrections:
- Runs 32-40 ran WITHOUT DARLING_TOLERATE_UNRECOGNIZED_SELECTORS=1, unlike runs 23-31; their
  crashes are not regressions. Run33 used a stale install (build had failed) and is marked INVALID.
- "The wall is IOSurface" (this morning) was half the story: with IOSurface working nothing asked
  for one, because of 3 and 4.
- Still tolerated in discovery mode (run53/54): none logged. Still "unimplemented": BridgedContentView
  sortSubviewsUsingFunction:context:, setShowsResizeIndicator:, isOnActiveSpace (answers YES), LMGetKbdLast,
  CGColorSpaceCopyICCData.

Open:
- Riot UI WebSocket to the backend fails (above). securityd SEGV in run54 looks like shutdown (a
  SIGTERM precedes it); not decoded (decode.sh failed on it: no register lines).
- Stock Electron renderer aborts when a page uses text: NSFontManager -> NSFontFamily ->
  `[NSDisplay currentDisplay]` needs a window server, and the sandboxed renderer is refused the X
  authority file (electron-t2). Font listing should not need a display connection. Design not chosen.
- CVDisplayLinkStart is a TODO that reports success and never calls back. Not hit by Chromium 108 here.
- A layer-hosting view's layer may get its contents replaced by AppKit's own drawing (not verified).
- 24-pixel light strip: at the top of the test window, at the bottom of the Riot window (content
  offset by a title bar height). Not looked at.
- Screenshots: windows were opening mostly off-screen; rc-shot.sh now moves a window to 0,0 first.

Tools added: tools/cycle.sh (refuses a dirty tree or unfinished build, installs, checks a symbol,
runs), tools/callcount.py and whocalls.py (gdb on a live guest: count calls into / name callers of
a Darling function, no rebuild), tools/threads.sh (all threads of a core), tools/electron-test.sh
with ../electron-22.3.27 (stock Electron, checksum verified, one-window test app; FRAME_TEST_HW=1
keeps Chromium's default GPU path; gives --v=1 logs the Riot Client cannot). DARLING_IOSURFACE_TRACE=1
logs surfaces made, ported, opened and set on layers.

## 2026-10-05 (evening): why the Riot UI's WebSocket fails - certificate trust never answers

No code changed. Superproject still 0e4791db8.

Chain, each link observed:
1. Riot UI: "[WebSocket] event: ERROR / CLOSED (1006)" about 30 s after each attempt (run54).
2. The backend's listener is fine: host `curl -k https://127.0.0.1:<app-port>/` answers HTTP 401 (run57).
   The Electron main process's five ECONNREFUSED (run55 strace) were attempts before it listened.
3. Stock Electron, nettest app (electron-n2): http fetch reaches the server (CORS refusal = an
   answer); every https/wss request, including https://example.com, gets no answer in 20 s.
4. Test TLS server log (electron-n7): client hello arrives, server sends its certificate and
   Finished, the client never answers. So the stop is at certificate verification.
5. Browser process enters SecTrustEvaluate (callcount, n9) and SecTrustGetResult is never called.
6. tools/probes/trusttest (30 lines, no Chromium): SecTrustCreateWithCertificates 0, then
   SecTrustEvaluate never returns (30 s).
7. Security's client (OSX/sec/ipc/client.c:338) talks to `com.apple.trustd.agent` when uid != 0 and
   to `com.apple.trustd` when root. Guest uid is 1000. lookuptest: com.apple.trustd kr=0;
   com.apple.trustd.agent kr=1102 "Unknown service name" (0.00 s). Darling's launchd loads
   LaunchDaemons only; the agent plist sits in LaunchAgents, unloaded.
8. `trustd` has never been exec'd in this prefix's dserver.log (0 times; securityd 121). After
   `launchctl start com.apple.trustd`: status 3, still no exec. Its plist asks for UserName _trustd.
   Cause of status 3 not established (the user switch in non-root mode is the suspect).

So two defects, plus one unknown:
- libxpc: a connection to an unknown service leaves a synchronous send waiting forever; on macOS the
  caller gets a connection-invalid error.
- Nothing provides com.apple.trustd.agent.
- Unknown: whether trustd can run at all here, and whether its evaluation works once reachable.

Not the cause, checked: DNS. getaddrinfo, CFHost and res_nquery all resolve in a plain process
(dnstest, hosttest), curl in the container reaches clientconfig.rpg.riotgames.com. The backend's
"Could not resolve host" lines (every run since run3) are unexplained and separate.

Mistakes made while investigating, for the record:
- I sent SIGABRT to pid 2191 believing it was the guest's network helper. It was the network helper
  of the host's Mullvad VPN GUI (/usr/lib/electron42, matched by `--type=utility`); Chromium
  restarted it (pid 258558). Guest processes are now selected by /proc/<pid>/exe ending in mldr.
- A `pkill -f` pattern matched my own shell and killed two of my commands.

Tools added: tools/retval.py, tools/livebt.py (gdb; both weak: gdb's unwinder cannot follow Mach-O
frames), tools/threads.sh scans deeper; probes dnstest, hosttest, trusttest (+testcert.der),
lookuptest; electron-22.3.27/nettest app and srv/ (self-signed cert; http.server 18080 and
`openssl s_server -accept 127.0.0.1:18443 -WWW -state` must be started by hand).

## 2026-10-05 (late): certificate trust, corrected chain and state at stop

Corrections to the section above: libxpc does NOT hang on an unknown service (xpctest: "Connection
invalid" in 0.00 s), and Security does NOT use com.apple.trustd.agent here: it picks com.apple.trustd
whenever launchd's manager name is "System" (client.c securityd_in_system_context), which it is.
Real causes, in order found:
1. trustd died at start: -[NSDictionary writeToURL:error:] missing. Fixed (corefoundation,
   foundation; superproject "writeToURL:error:" bump).
2. launchd could not start trustd: its plist says UserName _trustd, launchd (guest uid 0) calls
   getpwnam, the account does not exist, child _exit(ESRCH) = the "status 3". Accounts come from
   <prefix>/private/etc/{passwd,master.passwd,group}, written by src/startup/darling.c (~line 3234)
   with root and the host user only. NOT FIXED IN CODE. Experiment only: I appended a _trustd
   (uid/gid 282) line to those three files in ~/.darling-league-run2; with it trustd runs and
   trusttest gives SecTrustEvaluate status 0, result 5 in 0.21 s (right for a self-signed cert).
3. trustd aborted when going to the network: NSURLSessionConfiguration lacked
   _sourceApplicationAuditTokenData. Fixed (cfnetwork branch local/league-urlsession-audit-token).
4. OPEN: trustd next aborts on +[NSURLComponents componentsWithURL:resolvingAgainstBaseURL:]
   (missing), and also segfaults (not decoded). So a real chain (https://example.com) still gets no
   answer, while the self-signed server answers in 0.4 s and Chromium raises certificate-error.
Also open: Electron main sometimes segfaults at start in CFRunLoopAddSource with a NULL source
(n13, n15, n16; caller unknown); dserver.log in the prefix is 1.5 GB at info level.
Riot Client not re-run since these fixes.

## certificate trust fixed; next stop is font listing (2026-10-05, superproject 313060857)

Runs electron-n17..n22, riot-client-55. Tools added: `tools/corebt.py` (fault registers and stack
from a core alone), `tools/probes/urltest` (trustd's Foundation/CFNetwork calls, per-step pools),
`ET_PRE` in `electron-test.sh`.

| Symptom in trustd | Cause | Fix (local commit) |
|---|---|---|
| abort, `+[NSURLComponents componentsWithURL:resolvingAgainstBaseURL:]` | class was empty | foundation: real class for scheme/host |
| SEGV in `objc_release` at pool drain | libxpc pipe calls returned an autoreleased dictionary that callers release | libxpc: retain at the three hand-over points |
| zombie: `objectForKey:` to freed `__NSCFDictionary` in `-[TrustURLSessionDelegate contextForTask:]` | libtrustd compiled without ARC, properties were `assign` | security: `-fobjc-arc` on its ARC sources |

How the last two were found: `NSZombieEnabled=YES` in a copy of trustd's launchd plist in the test
prefix (restored afterwards). `launchctl setenv` does not work in the guest ("Socket is not
connected").

Result: no trustd cores. Self-signed HTTPS: `certificate-error` in 0.4 s. `https://example.com`:
`ERR_CERT_AUTHORITY_INVALID` after more than 20 s (not investigated). Riot Client: the UI reaches its
backend, starts drawing text, and every renderer aborts on "Failed to connect to a window server"
during font listing; after 3 renderer crashes the client quits. A relative `DARLING_INSTALL_PREFIX`
gives "Timed out waiting for shellspawn"; use an absolute path.

## runs 56-63: renderer past fonts, main process stays up (2026-10-05)

| Run | First stop | Fix |
|---|---|---|
| 55 | renderer: "Failed to connect to a window server" in font listing | cocotron: font listing as NSDisplay class methods |
| 56 | renderer: CFRetain(NULL) after `-[NSFont printerFont]` | returns self |
| 57 | renderer: `-[O2Font_freetype copyTableForTag:]` abstract | FreeType tables and tags |
| 58-59 | renderer: Chromium out-of-memory trap; server: "cannot share a range that only partly overlaps" | memory entries made of pieces; xnu S2C control length |
| 60 | main: CFDictionaryGetValue(NULL) after CTFontCopyTraits | traits from the font |
| 61 | renderer: same trap, "touches 8 or more shared regions" | no piece limit |
| 62 | main: NULL weak import `MPNowPlayingInfoPropertyElapsedPlaybackTime` (logged as "fault inside the handler for signal 13", exit 137) | new MediaPlayer framework |
| 63 | main alive, window open and dark; GPU helper SEGV in ~AudioConverter (garbage this), two renderer crashes | open |

How the trap in 58 was identified without symbols: `corebt.py <core> 0 220` disassembles before the
fault; the function stores its argument in a global and traps, which is Chromium's out-of-memory
exit; the size (0x20000) matched the server's refusal line for that pid. `riot-client-*/dserver.log`
holds lines of older runs too: filter by pid.

## runs 64-65, electron-text1: first Riot page content (2026-10-05)

Electron publishes breakpad symbols per release; the x86_64 slice of Riot's Electron Framework has
the same LC_UUID as stock 22.3.27, so `corelib.py` now names its functions. With names, the run 58
out-of-memory trap is `viz::bitmap_allocation::AllocateSharedBitmap`: Chromium creates a region,
maps it, and asks for a read-only entry from the mapped address. darlingserver answered that from
records of mappings the guest had long unmapped. Fix: inserting a record drops or trims older
records in its range (`memtest` reuse case).

Run 65: Riot window shows the dark page and the red loading spinner (`riot-client-65/window-0.png`).
One renderer crash (PartitionAlloc freelist corruption), GPU helper crash in audio decoder teardown.
`electron-text1`: stock Electron draws text, gradient and a moving square, no cores.

## 2026-10-06: bad boots root-caused; renderer heap corruption reproduced in stock Electron (no code change)

**Bad boots ("services NOT reachable", Electron start crash in PowerMonitorDeviceSource).** In non-root
mode two shellspawn processes start on every boot: the launcher's own (`spawnShellspawn`,
`src/startup/darling.c`, parent 1, recorded in `.shellspawn.pid`) and launchd's job
(`org.darlinghq.shellspawn`, RunAtLoad). Both run `unlink(socket); bind(socket)` on the same path
(`src/shellspawn/shellspawn.c` setupSocket), so the later one owns the socket. The launcher's copy
is not a child of launchd and has no bootstrap port; a guest started through it prints
`bootstrap_port = 0x0` and every lookup returns 268435459 (MACH_SEND_INVALID_DEST). Snapshot of a
bad boot: pids 49493 (parent 1) and 49509 (parent launchd 49494), both `/usr/libexec/shellspawn`.
Rate: 0 of 32 boots after small probes, 4 of 7 boots that follow an Electron run. Origin: fork
commits 5025863bb and 778352dfe (on origin/master).
The "first XPC message after boot fails" finding of 2026-10-05 did not reproduce: 14 of 14 cold
`xpctest com.apple.trustd` answered in under 0.1 s. Not explained; possibly it was a bad boot.
`bstest` lists six names nobody provides here (wrong name or no such daemon); only its
notification_center, launchservicesd and SecurityServer lines mean anything.

**Renderer heap corruption.** Reproduces in stock Electron 22.3.27 in under 30 s with
`electron-22.3.27/atobapp` (`ET_APP=atobapp`; `ATOB_WORKERS`, `ATOB_MAIN`, `ATOB_KIND=atob|str|math`).

| Run | Main thread | Worker | Renderer |
|---|---|---|---|
| atob2-w0 | atob | none | clean, 60 s |
| atob7 | idle | arithmetic only | clean, 40 s |
| atob9 | atob | arithmetic only | clean, 40 s |
| atob1 | atob | 4 x atob | SEGV in worker (StringResource8 with impl = -1) |
| atob3-w1 | atob | atob | trap, PartitionRoot::Free under window btoa (main thread) |
| atob4 | idle | atob | SEGV, PartitionBucket::SlowPathAlloc under StringCache |
| atob6 | atob | strings, no atob | trap, PartitionRoot::Free under window btoa (main thread) |

One run per row. Cores of riot-client-79: a freelist cycle (slot ...a00 -> ...a40 -> ...a00) and a
free of a slot already on the freelist, both in the 32-byte bucket: double frees inside Chromium's
allocator. Ruled out: in-place conversions of private memory (0 logged in run 79), S2C signals on
the crashed threads (0), os_unfair_lock exclusion (`locktest` PASS), per-thread storage through
%gs, pthread_getspecific and thread_local (`tlstest` PASS, 8 threads x 6 generations). Cause not found.

Other: browser process SEGV in objc_msgSend from `ui::TextServicesContextMenu::IsSpeaking`
(`isSpeaking`), once (electron-atob11). New probes: pmtest, pstest2, tlstest.

## 2026-10-06 (later): one shellspawn per container (superproject c8f753ad0)

Change: the launcher no longer forks its own shellspawn in non-root mode (`spawnShellspawn` and its
three calls removed from `src/startup/darling.c`); when the socket refuses a connection it retries
rather than deleting the socket file; `shellspawn` exits at start when `bootstrap_port` is null.
Shutdown's `.shellspawn.pid` handling is untouched and now idle.

| Check | Result |
|---|---|
| 10 rounds of: Electron run (12 s), shutdown, fresh boot, `bstest` | 10 of 10 reach notification_center |
| shellspawn processes per boot | 1, parent is launchd, every round |
| `darling shutdown` after each | exit 0, no process left with the install's mldr as exe |
| shellspawn started by hand through mldr/vchroot in a running container | prints the guard line, exit 1, socket inode and mtime unchanged, `bstest` still passes |

Before: 4 bad boots of 7 following an Electron run. Loop script was in the session scratchpad
(not kept). Not tested: the reconnect-retry path (shellspawn dying while the container stays up).

`riot-client-riot-client-81` (150 s): boot check passed on attempt 1. Login page renders with all
letters present (window-0.png). No `-202` lines in RiotClientElectron.log. Two renderer cores:
113490 SEGV (not decoded), 113728 TRAP in FreelistCorruptionDetected under blink atob, rax
0x2020202020202000 (same value as run 79; 0x20 is an ASCII space).

## 2026-10-06 (afternoon): renderer heap corruption root-caused and fixed (xnu 23353f1, superproject ebb98e073)

**Cause.** `libsystem_kernel.dylib` exported weak, empty `os_unfair_lock_lock` and `os_unfair_lock_unlock`
(`file_handle.c`, `workq_kernreturn.c`). Electron Framework imports all three lock calls from "libSystem";
dyld bound lock and unlock to the empty ones and trylock to libsystem_platform. PartitionAlloc
(`SpinningMutex`: trylock spin, then lock; unlock) therefore locked once and never again.

**Evidence.**
- Core 122677 (electron-hc-1135-base): Electron Framework's lazy pointers: trylock -> libsystem_platform,
  lock and unlock -> libsystem_kernel. Main thread, a worker and a third thread trapped within 6 ms in
  FreelistCorruptionDetected with identical values, same PartitionRoot (flags: no thread cache), same
  lock; lock word 0x307 (main thread's port), no-waiters bit still set.
- Core 113728 (riot-client-81): slot 181 of a 32-byte span held a live StringImpl while the span's
  metadata said head = slot 181, 181 allocated: one pop lost.
- `probes/locksymtest`: lock/unlock from `dlsym(libSystem.B.dylib)` came from libsystem_kernel and left
  the word 0. FAIL before, PASS after.

| Reproducer (atobapp, 60 s) | Result |
|---|---|
| nothing inserted, old build | renderer gone 5 of 5, before round 100 |
| one of lock / trylock / unlock swapped | still crashes (1 run each) |
| all three swapped (`lockswap-ALL`, `lockaudit`) | renderer survives 4 of 4 |
| nothing inserted, fixed build | 5 of 5 clean, rounds 500 |

`--js-flags=--single-threaded` and `--jitless` did not prevent it (1 run each, old build).

**Fix.** Stubs removed; saved FSRefs, audit info and parked workqueue threads use `libsimple_lock`.

**Riot Client** `riot-client-riot-client-82` (180 s): boot check attempt 1, 0 cores, login page with the
cursor in the username field.

Refuted hypotheses and their probes: stack-address main-thread detection (`stacktest`), lock inside
thread-exit destructors (`exitlocktest`), shared per-thread slots (core), in-place memory conversion
(0 log lines). The 0x2020202020202000 value of earlier notes is PartitionAlloc's own debug padding.
Side finding, not investigated: an inserted library that interposes a function and calls it hangs
(its own call appears to be interposed too).

## 2026-10-06 (later): windows repaint; four crashes met on the way (superproject 117323c81)

Tool: `tools/paint-test.sh <label>` runs stock Electron with `electron-22.3.27/paintapp` (60 fps counter,
moving box, text field, a getVoices call) and reports screen pictures, frames per surface, lookups.

| paintapp, 35-45 s | before | after |
|---|---|---|
| page's own frame rate | 60 fps | 60 fps |
| distinct pictures in 16 screen grabs | 1 | 16 |
| frames per surface (GPU helper) | 1043 into one | 960 / 964 over two |
| frames the browser took (lookups) | 4 | 1886 |
| typed "hello" visible on screen | no (page got it) | yes |

Cause: no use count on a displayed surface (IOSurfaceIsInUse always false), so Chromium reused one
buffer and viz dropped the unchanged CALayerParams. Fix: cocotron 24e732d5.

Crashes found once frames and input flowed, each fixed:
| Where | Cause | Fix |
|---|---|---|
| browser, TextServicesContextMenu::IsSpeaking / TtsPlatformImplMac::GetVoices | NSSpeechSynthesizer catch-all stub returns nothing; garbage used as an object | upstream picks + cocotron e29b101f, f9363b47 |
| browser, dyld halt on first key press | TISCreateASCIICapableInputSourceList exported C++-mangled (own bug, 0f19657ae) | 6d5017494 |
| browser, LSCopyDefaultApplicationURLForURL | `*outError` tested with a NULL pointer | a6597cc3d |
| darlingserver, knote_post calls NULL | callback read twice, cleared in between by another thread | darlingserver 6e11421 |

`riot-client-riot-client-88` (150 s, no input): 0 cores, 10 Riot processes alive, 2065/1984 frames over
two surfaces, browser took 3192. Runs 84-87 ended early on the crashes above; their script-made
screenshots are of the desktop and mean nothing.

## 2026-10-06 (afternoon): clicks, pointer, and concurrent name lookups (superproject 65e52a49c)

- Clicks: `-[NSEvent subtype]` raised for mouse events (8 exceptions in run 89), event lost. Pointer:
  `setHiddenUntilMouseMoves:` never showed it again. cocotron b7ce9482. Run 90/91: 0 exceptions;
  hCaptcha solved with the mouse in run 91.
- "Unable to sign in" (runs 90, 91): backend log `Failed to create RSO authorization (512)
  temporarily_unavailable: Unable to connect to server` at +8 s, `PSL state changed to Error`; later
  `IdentityAuth authorized` but no `Using direct token exchange` (run 89, which reached Home, had it).

| Probe (in the container) | before | after Libinfo 5a8e9d8 |
|---|---|---|
| `dnsmttest 12 1` (one lookup at a time, in a thread) | 0 of 12 fail | - |
| `dnsmttest 12 2` | 24 of 24 fail | 0 of 24 |
| `dnsmttest 12 8` | 94 of 96 fail | 0 of 96 |
| `resmttest` res_query / res_nquery, 4 at once | 36 of 40 / 0 of 40 | - |

Plain `dnstest`, CFHost and res_nquery (`hosttest`) always worked one at a time, cold or warm, which is
why this stayed "intermittent" since 2026-10-04. Cause: `darling-resolver/resolver.c` called `res_query`
(process-wide state); now `res_nquery` with a state per call.

## 2026-10-06 (afternoon): install spinner = fork child deadlock (superproject c3f1943d0)

Run 92 backend log: 0 "Could not resolve host" (12-13 in runs 89-90), `Using direct token exchange`,
then `patch-proxy: PutPriorityPatchRequestV2: product 'league_of_legends'`, install path
`/Applications/League of Legends.app`, `agent: LaunchAgent: Running: list com.riot.riotclient.checkinstalls`
and nothing more. A child of RiotClientServices (same command line, 1 thread, asleep 181 s) was parked in
`psynch_mutexwait` under `kqueue_closed_fd` <- `close$NOCANCEL` <- `uv__signal_global_reinit`
(libdiscord_partner_sdk) <- `_pthread_atfork_child_handlers` <- `libSystem_atfork_child` <- `fork`.
`libsystem/init.c` called `kqueue_atfork()` after the client's handlers; moved before (libsystem 05a65a8).

| `probes/forkkqtest` (4 threads closing files, atfork child handler closes one, 300 forks) | result |
|---|---|
| before | child hung at fork 4 and at fork 6 (2 runs) |
| after | 300 of 300, 3 runs |

## 2026-10-06 (afternoon): League client start (superproject ee829a120)

Runs 93 and 94 (Play): `LeagueClient` SIGSEGV at start, rip 0x0, return address in
`LeagueClient.app/Contents/Frameworks/mvg.framework/mvg`. `baseline/leagueclient-1..5` run the binary
alone (crashes the same way in < 1 s).

| Observation (leagueclient-2, `DYLD_PRINT_APIS=1`) | |
|---|---|
| imports missing by static scan (`scan2level.py`), LeagueClient and mvg | 0 |
| names looked up with dlsym that Darling lacked | `__atomic_store` (and `_dyld_objc_class_count`, asked by the runtime) |
| registers at the fault | rip 0, rdi 0x58, rcx 5: `__atomic_store(88, ..., seq_cst)` |

Fix: compiler-rt 98166216 builds `lib/builtins/atomic.c`. `probes/atomictest`: store/exchange/
compare_exchange lookups NULL before, all four found and correct on an 88-byte object after.
`probes/denyattachtest` (xnu 24e603c, darlingserver db6024a): unknown ptrace request returned 0 and an
attach after PT_DENY_ATTACH was allowed before; EINVAL and refused after.

leagueclient-3, -5 (alone, after the fix): fault at LeagueClient+0x5e1613, `movl %eax, 0x0; ud2` right
after a call taking the string "LCE-CA900FE5": the client's own fatal-error exit. leagueclient-4 (same,
container booted under strace): ran 90 s, no output, no crash.

## 2026-10-06: run 95, League client crash 2 = system_profiler -xml

Build ee829a120. LeagueClient started with real Riot Client arguments and died 6 s later on its main
thread in `CFArrayGetCount(NULL)` (return address LeagueClient+0xb6b6e). The caller runs
`system_profiler SPDisplaysDataType -xml` and parses the output as a property list. Darling's tool
ignored `-xml`. Fixed in superproject 82430c0b7 (`plutil -lint`: invalid -> OK). Client not yet rerun.
