# Learning Progress

## Done: League baseline run (2026-10-04)
Report: ~/.local/share/darling/macos-apps/league/baseline/REPORT.md (local only, nothing posted).
Result, 3 of 3 runs identical: installer loads, links (static scan clean: 18 libs, 509 symbols),
starts, then SIGSEGV ~0.4s in at objc_msgSend+0x1d in libobjc, read of 0x18 (NULL class pointer).
Stage reached: runtime start. Sender and object not yet identified. stderr warns NSConstantArray /
NSConstantDictionary are implemented in both CoreFoundation and Foundation (lead, unproven).
Deviation: used an x86_64 checker because scan-imported-apps.py reads arm64 slices only.
Left on disk: prefixes ~/.darling-league-run1..3 (762 MB each); branch local/league-baseline.

## Done: installer flow (2026-10-04)
Click workaround (learner decision: simple workaround now, real-Mac behaviour later; mechanism
proposed by Claude and approved): cocotron 39deb0d4, NSThemeFrame mouseDown: forwards to the window,
background dragging removed (ponytail comment). Superproject local/league-baseline cbc3b6013.
Verified in one untimed run driven by the learner: Install click worked; installer wrote
/Users/Shared/Riot Games/Riot Client.app (1.2 GB) and exited 0 with "Success!".
Still open from this phase: intermittent hostname lookups; frame-origin jump hypothesis untested;
crash on quit; lsregister missing (non-fatal).

## In progress: Riot Client (Electron) bring-up
Approach (Claude's proposal from the learner's stated priority of reaching a playable game; learner
replied "let's start getting Electron working", taken as agreement, no reasoning given): test
against the Riot Client itself; clear load-time blockers as a batch, then one failure at a time.
Facts so far: Electron 22.3.27, classic binding. Missing symbols Electron imports: 99 required,
54 weak. The backend library (libRiotClientFoundation, dlopen'd with RTLD_NOW) is blocked by
exactly one required symbol: LSSetDefaultHandlerForURLScheme (CoreServices), imported by the bundled
Discord SDK; its other missing import is weak. Darling declares that function in
src/frameworks/CoreServices/include/LaunchServices/LSInfo.h but does not implement it; the sibling
LSCopyDefaultHandlerForURLScheme in src/LaunchServices/LSInfo.m is a stub returning NULL. No handler
database exists in Darling. The Discord SDK has a "Failed to set default handler for URL scheme"
message, so it handles failure. This file is in the superproject, not a submodule.

## Done: LSSetDefaultHandlerForURLScheme placeholder (local only, 2026-10-04)
Learner decision: claim success again. Implemented in the superproject (LSInfo.m), commit 18fb68253
on local/league-baseline: logs the request, returns noErr, ponytail comment. Verified in one run:
the Riot Client backend library now loads ("foundation_loaded"). Not pushed.

## Done: weak NSApplication delegate (local only, 2026-10-04)
Learner decision: weak reference like macOS, for long-term stability. Claude's additions approved:
weak API inside NSApplication.m only, all reads through -delegate, delegate told directly at the
four post sites, observer registration removed. cocotron febdba36; superproject 27a09f2cd. Not pushed.
Verified in one valid run: setDelegate: crash gone; backend ran ~1 s of startup. An earlier run was
invalid because the container had stayed up across the reinstall and served the old AppKit.
Not yet checked: installer crash on quit.

## Done: AVCaptureDevice authorization answers (local only, 2026-10-04)
Learner decision: answer "not asked yet"; reasoning: microphone is only for party voice chat, not
needed for ARAM/Classic. Claude's addition approved: requestAccess answers "not granted".
Superproject commit b2e974652. Verified in one run: backend runs ~20 s, reaches Riot services.

## Done: tray-menu methods (local only, 2026-10-04)
Learner priority: only what the client -> game -> client loop requires. Claude's proposal approved:
NSMenuItem view/setView: storage, NSStatusItem button nil. cocotron 8f66c126; superproject 58270ac0f.
Verified in one run: backend ran 60 s and launched the Electron UI process for the first time.

## Done: load-time batch for Electron (local only, 2026-10-04)
Approved batch: 4 menu/activation methods + 33 load-time symbols. Commits: superproject 6121a1a38,
cocotron edc3cc56, foundation abca9cf35 (branch local/league-baseline-consts). Deviation, done
without a separate checkpoint as the same kind of fix: d0cf1fae5 re-exports NSUserActivity through
LaunchServices, a 34th load-time symbol the flat scan had missed; a two-level scan
(tools/scan2level.py) now reports 0 required load-time gaps in the UI binaries. Also had to link
MediaAccessibility against CoreFoundation. Verified: Electron loads and reaches ElectronMain.

## Done: silent power-notification port (local only, 2026-10-04)
Learner decision: fix in the registration function, silent channel. IOKitUser e63ef13 (branch
local/league-silent-power-port); superproject 25d247087. Verified: Electron passes that call.
May be a workaround for the finding below rather than for a missing registry entry.

## Done: launchd PID limit (local only, 2026-10-04)
Learner decisions: find and fix the non-root lookup failure; then raise the limit in the service
manager (over a PID namespace for non-root mode), reasoning "to get a working League".
Root cause (verified via launchd's debug log): job_new_anonymous refused caller PIDs >= 100000;
non-root mode has no PID namespace and host PIDs are ~500000. Fix: superproject 2dc75710a, limit
4194304 under #ifdef DARLING. Failing test was tools/probes/bstest (9 of 9 refused); after the fix,
over three fresh boots, lookups reach launchd (3 existing services found, 6 unknown names answered
as unknown) and the notify/power-source calls succeed. Run 8: backend exits 0, crash handler
spawns, certificate error gone, lookups failing less. One unexplained null bootstrap port, once.
Open question the learner did not address: apps seeing six-digit PIDs.
The IORegisterForSystemPower silent-port workaround (IOKitUser e63ef13) should be re-examined now
that services are reachable.

## Done: discovery mode for unrecognized selectors (local diagnostic, 2026-10-04)
Learner decision: make a missing method non-fatal. Claude's design approved: opt-in env variable
DARLING_TOLERATE_UNRECOGNIZED_SELECTORS, log once per class+selector, return zero, default
unchanged, local only (upstream comments forbid working around the raise). corefoundation 14fb4ab;
superproject ad7df3053. Verified with tools/probes/seltest in both modes.
First use (run10): Electron reached thermalState and three NSScreen methods (EDR values,
colorSpace), then aborted on a missing lazily bound C function, CGDisplayUsesForceToGray.

## Open: intermittent boot with no bootstrap port (non-root mode)
2 of ~12 container boots; cause unknown; not investigated. tools/rc-run.sh checks with bstest and
reboots the container, recording each bad boot. Needs root-causing later.

## Done: Electron batch with real answers and discovery placeholders (local, 2026-10-04)
Learner chose "Both parts". Real: thermalState nominal, NSScreen EDR 1.0 and sRGB colorSpace,
CGDisplayUsesForceToGray false, vDSP_vsadd corrected and built, plain-copy __atomic_load.
Placeholders: 61 lazily bound functions that log their name and return zero (not implementations).
Commits: superproject a6bb118c9 plus a follow-up pointer bump; cocotron 60f289ab; corefoundation
67d515f; foundation 91263c086; configd 36deb82 (branch local/league-discovery-placeholders).
Verified: two-level scan of Electron reports 0 required symbols missing.

## Done: memory entries, simple flavour (local only, 2026-10-04)
Learner decision (own reasoning): brand-new regions first for a working prototype, then patch what
Chromium asks for. Claude's additions approved: named entry holding the memfd descriptor; map via
task_map_file; null-handle map allocates; other flavours log and return an error; one probe.
darlingserver 018a3e0 (branch local/league-memory-entry); superproject c63dc605d.
Verified: tools/probes/memtest 10/10 on one boot; run12: server survived, Electron's JS main process
ran for the first time (static server started), used two new-region mappings, then asked for
read-only entries for EXISTING private memory (0x20000 and 0x28 bytes), was refused, and trapped.
Ceiling: memfd kept until the task dies. Not tested: handle passed between processes.

## Done: memory entries for existing memory, live view (local only, 2026-10-04)
Learner decision: live view via in-place conversion (after asking for Claude's thinking). Claude's
additions approved: shared helper dtape_map_share_region, whole pages, overlap abort -> logged error,
read-only enforced, VM_COPY refused, probe extended. Deviation added while implementing, not in the
checkpoint: recorded shared entries are dropped when the region is private again (stale after a
guest unmap). darlingserver a4b692b; superproject 5cf3bb914.
Verified: memtest 21/21 on one boot; run13: no refused requests, server up, Electron reached its
watchdog start and spawned GPU and network helper processes for the first time.

## Done: sandbox_init clean success (local placeholder, 2026-10-04) - did not fix the stop
Learner decision: clean, logged success; helpers run unsandboxed (chosen after Claude laid out the
honest-failure vs success consequences; learner's first answer was "Not implemented?").
Superproject 92a567279. Run14: helper still prints "Failed to initialize sandbox." and the new log
line never appears, so the call is never reached; Claude's error-buffer explanation was wrong.

## Done: NSAppearance currentAppearance lifetime (real fix, local, 2026-10-04)
Found by following the learner's suggestion to look wherever the crash was recorded (coredumps).
Fix proposed by Claude as a standard repair, approved: getter returns retain+autorelease.
cocotron c93f48c9; superproject d4332433c. appeartest FAIL before, PASS after. Run15: sandbox
failure gone (helpers pass the placeholder sandbox call), main crash moved to window creation.

## Done: NSWindow initializer avoids overridable conversions (real fix, local, 2026-10-04)
Learner's first idea: check the renderer is ready first (right shape, wrong object; Claude explained
two-stage construction). Learner then chose option A of three Claude offered (style-based
calculation), reason given: "best and most stable". Both calls changed (Claude's addition, approved).
cocotron d7b10d34; superproject 1de98c313. wintest FAIL before, PASS after. Run16: Electron creates
its window, shows its splash, loads its page, spawns renderer helpers.
Note for Claude: learner said "I am really trying here" after repeated open questions on ObjC
internals. Use choices first in Darwin/ObjC territory, as the profile says.

## Done: real SCErrorString (local, 2026-10-04)
Learner chose it ("Replace placeholder please"); Claude proposed copying Apple's function from
SCD.c; approved. configd 05756b5; superproject a088cf876. scerrtest 5/5. Run17: strlen(NULL) crash
gone; main process stays up until Chromium quits over its helpers.

## Paused: Riot Client (Electron) bring-up (2026-10-04)
Paused by the learner's choice at: helpers trap because sandbox_check says "not sandboxed" after the
placeholder sandbox_init reported success. Option A (consistent pretend answer) was offered and NOT
chosen. Resume point: run17, superproject a088cf876.

## In progress: real sandbox for Darling (learner's direction, 2026-10-04)
Learner decisions: real sandbox before continuing Electron; enforce in the Linux kernel; file rules
through Landlock first; unenforceable rules ignored with a log line; TinyScheme to run profiles
("sounds like the correct choice"). Claude's three-step route presented; learner did not comment on
it and approved step 1.
Step 1 DONE (local): profile reader, superproject 92515ba89. Host test 18/18; Chromium's common
profile -> 68 rules. Not called by anything yet.
Claude's additions inside step 1 beyond the checkpoint: unknown words are never evaluated in call
position and any rule touching something not understood is dropped whole (never widened).

## Done: sandbox steps 2 and 3, Landlock enforcement (local, 2026-10-04)
Learner decisions: allow an approximate rule by its path; Darling adds its own needs and logs them.
Claude's additions approved at the checkpoint; superproject 8df0dc853. sbtest 13/13. Run18: 24
processes really confined; helpers pass the sandbox check and fail later on other things (GPU: EGL
init; network: a socket call returning ENOSYS; renderer: abort, not decoded).
Added while implementing, not in the checkpoint: per-category summary lines by default with
DARLING_SANDBOX_VERBOSE for per-rule lines (the learner asked for a log line per ignored rule);
single-directory rules (path/literal on a directory) are not allowed rather than widened;
macOS symlinks resolved with realpath before translating.
Unknown: whether the GPU/renderer failures are caused by sandbox refusals. No comparison run exists.

## Pending decision
Decision: fixes for the network helper (change_fdguard_np) and the Metal path (supportsFamily:).
Stage: guard call DONE (xnu 004c0f4, superproject b8b386122; guardtest FAIL->PASS; run20: the UDP
fatal check is gone, network and renderer helpers now SIGSEGV, not decoded). Metal family -> Build
checkpoint with new facts, awaiting the learner's choice among A/B/C (asked 2026-10-04).
Learner decisions: (1) add the fourth guard call alongside the existing three; (2) option A, yes to
baseline GPU families only. New facts that undercut (2): ANGLE asks only about Mac2 (2002) and
refuses Metal unless that is yes; baseline-only gives no display. The fork's header documents that
supportsFamily: is deliberately left out. After a yes to Mac2 the next gap is -registryID.
Learner (2026-10-04): "Lets get Metal to Vulkan working 100%." Did not pick A/B/C; Claude reads it as
the Metal route and proposes A (opt-in switch) for the family answer unless told otherwise.
Probe experiment: with Metal on, yes to Mac2 and three missing methods tolerated, ANGLE's Metal
backend initialises on the AMD GPU (EGL 1.5, vendor "Google Inc. (AMD)").
Metal step 1 DONE (approved with the opt-in switch): indium 8752574, metal 341a133, superproject
86bc68c21. Milestone 1 reached without probe tricks: renderer "ANGLE (AMD, ANGLE Metal Renderer: AMD
Radeon RX 6650 XT (RADV NAVI23), 10.16)". Next gap at the first draw: -[MTLTextureInternal
iosurface]; with it tolerated the probe SIGSEGVs (not decoded). Awaiting the learner at a Build
checkpoint on how to proceed through the Metal gaps (asked 2026-10-04).
Learner decision (2026-10-04): approach B. Per-step approval is paused for the stretch to Metal
milestone 2 (one frame drawn off-screen and read back correctly), under these rules: answers the
driver/implementation can truthfully give are implemented for real; anything that would be a guess
or a pretence stops and goes to the learner; Claude reports at milestone 2 or at the first real
design question. Scope: src/external/metal (and indium). Local commits only. This does not cover
other areas, pushes, or placeholders.
Learner decision (2026-10-05): route B, ANGLE's own Vulkan backend on RADV through a wrapped host
driver and environment steering; display wall (IOSurface/CALayer) after. Step 1 DONE (approved): wrap_elf of libvulkan_radeon.so + manifest, superproject 326b738fb;
vkicdtest proves the wrapped driver works (both AMD GPUs listed). ANGLE's Vulkan backend still fails:
it requires VK_EXT_metal_surface (Mac WSI) at instance creation; RADV lacks it. Shim approved and DONE (fd67abf6d, 1b91bf637, 12e5e4ee2). MILESTONE 2026-10-05: ANGLE's Vulkan
backend renders the probe pixel-exact on the RX 6650 XT through Darling (radv-26.2.4). Under B along
the way: withheld VK_KHR_fragment_shading_rate (ANGLE bug: fills one global, calls another); fixed
my own shim bug (lookups without an instance); sandbox own-needs grew by host /usr/lib, /usr/lib64,
/usr/share, /etc. Run22 with ANGLE_DEFAULT_PLATFORM=vulkan: GPU helpers crashed in my sandbox reader (TinyScheme
interpreter allocated unzeroed; second pass inherited stale fields) - fixed with calloc+scheme_init
(2026-10-05). Fallback helpers failed eglInitialize (EGL_NOT_INITIALIZED), unexplained. Next: run23.
Earlier note: ANGLE's Metal shaders need a real MSL
compiler; Darling's mslc cannot parse them (no preprocessor, no ?:, no templates). Software path
(SwiftShader) now renders the probe's triangle correctly. Options to put to the learner: software
first; ANGLE Vulkan backend on RADV via wrapped host ICD + env steering; grow mslc / find an MSL
front end. Also flagged: IOSurface allocation fails in Darling (display wall after shaders).
Deviation under B: exp10 fix is in src/libm, outside the agreed Metal scope; a real one-line fix
with existing precedent (sincos.c), reported.
Root cause found 2026-10-05: Darling's libsystem_m ___exp10 is a self tail-call (infinite loop);
clang turns pow(10,x) into __exp10 on Darwin, so ANGLE's float lexer hangs on every shader literal.
Not Metal-specific. Fix belongs in Darling's libm (src/external/libsystem or libm). Under B this is
plumbing: a real fix, no pretence; will implement once the source is located.
Progress under B (2026-10-05): three more Metal fixes committed (texture-less attachments skipped,
texture-to-buffer copy with options, synchronizeResource for buffers). Milestone 2 half reached: a
clear is drawn and read back correctly through Metal->Vulkan->RADV. Blocked at shader compile: an
ANGLE front-end thread spins forever in its GLSL float lexer under Darling; reproduces on
SwiftShader too, so it is a Darling libc/libm-level issue, not Metal. Investigating with ANGLE's
source at b3a6010a958f. Still under B: this is plumbing, not a design question, unless the fix
would need a Darling behaviour change with trade-offs.
What was in that checkpoint:
supportsFamily: behind an opt-in switch; registryID; isDepth24Stencil8PixelFormatSupported from
Vulkan; newLibraryWithSource:options:completionHandler: via the existing synchronous compile;
eglprobe extended to render one frame off-screen and read it back.
Milestones Claude proposed toward "working": ANGLE initialises -> one frame rendered and read back
-> Chromium's GPU helper stays up -> pixels reach the window -> client UI visible.
Learner direction (2026-10-04): leave the sandbox as is; work on graphics start-up and the missing
socket call. Stated milestone: Vulkan on their AMD GPU through Darling displaying the League client.
Facts: see REPORT.md "graphics start-up and the network helper". Key ones: Darling's Metal is gated
off unless DARLING_ENABLE_METAL=1 (deliberate, in MTLDevice.mm); with it on, the device is the
learner's "AMD Radeon RX 6650 XT (RADV NAVI23)"; ANGLE's metal backend then needs
-[MTLDevice supportsFamily:], which does not exist. Syscall 444 change_fdguard_np is missing while
441-443 exist. tools/probes/eglprobe and metalprobe give a fast loop without the Riot Client.
Sandbox follow-ups left open: own-needs list is a first guess; service/network rules unenforced.

## Done: bug-fix stretch, runs 23-31 (local, 2026-10-05 morning)
Learner instruction (background session): "Lets get a working League client rendered in darling!
Fixing any problems that it finds." Claude's reading, not confirmed by the learner: fix defects that
have one evident repair without a checkpoint each; stop at anything that needs a design.
Seven fixes, all found from cores/traces and re-run; table in REPORT.md "runs 23-31". Superproject
43f518955. Outcome: every Electron process stays up, page loads, windows appear but are blank.
Choices Claude made alone inside that reading (learner may want to revisit):
- CGFontRenderingGetFontSmoothingDisabled answers "disabled" (Onyx2D has no LCD text);
  CGPreflightScreenCaptureAccess "no"; AudioDeviceDuck "unsupported"; _CSCheckFixDisable "no".
- File ports implemented in darlingserver the same way as the memory entries (server holds the fd).
- Mouse-moved events are dropped for a window that is not on screen.
- OpenGL.framework loads the host EGL with dlopen instead of linking it.
Corrections: ANGLE_DEFAULT_PLATFORM=vulkan never reached Chromium (it asks for ANGLE's OpenGL
backend by name); the "EGL init fails" findings of runs 18-28 were the host's EGL, not ANGLE.
iokitd fix (CFRelease(NULL)) is not exercised yet.

## Done: frames reach the window (2026-10-05 afternoon, local, superproject 0e4791db8)
Learner asked for options, then said "Alright. Lets get started!" without naming one. Claude took
that as accepting its recommendation (B: surface = Mach memory entry, software frames first); the
learner gave no reasoning of their own, so no understanding is recorded. No Implementation
checkpoint was asked before coding; the learner may want that step back.
Outcome and evidence: REPORT.md "runs 32-54". Stock Electron shows its page; the Riot Client's main
window appears and receives frames; its page is blank because its WebSocket to the backend fails.
Choices Claude made alone (learner may revisit): controlAccentColor/currentControlTint say blue;
isFullKeyboardAccessEnabled NO; accessibility tokens nil; occlusionState reports 8194/8192 like
macOS and ignores windows covering it; surface IDs are pid-based, no lookup by ID; attachments are
per process; a surface is re-uploaded on every draw; libkqueue drops a Mach-port event when its
notification was already read.
Introduced by Claude, not yet explained back by the learner: level-triggered wake-ups and why a
blocking read under a lock deadlocks; why a receiver must not trust a shared header.
Open decisions for the learner: (1) font listing without a window server (sandboxed renderers);
(2) what to chase next: the WebSocket failure, or text in sandboxed renderers.

## In progress (2026-10-05): Riot login page shows on first load
State at riot-client-77 (superproject "security: host roots parse"): the Riot login page renders on the
first load with text, no crash in any process for 170 s (riot-client-77/window-0.png).
Learner decisions (2026-10-05): system trust anchors = the HOST's CA store; provided by answering in
the Security functions (not by generating macOS's files).
Implemented: while /System/Library/Keychains/SystemTrustSettings.plist is absent, the system domain of
SecTrustSettingsCopyCertificates / CopyTrustSettings / CopyUnrestrictedRoots answers from the host CA
bundle under /Volumes/SystemRoot (121 roots here; probe anchortest). Cause it fixed: Chromium verifies
against that list, it was empty, every HTTPS connection got ERR_CERT_AUTHORITY_INVALID (-202), and the
page stayed on the spinner.
Facts found on the way: the launcher (ensureKeychains in src/startup/darling.c) already writes
SystemRootCertificates.keychain from the host bundle, but Darling's Security reads no certificate
out of that file (`security find-certificate -a` lists none); SecTrustEvaluate via trustd accepts
Riot's chain (probe trusttest <host> <der>...); trustd still uses Apple's 2020 list, so the two paths
can disagree (learner was told; feeding trustd from the host store is a later step, not decided).
Earlier fixes this stretch: glyph boxes (CTFontGetBoundingRectsForGlyphs; the renderer heap corruption
has not recurred in 4 runs), AudioConverter reference validation, TISCreateASCIICapableInputSourceList,
PROCMAP_QUERY region lookups.
Done on the learner's word (2026-10-05): (1) deleted 87 cumulative dserver.log copies from run folders
before riot-client-71 (du said 81.8 GB; free space did not change, so on this btrfs they shared
extents with the prefix log, which is kept and is 4.2 GB); (2) launcher adds the _trustd account
(uid/gid 282) to passwd, master.passwd and group at every start when missing (superproject
"startup: every prefix has the _trustd account"); checked on the old prefix with the hand-added
lines removed (added once, not twice) and on a fresh prefix.
New fact, not fixed: the FIRST XPC message to an on-demand launchd service after a boot fails at once
with "Connection invalid" (probe xpctest com.apple.trustd: cold fails, warm answers in 0.09 s), although
bootstrap_look_up succeeds cold (probe lookuptest). So the first SecTrustEvaluate of every boot returns
kSecTrustResultInvalid in 0.00 s. No trustd core. Cause not found (suspects: what happens to the
client's send right or its queued message when launchd hands the receive right to the started job).
Open, not fixed:
- 8 connections per run still fail with -202; hosts not identified.
- SecTrustCopyAnchorCertificates returns -50 in the probe (its per-root SecTrust loop), not looked at.
- Web-font text on the login page misses or swaps some letters (S, E, T: "ign-in", "US. RNAM.").
- Learner report "slow, repaints super slowly": not pinned down. Server idle on the spinner (gdb
  samples, ptrace_scope now 0); not yet measured on the login page.
- Intermittent start crash (PowerMonitorDeviceSource -> CFRunLoopAddSource(NULL)); TSM document
  property placeholders; 24-px strip; backend "Could not resolve host"; 77 GB of old log copies in
  baseline/ (deletion awaits the learner); _trustd account design awaits the learner.
Tools/probes added: sample.py, anchortest, trusttest (chains in probes/chains), locktest, timertest;
test apps textapp, videoapp.

## Facts: runs 78-80 read back (2026-10-06, no code change)
Runs 78-80 (2026-10-05 22:40-22:55, superproject 46851bee0 then 0244fb85d) had no notes. Read back:
- Renderer heap corruption is NOT gone. Run 79 (with "FreeType face locking"): three renderer traps,
  two in PartitionAlloc (FreelistCorruptionDetected with rax 0x2020202020202000, and
  PartitionRoot::Free) under blink atob, one in VideoFrameSinkBundle::FlushMessages. Run 78: two
  SIGSEGV and one SIGTRAP, not decoded. Run 80: no cores, app exit 1, shot-*.png are 393 bytes.
- Boot check needed 2-3 container boots in each of the three runs.
- 8-9 connections per run still fail with -202.
- riot-client-78/window-0.png is a picture of the terminal, not the Riot window.
- The game itself (League of Legends.app) is not in the prefix: nothing past sign-in is measured.
Learner asked (2026-10-06) "What features are missing?"; answered with a gap list and a Build
checkpoint on what to chase first. Learner chose (no reasoning given): heap corruption and the cold XPC failure.

## In progress: upstreaming to VibeDarling (2026-10-06)
Learner asked what can go upstream; Claude sorted the 82 local commits (sendable / already upstream /
not sendable) by grepping origin/master (26 commits ahead of the branch point). Already upstream:
-[NSImage drawInRect:] (cocotron 55fd6a7b); supportsFamily: partly (metal). Learner decisions: fork
on their profile (ztancrell/darling, remote `fork`); after saying they were anxious about wasting
reviewers' time, send only the exp10 fix first. Posted: issue #1092, claim (2026-10-06T15:06:36Z,
24 h), PR #1093 from fix/libm-exp10-self-call. Evidence: upstream exp10.c compiled standalone with
clang 23.1.1 is `jmp ___exp10` on x86_64, arm64 and i386. Not verified: rebuild on current master;
other compilers (PR #887's test passed somewhere); the Swift test was not run.
Local only, not pushed: fix/launchd-anonymous-pid-limit, fix/opengl-private-host-egl (Claude advised
holding the EGL one back: design chosen without the learner, tested on Electron only).
No reasoning of the learner's own recorded for the sort; choices were made from Claude's options.

## Done: one shellspawn per container in non-root mode (local, 2026-10-06, superproject c8f753ad0)
Learner asked for options, then chose "A with B as a guard" (no reasoning given): A = launcher stops
spawning its own shellspawn; B = shellspawn exits when it has no bootstrap port. Claude's additions,
approved at the Implementation checkpoint: reconnect path retries instead of unlink+spawn; shutdown's
.shellspawn.pid handling left untouched (now idle). Fact found: launchd's job already has KeepAlive.
Verified: 10 of 10 boots that follow an Electron run reach services, one shellspawn each (child of
launchd), shutdown exit 0 with no guest process left; a hand-started shellspawn prints the guard line,
exits 1, socket inode unchanged. riot-client-81: boot check passed on attempt 1; login page renders
with correct text; two renderer crashes remain (below).
Not explained back by the learner: why the copy without a bootstrap port breaks service lookups.

## Done: League client's first crash = missing __atomic_store (2026-10-06, superproject ee829a120)
Learner chose the Darling-side look. LeagueClient reproduces alone in under a second. First guess
(PT_DENY_ATTACH returning garbage) was refuted by a probe: it returned 0 by luck. Actual cause from
DYLD_PRINT_APIS: `dlsym(RTLD_DEFAULT, __atomic_store) ==> NULL`, then a call to it (rip 0, rdi 0x58,
rcx 5). compiler-rt's atomic.c was commented out of the build. Learner approved building it and
"Also fix ptrace".
- compiler-rt 98166216: atomic.c built. atomictest FAIL -> PASS. Superproject de2c76e4b drops Claude's
  earlier plain-copy ___atomic_load stand-in from src/sandbox/sandbox.c.
- xnu 24e603c + darlingserver db6024a: PT_DENY_ATTACH recorded and enforced, unknown requests EINVAL,
  refused PT_ATTACHEXC no longer stops the target. denyattachtest FAIL -> PASS.
After: standalone LeagueClient gets further (597 loader calls, was 471; host Mesa loads) and then stops
in its OWN fatal-error path: `LCE-CA900FE5`, then a deliberate write to address 0, in code next to the
strings "servers.lcds.lcds_host". Claude's reading, not confirmed: it lacks the arguments the Riot
Client passes (--region, --riotclient-app-port, ...), so this is an artifact of running it alone.
Under strace the same binary ran 90 s silently without that stop (not understood; not pursued).
Owed: the learner presses Play through the Riot Client on this build.
Not looked at, by the rule: anything inside mvg beyond which image a return address belongs to.

## Upstreaming, second batch (2026-10-06 evening UTC)
Learner: "upstream today's fixes, empty lock stubs. As long as they are legit fixes". Sent, each
re-checked against the repo's current main (touched files byte-identical to ours before the fix):
- #1094 / darling-xnu PR 62: empty os_unfair_lock stubs.
- #1095 / darling-Libinfo PR 1: resolver state per query.
- #1096 / darling-Libsystem PR 3: kqueue_atfork before client atfork handlers.
Earlier: #1092 / darling PR 1093 (exp10), claim from 2026-10-06 15:06 UTC.
Held back by Claude, reasons given to the learner: CALayer use count (only shows an effect with the
local IOSurface rewrite, which upstream lacks); mouse subtype + pointer (one commit holding two fixes,
needs splitting); LSInfo NULL test (upstream has 6 such sites, only 1 fixed); kqchan callback (seen
once); NSSpeechSynthesizer; non-root launcher/launchd fixes. All four open claims expire about 24 h
after posting and need renewal comments if review runs longer.
Run 94: full 22.9 GB install, Play pressed, LeagueClient segfaulted in mvg again; decision on a
Darling-side look still open (learner chose upstreaming next, did not answer that).

## Milestone and boundary: League client downloaded, started, crashed in mvg (2026-10-06 13:43, run 93)
Install went through after the fork fix: /Applications/League of Legends.app (8.4 GB on disk, patch
progress 3.37 % logged), and the Riot Client launched
`League of Legends.app/Contents/LoL/LeagueClient.app/Contents/MacOS/LeagueClient`. It died about at
start with SIGSEGV; the faulting stack is in `LeagueClient.app/Contents/Frameworks/mvg.framework/mvg`
(obfuscated symbols, nested signal frames). The Riot Client then closed its window and quit, leaving
RiotClientServices and its crash handler behind ("container already running").
Claude's reading, not confirmed: mvg is Riot's protection / anti-cheat component for macOS. The profile
says no anti-cheat bypass and that whether it accepts the environment is out of scope. Put to the
learner as a decision; no analysis of the module was done beyond naming where the crash is.
Upstream question answered the same hour: all of today's fixes are still absent from origin/master
(47f149d82); PR #1093 open, no reviews.

## Done: fork child no longer deadlocks in the kqueue layer (2026-10-06, libsystem 05a65a8, superproject c3f1943d0)
Run 92: signed in cleanly (0 "Could not resolve host", direct token exchange); learner clicked Install
for League; client set /Applications/League of Legends.app, ran `launchctl list
com.riot.riotclient.checkinstalls`, spinner forever. Found: forked child of RiotClientServices asleep
with 1 thread; stack (new tools/livestack.py): fork -> libSystem_atfork_child -> client atfork
handler (libuv in the Discord SDK) -> close -> kqueue_closed_fd -> pthread mutex wait. libSystem
called kqueue_atfork() after the client handlers. Fix approved ("Implement and restart"): call moved
before them. forkkqtest: FAIL by the 4th-6th fork before, 300 of 300 x 3 after.
Not yet verified: the install continuing in the real client.
Seen, not addressed: Darling's `launchctl list <missing label>` prints "launchctl list returned
unknown response" and exits 1 (macOS: "Could not find service", exit 113); an Agent log from
2026-10-04 shows osVersion 10.10.10 for this prefix (League may need a newer reported macOS).

## Done: concurrent name lookups (2026-10-06, Libinfo 5a8e9d8, superproject 65e52a49c)
Learner report: "Unable to sign in", client keeps loading. Logs: sign-in authorised, but the backend's
startup request to the auth server failed ("Unable to connect to server" beside "Could not resolve
host") and it never recovered. Cause proven with probes: darling-resolver used res_query (one shared
state); 2 lookups at once failed 24 of 24, 8 at once 94 of 96, 1 at a time 0. Fix approved at an
Implementation checkpoint ("Implement and restart"): res_nquery with a state per call.
Verified: dnsmttest 0 of 24 and 0 of 96 (one name in the first list has no address at all; replaced).
Closes the "intermittent hostname lookups" item open since 2026-10-04. Not yet verified: sign-in in
the real client on this build.
Clicks and pointer (cocotron b7ce9482) are installed since run 90: 0 mouse exceptions, hCaptcha solved
by the learner with the mouse in run 91. Pointer visibility: learner has not said.
Learner note: wants the client to offer the macOS League download; its log already shows patchline
KeystoneFoundationLiveMac. Vanguard reports "Unsupported operating system" (anti-cheat: out of scope).

## Built, NOT installed or tested: clicks and pointer (2026-10-06, cocotron b7ce9482, superproject 249849b7b)
Learner report after sign-in: pointer invisible, clicks do nothing. Causes read from the live log and
the source: -[NSEvent subtype] raises for mouse events (8 exceptions in riot-client-89);
setHiddenUntilMouseMoves: hid the pointer for good. Learner chose "Implement, don't stop it" (their
signed-in client keeps running on the old install) and "Linux theme for now" for the pointer's look.
Owed once the learner says the client may stop: shutdown, install, paint test with the new
mousedown/mousemove counters, pointer check after typing, then the learner tries the client.
Seen, not addressed: -[NSEvent_mouse CGEvent] is missing (tolerated, returns nil).

## Milestone: signed in, Riot Client home page (2026-10-06 13:06, riot-client-89, superproject 117323c81)
The learner typed their own credentials into the live client and reached the Home page: news cards
with images, game icons in the side bar, account shown Online. 8 Riot processes alive, 0 cores after
2.5 minutes. Friends panel still shows loading placeholders (not investigated). The game itself is
not installed yet: nothing past this page is measured.

## Done: windows repaint (2026-10-06, superproject 117323c81)
Learner: "get the client to actually draw and repaint and paint correctly"; approved the repaint fix
at an Implementation checkpoint without questions.
Cause (measured with paintapp + DARLING_IOSURFACE_TRACE): nothing marked a displayed IOSurface as in
use, so Chromium's software output device painted every frame into one surface, and viz drops layer
parameters equal to the previous ones except every 10 s (root_compositor_frame_sink_impl.cc:554).
Approved fix: cocotron 24e732d5: CALayer holds a use count on its contents and setContents: asks
for a frame.
DEVIATIONS, done without a checkpoint each because each blocked the approved test and had one
evident repair (learner may revisit any):
- cocotron: three upstream NSSpeechSynthesizer commits cherry-picked (real initWithVoice:), plus
  e29b101f isSpeaking/isAnyApplicationSpeaking NO, stopSpeaking; f9363b47 availableVoices and
  defaultVoice both nil (Claude's choice: truthful "no voices"; a pretend voice was NOT added).
- superproject 6d5017494: TISCreateASCIICapableInputSourceList exported with its C name. Claude's own
  bug from 0f19657ae (mangled symbol; the earlier note that it worked was never exercised).
- superproject a6597cc3d: LSCopyDefaultApplicationURLForURL tested *outError instead of outError.
- darlingserver 6e11421: kqchan callback read once (server died calling NULL, once).
Verified: paint-test.sh: 16 distinct pictures in 16 grabs (was 1), frames split over two surfaces,
browser took 1886 of 1925 frames, typed text on screen, 0 cores. riot-client-88: 150 s, 0 cores, 10
processes alive, frames 2065/1984 over two surfaces.
Invalid evidence, discarded: Riot runs 84-86 screenshots taken by Claude's script were of the
desktop (window unmapped or client already gone).
Open: what URL the client tried to open externally after Claude's blind click and typing in run 86
(openExternal now fails without crashing; no handler exists). 24-px grey strip (top in the test app,
bottom in the Riot window). Typing into the real client by the learner not yet done on this build.
A second agent's container (build/codex-lock-diagnostics) was running on this build; not touched.

## Done: renderer heap corruption (2026-10-06, xnu 23353f1, superproject ebb98e073)
Root cause: libsystem_kernel exported weak, EMPTY os_unfair_lock_lock/unlock (file_handle.c,
workq_kernreturn.c; in the tree since 2023, also upstream). Electron's imports of those two bound to
them; trylock bound to libsystem_platform. PartitionAlloc's root lock was therefore taken once and
never excluded anyone again.
How it was found (Claude's work; learner said "solve once and for all", then approved the fix at an
Implementation checkpoint without questions): core of a run where three threads trapped inside the
same locked section (same root, same lock word 0x307); reading PartitionAlloc 108 source; an inserted
library replacing the three lock calls made the reproducer survive; then nm showed the duplicate.
Refuted on the way, each by a probe: "spaces" overwrite (crash handler's own padding); Blink's
stack-address main-thread test (stacktest); lock during thread exit (exitlocktest); shared
per-thread slots; in-place memory conversion (0 logged).
Fix: stubs deleted; the three internal users use libsimple_lock.
Verified: locksymtest FAIL -> PASS; installed libsystem_kernel exports no os_unfair_lock symbol;
atobapp 5 of 5 clean for 60 s (was 5 of 5 crashing); riot-client-82: 180 s, 0 cores, login page.
Not measured: whether the "slow repaint" report is gone (the broken lock spun its full retry count
on every acquisition). Not explained: why a probe built with Darling's SDK binds to the real ones.
Introduced by Claude, not explained back by the learner: what a lock is for; symbol binding through
libSystem's re-exports.
New probes/tools: stacktest, exitlocktest, locksymtest, lockaudit.dylib, lockswap-*.dylib, findslot.py.

## Done: font listing without X, and what followed (local, 2026-10-05)
Learner decisions: (1) "Fonts without X": font listing must not need a window server; confined
helpers do not get X. (2) Memory entries: "Entry made of pieces" for a range that partly overlaps
regions shared earlier.
Implemented, each a real answer:
- cocotron: fontconfig listing moved from X11Display to NSDisplay class methods; -[NSFont
  printerFont] returns self; font tables and table tags from FreeType; font descriptors made from
  data carry their font (CTFontManagerCreateFontDescriptorFromData); CTFontCopyTraits and
  CTFontGetSymbolicTraits read from the font (width trait values are a linear guess, marked).
- darlingserver: a memory entry is a list of pieces; no limit on their number; remap still needs one
  backing. memtest 35 checks pass (overlap and 20-page cases added).
- xnu: the S2C receive loop restores msg_controllen (a descriptor-less S2C call made the next one
  lose its descriptor).
- New src/frameworks/MediaPlayer (now-playing centre, remote commands, artwork, 8 keys). Claude's
  addition, NOT put to the learner beforehand: it stores what is set, shows nothing, delivers no
  commands; MPRIS named as the live route. Key string values are from memory, not checked on a Mac.
- AudioToolbox: failed converter no longer handed back.
Probes added: fonttabletest, fonttraittest, urltest. Tool: corebt.py (now also disassembles).

## Done: certificate trust (local, 2026-10-05, superproject 313060857)
Corrected chain (the agent theory in older notes was wrong): the guest's launchd is the System
manager, so Security asks com.apple.trustd, not the agent. trustd never ran. Fixes, each a real one:
foundation/corefoundation -[NSDictionary/NSArray writeToURL:error:]; cfnetwork
NSURLSessionConfiguration._sourceApplicationAuditTokenData; foundation NSURLComponents (creation,
scheme, host, URL); libxpc pipe replies owned by the caller (was a double release in every Libinfo
user lookup and in launchd's request loop); security: libtrustd's ARC sources built with ARC (two
files still without, see trust/trustd/CMakeLists.txt). Results (electron-n22): no trustd cores;
self-signed https -> certificate-error in 0.4 s; https://example.com -> ERR_CERT_AUTHORITY_INVALID,
but only after more than 20 s.
OPEN, learner's to decide: the _trustd account exists only as hand-added lines in the test prefix
(originals in baseline/prefix-etc-before-trustd/); the launcher (src/startup/darling.c, where
passwd/master.passwd/group are written) does not create it. Learner asked "what would make the most
sense?"; Claude's suggestion, not confirmed: a real _trustd account, uid/gid 282 as on macOS.
OPEN, not investigated: why example.com is "authority invalid" (its chain ends at SSL.com 2022 /
Comodo AAA; the root bundle is from 2020) and why the verdict takes over 20 s.

## Superseded: how frames get to the window (IOSurface and layers)
Stage: decided and implemented, see the section above.
Facts: REPORT.md "The wall". IOSurface in Darling is an unfinished client/server pair (iokitd);
QuartzCore draws CGImage layer contents only; CAContext/CALayerHost are stubs; CGLCreateContext fails.
Not decided: where surface memory lives and how it crosses processes; software frames first or the
GPU path; what draws an IOSurface into the X11 window.
This supersedes the older "Pending decision" below for the graphics route; that section is history.
Facts added 2026-10-05 (afternoon session; learner said "Proceed", gave no approach, checkpoint re-asked):
- Electron Framework imports 24 IOSurface functions (llvm-nm -u): Create, Lock/Unlock, GetBaseAddress
  (+OfPlane), size/format getters, Increment/DecrementUseCount, IsInUse, GetID, SetValue,
  CreateMachPort, LookupFromMachPort; plus CVPixelBufferCreateWithIOSurface/GetIOSurface,
  CGSMainConnectionID, and class references to CAContext and CALayerHost.
- Darling's CAContext/CALayerHost implement only methodSignatureForSelector:/forwardInvocation:.
- run31 dserver.log: the GPU helper is exec'd in bursts of about 10, roughly 0.25 s apart; one survives.
- From Claude's memory of Chromium's source, NOT verified against Electron 22: Chromium tests
  CAContext/CALayerHost with respondsToSelector and, when they fail, sends each frame's IOSurface to
  the browser as a Mach port and sets it as a CALayer's contents; its software compositor on macOS
  also draws into an IOSurface (lock, write at base address, unlock).

## Deferred: real font registration (no design chosen)
Facts Claude verified in cocotron:
- Every by-name font creation funnels through O2FontCreateWithFontName (Onyx2D/O2Font.m:413), which
  asks fontconfig (files on disk only) and falls back to the empty pattern.
- Fonts built from data (O2Font_freetype initWithDataProvider:) never set _name, so
  CGFontCopyPostScriptName returns NULL for them; FreeType can supply the name but is not asked.
- The installer imports only CGFontCreateWithDataProvider, CGFontRelease, and the two
  CTFontManager register/unregister functions; its binary names MarkPro-Bold/Medium and CJK fonts.
  Inference (unverified): it then asks AppKit for those fonts by name, so current text is a fallback.
Upstream check: none of the gaps fixed in the fork; no open PR covers them; no push access; no forks.

## Done: -[NSImage drawInRect:] (local only, 2026-10-04)
Learner design: delegate to the four-argument method; draw plainly. Claude corrected the literal
"NULL" values to NSZeroRect / NSCompositeSourceOver / 1.0; learner approved.
Implemented: cocotron cc0eafbf; superproject local/league-baseline 77c40b8fb. Not pushed.
Verified over three runs: window draws every time; 2 of 3 reach the install screen (Install,
Advanced Options) after fetching config and CDN data; 1 of 3 shows "NO INTERNET FOUND" from
failed hostname lookups (intermittent, cause unknown); run 3 segfaulted during quit after the
window was closed (objc_msgSend respondsToSelector: to an invalid object; cause unconfirmed).
Learner observed both screens on their own display and reported them.
Report section added to baseline/REPORT.md; evidence in baseline/after-drawinrect.

## Done: font-registration stub change (local only, 2026-10-04)
Learner decision: report success anyway, log verbosely (no reasoning given; Claude flagged it is
untrue and not acceptable as a fork fix). Learner approved Claude's additions (NSLog, font name,
Unregister sibling, ponytail comment, local branch, three reruns in existing prefixes).
Implemented: cocotron branch local/ctfontmanager-pretend-registration, commit 02d9472b; superproject
local/league-baseline 20173f7e4. Not pushed. Always returns true, including for a NULL font.
Verified 3 of 3: font crash gone; 22 registrations logged (all "(no name)"); app starts its
remoting server, launches its agent, requests config from Riot, shows splash, then aborts on
uncaught exception -[NSImage drawInRect:] unrecognized selector during first window draw.
Still owed: real font registration; issues/PRs for xnu fix and this; never-cleared sigexc flag.

## Crash diagnosis (2026-10-04)
- Learner-proposed approach: go through the coredump in detail, and rerun with extra logging.
  Claude carried both out; the specific techniques (register decode, stack walk, disassembly,
  pty stdout, xtrace) were Claude's.
- Verified cause: CTFontManagerRegisterGraphicsFont is a stub (cocotron CoreText/CTFontManager.m:9)
  returning false without setting *error; app calls CFErrorCopyDescription on its uninitialized
  error -> objc_msgSend on garbage 0x5A35B1ECB. Confirmed by STUB line on a pty rerun and by xtrace
  (last file: Resources/Fonts/fonts.wad). Duplicate NSConstantArray warning is not the cause.
- Report section added to baseline/REPORT.md; evidence in baseline/diag/.

## In progress: dyld link fix (local only)
Approved 2026-10-04 as "Implement, nothing public": learner asked Claude for the best shape, Claude
recommended it (one static TSD slot; never-cleared flag as a separate issue; file issue + claim),
learner approved the code but NOT the public posts. So: no issue, no claim, no PR exist. This
departs from the repo's claim-first protocol, by the learner's choice.
Implemented: xnu branch fix/sigexc-no-tlv-in-dyld, commit 4d77d4b (tsd.h: __PTK_DARLING_SIGEXC_SIGNAL
on KEY3; sigexc.c: two __thread variables replaced by slot get/set). Superproject branch
local/league-baseline, commit 1ef728c18 (xnu pointer). Not pushed.
Verified: dyld links (i386 + x86_64); full build completed, ninja -n reports no work; container
booted 3 of 3 times; guard did not misfire in the three runs. The never-cleared flag is still untested.
Still owed if the learner later agrees: issue for the build failure, issue for the never-cleared
flag (unverified, from reading), PR to VibeDarling/darling-xnu.

## dyld link failure (facts verified 2026-10-04)
- Build stopped at 25799/26932 linking src/external/dyld/dyld; 1134 steps remain.
- Undefined __tlv_bootstrap from two `static __thread` variables in xnu
  darling/src/libsystem_kernel/emulation/src/linux_premigration/signal/sigexc.c lines 377-378
  (xnu HEAD b1cd162; superproject PR #1073). Defined only in libdyld's threadLocalVariables.c,
  not linked into the dyld executable. x86_64-only test link fails identically.
- The same library already documents this constraint in emulation/src/xnu_syscall/machdep/impl/tls.c.
- Existing per-thread storage without __thread: pthread direct TSD slots, emulation/include/common/tsd.h
  (__PTK_DARLING_KEY0-2 used, others unused), used by per_thread_wd.h; and a tid-keyed table in tls.c (arm64).
- From reading only, not run: in_sigexc_handler is set at line 417 and never cleared anywhere in the file.
- darling-xnu has issues disabled; issues go in VibeDarling/darling. None found for this.

## Record of the checkpoint
Decision: League baseline run (Riot installer under Darling).
Learner decisions: capture the full evidence set on every run (Claude's reading of "we should
capture depth"; stated back to the learner); three runs.
Claude's proposed additions, not yet confirmed: static import scan before running; fresh disposable
prefix per run; 3-minute hang limit ended with `darling shutdown`; report kept local, no GitHub
issue or repo commit yet.
Proposed scope: DESTDIR install of the finished build, download the official cask zip outside the
repo, a run script in the session scratchpad, a local Markdown report. No changes to Darling source.

## Environment facts (2026-10-04)
- All 149 submodules checked out (learner ran the fetch; verified, no errors).
- Build tree: build/ (git-ignored), RelWithDebInfo, clang 23.1.1, from clean master 5a49c8fba;
  ninja -j10 running in background, at about 4300 of 26932 steps when last checked, no failures so far.
- Host: x86_64, 12 cores, 14 GB RAM, X11 session, RADV Vulkan 1.4, 89 GB free, no ccache.
- Cask `league-of-legends`: a zip holding a manual installer app that downloads the game itself.
- No League/Riot issue exists in the fork.
- tools/darling-applications: run-qa-profile.sh is specific to the Applications viewer on Wayland
  (not usable here); scan-imported-apps.py does static import scans of app bundles.

## Baseline reports for app failures
- Introduced (explained by Claude): stages a Mac app passes through in Darling; Proton log to
  Darling evidence mapping (exit code/signal, dyld messages, darlingserver log level, coredumps).
- Demonstrated understanding: none recorded. Learner said they were drawing a blank on what a
  report should capture, then chose between two options Claude offered.
- Needs reinforcement: deciding what evidence separates one failure stage from another.

## Drawing layers (AppKit / CoreGraphics / backend / Metal)
- Learner reasoning (2026-10-04): a drawing method must need a graphics API, so maybe Metal-to-Vulkan;
  asked if that was logical. Own reasoning, offered tentatively.
- Introduced (explained by Claude): the missing method is at the AppKit layer; the existing
  four-argument drawInRect draws through CoreGraphics (Onyx2D software rasteriser) and the X11
  backend; Metal is a separate GPU path used by apps that call it directly.
- Demonstrated understanding: after the explanation, proposed implementing the missing method by
  delegating to the existing four-argument one (own design). Did not yet show how to check what
  default values mean in the code.
- Needs reinforcement: telling which layer a failure belongs to from its error message.

## Run 95 (build ee829a120): Play through the Riot Client, League client crash 2

- LeagueClient started with the real Riot Client arguments (so `LCE-CA900FE5` was an artifact of running it alone, as guessed).
- `PT_DENY_ATTACH` called twice, returned 0. No `__atomic_*` crash.
- Died 6 s after start, main thread, SIGSEGV in `CFArrayGetCount(NULL)`, return address LeagueClient+0xb6b6e.
- Caller runs `system_profiler SPDisplaysDataType -xml`, feeds the output to `CFPropertyListCreateFromXMLData`, then calls `CFArrayGetCount` on the result with no NULL check.
- Darling's `src/tools/system_profiler.c` ignores `-xml` and prints plain text, so the parse returns NULL. Genuine Darling gap, in the client's own code, not in `mvg`.
- Pending decision: implement `-xml` output for SPDisplaysDataType (awaiting learner approval). Not yet read: which keys the client takes from the plist.
- Side finding: `~/.darling-league-run2/private/var/log/dserver.log` is 10 GB (cumulative, ~750 KB/s while the client runs at info level).

## Done: system_profiler -xml (superproject 82430c0b7, installed)

- Learner approved "Implement this step" and "Empty it" (dserver.log truncated after shutdown).
- `src/tools/system_profiler.c`: `-xml` prints a plist array of `{_dataType, _items}`. Displays entry has `_name`, `sppci_model`, `sppci_bus`, `sppci_device_type`, `spdisplays_ndrvs`. No `spdisplays_vram` (unknown; client NULL-checks it). Other data types under `-xml`: entry with empty `_items`.
- Evidence: before, `plutil -lint` on the output = "invalid property list"; after = "OK". Text mode unchanged.
- Not verified: the League client itself past this point. Card name and 1920 x 1080 come from the tool's older guesses (CPU brand string, xdpyinfo), not from the real GPU.
- Owed: learner presses Play again on 82430c0b7.

## Run 96 (82430c0b7) and follow-ups, 2026-10-06 evening

- Run 96: LeagueClient got past system_profiler and started the UI process `LeagueClientUx` (CEF). dyld aborted it: `_kCVBufferPropagatedAttachmentsKey` missing. Scan: 4 load-time symbols missing (CoreVideo 1, DiskArbitration 2, CryptoTokenKit `TKTokenWatcher`), 9 lazy functions, plus `pm_sample_task` (skipped).
- Done, learner approved: e5b142b09 CoreVideo, 06ec20b65 DiskArbitration (new `DACallbacks.c`), 20a3979f5 CryptoTokenKit (`src/TKTokenWatcher.m`). Rescan: 0 at load, only `pm_sample_task` lazy. UI run alone reaches its own `main` and stops at missing `--app-name` (artifact of no arguments).
- Run 96 second finding: 15 s after the UI died, a LeagueClient thread (the one that calls PT_DENY_ATTACH repeatedly) took SIGSEGV; exception delivery called unsafe stub `thread_getstatus_to_user` and darlingserver aborted (whole container down, app exit 137). Module of the faulting address unknown (no core).
- Done, learner approved: darlingserver 62832fa, superproject 7b89e5ded (installed): both stubs call `thread_get_state` / `thread_set_state`.
- Probe `excstatetest` (EXCEPTION_STATE_IDENTITY handler redirects rip): BEFORE confirmed, darlingserver SIGABRT 20:22:04. AFTER NOT YET RUN.
- Blocked: the before-run left processes of prefix `~/.darling-league-run2` behind (launcher 366505, zombie server 366516, launchd 366543 and children, probe 366565). `darling shutdown` says "Cannot verify this prefix's server for shutdown." My kill was denied by the harness; the learner must clear them. Possible Darling bug: shutdown cannot clean up after a server that died while its launcher still holds it as a zombie.
- Owed: after-run of `excstatetest`, then learner presses Play on 7b89e5ded.
- 2026-10-06 20:3x: learner ran the kill; leftovers gone. `excstatetest` AFTER on 7b89e5ded: PASS (exception 1, code 0x1, address 0x10, thread resumed at handler-set rip), server alive. Container shut down. Owed: learner presses Play on 7b89e5ded.

## Run 97 (7b89e5ded): Riot Client self-update error; proc_pidfdinfo (installed c224c3875)

- Run 97 never reached Play: Riot published a new Riot Client release between runs. Launcher log: "Update failed: Failed getting path to current executable: Failed reading executable vnode. Expected 1200 bytes, got 0"; app.out: "sys_proc_info(): Unsupported callnum: 3". Learner quit from the error screen. No crash.
- 1200 = sizeof(struct vnode_fdinfowithpath) = proc_pidfdinfo(PROC_PIDFDVNODEPATHINFO).
- Done, learner approved: xnu 3cfbf02, superproject c224c3875. `_proc_pidfdinfo()` in emulation `misc/proc_info.c` (path via /proc/<pid>/fd, stat, offset and access mode via fdinfo). Other flavors still ENOTSUP.
- Probe `fdinfotest`: before "returned 0", after PASS (1200, right path, size, inode, offset 7, FREAD; closed fd, short buffer and pipe all refused).
- Not verified: the updater itself (it may need more after this); the UI and server-abort fixes from run 96 are still untested under Play.
- Owed: learner runs rc.sh; the client will try to update itself first.

## Runs 98-99 and hw.model (installed 7360536dc)

- Run 98 (c224c3875): proc_pidfdinfo fix worked; Riot Client self-updated to release BDD843A2DA3D4B37 in 13 s, restarted itself, and rc.sh shut the container down because it only watches the first process (script limitation).
- Run 99: Play pressed. Server did NOT abort (62832fa holds). LeagueClient died: SIGSEGV on the thread that calls PT_DENY_ATTACH, rip in `mvg` at a `call` instruction, rdi = "hw.model", rsp = 0xFFFFFFFFB94CF760 (bogus, equals rsi), rbp sane. UI process never started.
- Darling-side test only (probe `hwmodeltest`, no module analysis): `hw.model` did not exist (ENOENT, length left untouched) by name, by MIB and by sysctlnametomib.
- Done, learner chose value "Darling1,1": xnu 7d06305 (hw.model), superproject c61a3219d.
- Flagged deviation (found by the probe, not asked for): xnu 4ad30c4, superproject 7360536dc: `copyout_string()` now reports strlen+1 for all string sysctls. Not changed: short buffer still returns 0 where macOS gives ENOMEM.
- Probe: all 6 routes ok, PASS. Sanity: hw.machine, kern.ostype, kern.osrelease, brand string, hostname, sw_vers (26.0), uname all still print whole; bstest unchanged (3 services).
- Note: sw_vers says 26.0 while the Riot launcher logs osVersion 10.10.10. Not investigated.
- Not verified: that the client's crash was only the missing value. If the next stop is again inside mvg and looks like a deliberate environment check, stop and tell the learner.
- Owed: learner presses Play on 7360536dc.

## Run 100 (7360536dc): MILESTONE, League client window drew; kext_request (installed 55f757314)

- hw.model fix worked: LeagueClient lived ~2 min, started LeagueClientUx plus helpers; learner saw "A League window with content". UI process alive 110 s.
- Then LeagueClient called `kext_request` (list loaded kernel extensions): unsafe stub, darlingserver SIGABRT, app exit 137. Three UI helper processes also died early in the run (413679, 413755, 413776), not investigated.
- Done, learner chose "answer not supported": darlingserver 4ab05bd, superproject 55f757314: `kext_request` returns KERN_NOT_SUPPORTED like `kmod_get_info`.
- Probe `kexttest`: after = kr 0x2e, server alive, PASS. Before-state taken from run 100's log and core (probe not run before the fix, to avoid another stuck dead-server container).
- Other `dtape_stub_unsafe` sites remain (host.c 13, task.c 22, thread.c 11, memory.c 30, processor.c 14, ...): each can still abort the server.
- Boundary note: asking for loaded kernel extensions is plausibly the protection module inspecting the system. Darling answers honestly (none, unsupported). If the client now refuses to continue, that is out of scope.
- Owed: learner presses Play on 55f757314.

## Run 101 (55f757314): MILESTONE, League client usable; Metal notice

- kext_request fix held. Learner: reached the League client; "Graphics Upgrade" notice (Mac does not support Metal) closes and the client works behind it.
- Cause of the notice (client's own code, LeagueClient+0x13bb533): dlopen Metal, dlsym `MTLCreateSystemDefaultDevice`, call it; nil -> log "Mac Metal is not supported by this machine."
- Darling's Metal is gated off unless `DARLING_ENABLE_METAL=1`. `metalprobe`: off = NONE, on = "AMD Radeon RX 6650 XT (RADV NAVI23)".
- Learner decision (2026-10-06): try one run with Metal on (env only, no code). Risk told: Riot Client / League UI drawing may change or break.
- Owed: learner quits the client, runs rc.sh with RC_GUEST_ENV including DARLING_ENABLE_METAL=1, reports.

## Run 102 (55f757314, DARLING_ENABLE_METAL=1): UI window never appears

- Learner: stuck on the League logo splash; no LeagueClientUx window. Learner interrupted and shut the container down.
- Seen live: UX main alive; its GPU helper process stuck early with only 2 threads (main thread blocked in read() during start-up); the GPU helper's crashpad handler (LeagueCrashHandler) at 100 % CPU for minutes; no cores. dserver.log grew 2 GB in 4 min.
- Not decoded further (interrupted). Reading: with a Metal device present the CEF GPU process takes a different start-up path and hangs. Metal-off runs (run 101) remain the working configuration.
- Open: why the GPU helper blocks; why its crash handler spins (possibly also present with Metal off, not checked).

## Metal-on hang, investigation so far (2026-10-06 late)

- Learner: "lets improve the Metal switch and make it better".
- `readWriteTextureSupport` line in run 102 is from the Riot Client's Electron (00:54:50, before League started), not from League's GPU helper. Not the hang's cause as far as shown.
- League GPU helper (424118): 374 server log lines in 194 s, main thread blocked in read() right after start; never crashed until shutdown.
- Its own crash handler (424126, LeagueCrashHandler, ppid 1): start-up only in the server log, then 100 % CPU in user space. Sampled stack top: `mldr __darling_thread_create+0x205` under crashpad `WorkerThread::Start`.
- Code read: `src/startup/mldr/elfcalls/threads.c` `__darling_thread_create` ignores `pthread_create`'s return and then loops `while (args.pth != NULL) sched_yield();`. If the host thread is not created (or never clears `args.pth`), this spins forever. Hypothesis, NOT confirmed: this is the spin, and the GPU helper waits on its handler.
- Unknown: why it would happen only with Metal on; thread 2 of the handler was not sampled.
- Standalone GPU helper with Metal on does not reproduce (crashes at once, artifact of no browser).
- Needed: a Metal-on run left stuck so the handler's threads and errno can be sampled live.

## Run 103 (55f757314, Metal on): client up, notice gone, but "very very slow and delayed and laggy"

- Hang of run 102 did not recur: intermittent, so not caused by Metal. All crash handlers had 3 threads this time (the spinning one in 102 had 2).
- "Mac Metal is not supported" log line gone; learner confirms the notice is gone.
- Measured live: no guest process CPU-bound (UI browser 14 %, renderers 10-22 %, darlingserver 22 %); League's GPU helper runs `--use-gl=disabled`, Riot Client's `--use-angle=swiftshader-webgl`: both software, Metal draws nothing in the client. LeagueClient main thread idle in its event wait.
- Lag cause NOT found. No Metal-off measurement to compare with. Confounders: server log at info level (280 KB/s, PthreadCanceled TODO warning is the top line, one server call each), my own gdb sampling pauses the process, another agent's Electron pins one core.
- Tooling note: Chromium renames the main thread, so those processes have comm CrBrowserMain/CrRendererMain/CrGpuMain; `pgrep -x mldr` misses them. Match on /proc/<pid>/exe.

## GPU drawing: measurement (2026-10-06 night), learner chose "Measure first"

- Learner: "instead of using the CPU for rendering, make it use my dedicated GPU for a test".
- Runs electron-gpu-off-1 and electron-gpu-metal-1 (stock Electron 22.3.27, FRAME_TEST_HW=1, build 55f757314): identical first failure, Metal switch makes no difference: `ANGLE Display::initialize error 12289: Could not create the CGL context.` -> GPU process exits (exit_code=15) -> software fallback.
- `cgltest`: CGLChoosePixelFormat ok, CGLCreateContext = 10004 (kCGLBadContext).
- Cause read in `src/frameworks/OpenGL/OpenGL.c`: CGLCreateContext uses the global EGL `display`/`config`, set only by CGLRegisterNativeDisplay* (called by AppKit's window backend). A process with no window (the GPU helper, the probe) has display NULL, so eglCreateContext fails. It also passes no context attributes (profile/version from the pixel format ignored).
- Known walls after it: `CGLTexImageIOSurface2D` is a stub that returns success and does nothing (frames would be black / never reach the IOSurface the window shows); League's newer CEF may prefer ANGLE's Metal backend (not measured, no logs from it); which of the two AMD GPUs host EGL picks.
- Pending decision: step 1 = off-screen CGL contexts on the host GPU (awaiting learner approval).

## GPU step 1 DONE: window-less OpenGL contexts (superproject 02c4009c4, installed)

- Learner: "lets just do the Opengl one" (after asking about Vulkan; routes A/B/C explained, A recommended, Zink test offered and not taken up).
- `src/frameworks/OpenGL/OpenGL.c`: `offscreen_display_open()` (EGL_PLATFORM_SURFACELESS_MESA, pbuffer-capable RGBA8 config) used by CGLCreateContext when no display is registered; eglBindAPI per call; kCGLPFAOpenGLProfile honoured (3.2/4.1 core); pixel format keeps its terminator. `CGLTypes.h`: kCGLPFAOpenGLProfile, kCGLOGLPVersion_*.
- Probe `cglctxtest`: before = create 10004 for all three; after = PASS, renderer "AMD Radeon Graphics (radeonsi, renoir ...)" 4.6 core / compatibility. Old `cgltest` now creates its context too.
- GPU choice: default is the integrated Renoir GPU. With guest env `DRI_PRIME=1` the same probe reports "AMD Radeon RX 6650 XT (radeonsi, navi23 ...)". No code.
- Electron test `electron-gpu-cgl-1`: the CGL-context error is gone; GPU process initialises (104 ms) and then exits: `FramebufferGL.cpp:1244 GL framebuffer returned incomplete: 0x00008CD6` -> `Unable to initialize SkSurface` -> "Exiting GPU process because some drivers can't recover from errors"; after repeated crashes Chromium gives up on the GPU.
- Reading: 0x8CD6 = incomplete attachment = the texture Chromium ties to an IOSurface has no storage, because `CGLTexImageIOSurface2D` is a stub. That is step 2 (design choice pending: copy model vs shared GPU buffer).
- Not verified: Riot Client / League runs on this build (window drawing must not regress: when the GPU process keeps crashing Chromium falls back to software, as the Electron test did).

## INCIDENT 2026-10-06 ~21:38: host kernel GPU fault after GPU step 1

- Learner chose step 2 "A" (copy each frame). NOT started; only read CMake (OpenGL framework re-exports src/native/libGL.dylib).
- Learner ran rc.sh with host `DRI_PRIME=1` + Metal on, then shut down: "Process 452820 / 453481 left by a dead darlingserver did not exit."
- Both (Riot Client GPU helper, iokitd) are in state D, wchan `amdgpu_info_ioctl`, SIGKILL pending, holding renderD129 = card0 = 0000:0f:00.0 = integrated Renoir GPU (0x1638). The RX 6650 XT is card1 / renderD128 / boot_vga=1 (it drives the desktop).
- Kernel log: storm of `iommu ivhd0: AMD-Vi: Event logged [INVALID_DEVICE_REQUEST device=0000:0f:00.0 ...]`, thousands per second, journal rotating, load average 18 and rising.
- Reading (not proven): step 1 lets window-less guest processes open the default surfaceless EGL device, which is the integrated GPU; that GPU (not driving a display) faulted in the host kernel driver. Host kernel/driver problem triggered by our new GPU use.
- All code is committed (HEAD 02c4009c4, installed). Told the learner to save work and reboot.
- After reboot, before any GPU run: decide how to keep guests off the integrated GPU (default to the display GPU), and consider reverting/gating step 1 until then.

## Done: window-less contexts pick the display GPU (2026-10-06 ~22:00, superproject a11e9da22, installed)

- After reboot: host clean, 0 INVALID_DEVICE_REQUEST. Learner said the kernel problem is resolved (how: not stated). Renoir still bound to amdgpu.
- Learner asked why not just use the dedicated card; approved ("We can give it a shot") the proposal: enumerate EGL devices, take the boot_vga one, DRI_PRIME still wins.
- `OpenGL.c` `display_gpu_open()`: eglQueryDevicesEXT + render node -> `/Volumes/SystemRoot/sys/class/drm/<node>/device/boot_vga`, opened with EGL_PLATFORM_DEVICE_EXT; falls back to the surfaceless default.
- `cglctxtest`, no env: PASS on "AMD Radeon RX 6650 XT" (before: Renoir). With guest DRI_PRIME=1: also RX 6650 XT. No kernel events.
- NOT proven: that DRI_PRIME=1 flipping windowed processes onto Renoir caused the incident. NOT tested: Riot Client / League on this build; which GPU windowed (X11) processes and iokitd open.
- Rule: do not run with DRI_PRIME=1.
- Next: step 2 "A" (copy each frame) for CGLTexImageIOSurface2D, not started.

## Pending decision: GPU step 2 implementation checkpoint (asked 2026-10-06 ~22:10, awaiting learner)

- Facts read: surface = shared memory; `CARenderer.m` `CATexImage2DIOSurface` already uploads the surface's bytes (BGRA only) on every draw, in the window process. So step 2 only has to (a) give the texture storage in `CGLTexImageIOSurface2D`, (b) copy rendered pixels back into the surface memory inside the GPU process.
- Proposed: (a) glTexImage2D with NULL data + remember (context, texture, surface, retained); (b) on glFlush/glFinish read tracked textures back with a scratch FBO + glReadPixels (BGRA) into IOSurfaceGetBaseAddress.
- NOT verified: where to hook glFlush. OpenGL.framework re-exports `src/native/libGL.dylib` (wrap_elf of host libGL.so); ANGLE may dlopen `OpenGL.framework/Libraries/libGL.dylib` and dlsym GL functions itself, bypassing anything defined in the framework. wrap_elf generator not read yet.
- No code written for step 2.

## GPU step 2 DONE in stock Electron: frames drawn on the GPU reach the window (2026-10-06 ~22:05, superproject fea89a343, installed)

- Learner approved order "storage, stock test, then copy-back". Learner's answer on why the copy is in the GPU process: "it needs direct access to the GPU resources" (right in substance; corrected: window process only shows, GPU process draws into a texture, not the display). Not yet a demonstrated concept.
- caefe84c3 storage: `CGLTexImageIOSurface2D` allocates the texture. Electron GPU process no longer crashes (was 4 exits then software fallback).
- Test page matters: solid-colour page shows even with no frame (Chromium uses a plain coloured layer). New `electron-22.3.27/gradtest` (gradient + text), `ET_APP=gradtest`.
- fea89a343 copy-back: registry texture -> surface; framework's own `glFlush`/`glFinish` read each back (scratch FBO + glReadPixels) into the surface memory. ANGLE dlopens `OpenGL.framework/Libraries/libGL.dylib` by path, so that path is now a symlink to the framework and the host wrapper moved to `/usr/lib/native/libGL.dylib` (`cmake/use_ld64.cmake` mapping changed; relinks everything).
- Runs: electron-gpu-storage-grad-1 = blank window (GPU alive, no copy); electron-gpu-sw-grad-1 = gradient (software); electron-gpu-copyback-grad-2 = gradient + text, GPU process alive whole run, GPU helper held renderD128 (RX 6650 XT) in the count run, 0 kernel events.
- GAP found (not fixed, no issue filed): in non-root mode darlingserver copies the runtime into the prefix (`copyAndSetAttributes`), and skips an entry whose type changed (file -> symlink) and never removes files. Old `Libraries/libGL.dylib` stayed in `~/.darling-league-run2` and bypassed the hook (run electron-gpu-copyback-grad-1 blank). Removed that one file by hand. Any other existing prefix needs the same.
- Ceilings (ponytail comments in OpenGL.c): every surface texture read back on every flush; copy is one way (surface pixels are not uploaded for sampling); 4 bytes per pixel assumed; texture names not split by share group.
- NOT tested: Riot Client, League client, performance, video/WebGL pages, call counts of the read-back per frame.
- Side observation: software path draws the page shifted up under the title bar (text clipped); GPU path does not.

## Run 105 (fea89a343, Metal on, learner ran rc.sh): GPU helper SIGSEGV x3; fixed in b11368582 (installed, NOT re-run)

- Cores 61841/62122/62185: all in `media::VTVideoEncodeAccelerator::CreateCompressionSession`, null read. Cause: Riot Client weak-imports `kVTVideoEncoderSpecification_EnableLowLatencyRateControl` (and `kVTCompressionPropertyKey_BaseLayerFrameRateFraction`), absent from Darling's VideoToolbox. Reached only now because the GPU process lives long enough to be asked for encoders. Not caused by the copy-back code.
- b11368582: both constants added; `VTCompressionSessionCreate` returns kVTCouldNotFindVideoEncoderErr (-12908) instead of 0 with no session. Done without a checkpoint (learner had not asked; evident repair).
- Host: 0 kernel IOMMU events after run 105.
- Not known: what the learner saw on screen in run 105; whether the client draws on the GPU after this fix.

## Run 106 (b11368582, Metal on, learner ran rc.sh; "Debug why the client is not loading"): fix installed 1f062abe8, League NOT re-run

- Run 106: VideoToolbox crash gone (Riot Client GPU helper: 1 exit 139 early, no core read). League: UX renderer 68907 SIGTRAP 22:11:41; LeagueClientUx GPU helper 68749 SIGSEGV 22:12:02; LeagueClientUx main 68716 SIGSEGV 22:12:03.
- 68749 decoded: objc_msgSend `_removeSublayer:` from `-[CALayer removeFromSuperlayer]`, receiver freed. Cause read in `cocotron/QuartzCore/CALayer.m`: `_superlayer` is unretained and neither `dealloc` nor `setSublayers:` cleared it on the children, so a child outliving its parent messaged freed memory. Reached now because CEF's GPU path builds and tears down a layer tree per frame.
- Fix (cocotron, superproject 26a1939b3): `_orphanSublayers` in dealloc and setSublayers:.
- Tried and reverted (1f062abe8): `CATransformLayer : CALayer` instead of the forwarding stub. Stock Electron gradient test went BLACK (electron-gpu-layers-grad-1); with the stub back it draws (electron-gpu-layers-grad-2, pixels blue/green). Why real transform layers draw black: not investigated. The stub still logs per frame (156k lines per call name in run 106).
- NOT decoded: 68716 (decode.sh hung on it; 0 % CPU in its python step, cause unknown) and 68907. Not known whether they share the cause.
- Tool note: `pkill -f tools/decode.sh` kills the calling shell too (pattern matches own command line).

## Run 106 cores decoded (2026-10-07, no code change)

- Learner: "Lets get working on the undecoded crashes".
- 68907 (UX renderer, SIGTRAP 02:11:41 UTC): Chromium LOG(FATAL), text in the core: `FATAL:frame_impl.cc(640)] frame 21474836491 [5,11] connection retry failed`. CEF 108.4.13 (chromium 108.0.5359.125). The renderer gave up connecting a frame to the browser process. Not a Darling function crashing.
- UX browser log is silent from 91.4 s to 118.9 s and reports the renderer's death 22 s after it happened; `jade-home` "Request timed out after 8 seconds" at 81 s. Reading, NOT proven: the browser's main thread stalls for tens of seconds, and the renderer fatal is a consequence. Cause of the stall unknown; needs a live sample of the browser main thread. app.err holds 316k CATransformLayer stub lines for the run.
- 68716 (LeagueClientUx main, SIGSEGV 02:12:03): teardown, after LeagueClient unloaded the UX. Stack: `-[NSWindow close]` -> `orderWindow:relativeTo:` -> `_passKeyToFrontmostWindow` -> `resignKeyWindow` -> `-[NSNotificationCenter postNotification:]` -> `performSelector:withObject:` (`windowDidResignKey:`) on freed memory (isa 0, fault address 0x18, block reused for a URL string).
- Code read: `foundation/src/NSNotificationCenter.m` stores `_observer` as a plain unretained pointer; `cocotron/AppKit/NSWindow.m` `setDelegate:` registers the delegate as observer for 15 notifications. An observer freed without removing itself is messaged. Which object the dead observer was: not identified (assumed the window delegate).
- Order of events in run 106: renderer fatal -> GPU helper crash (fixed 26a1939b3) and client unloads UX -> browser crash on close -> UX restarted (69440).
- Tool bug: `corelib.py` `symbols()` runs `llvm-otool -l` on host ELF libraries (libgallium) and never returns; that was the decode.sh hang.
- Learner's answers: (1) torn between NSWindow ("sounds more of a proper fix") and the center ("also sounds right"); (2) Metal on, "since Riot uses Metal for the game"; named nothing to measure. Claude gave the separating facts (NSWindow passes the delegate itself to the center; dead observer unidentified) and proposed the center fix and a /proc sampler; learner chose "Implement this step" without questions.

## Done: notification observers are weak (2026-10-07, foundation commit under superproject 43b8b47e2, installed)

- `foundation/src/NSNotificationCenter.m`: `_NSNotificationObserver` keeps a weak reference (`objc_storeWeakOrNil`, `objc_loadWeakRetained` around the call); raw address kept only for matching in removeObserver:; classes refusing weak references keep the old path. Ceiling noted in code: dead entries stay listed.
- Probe `notifytest` (build: `build.sh notifytest -x objective-c -framework Foundation`): before = exit 139 after "all alive: 3 calls"; after = PASS 3 of 3.
- NOT tested: League / Riot Client on this build. Not identified: which observer was dead in run 106.
- `tools/stall.sh <file> [secs]`: logs the UX browser main thread's state, CPU ticks, syscall and wchan every 2 s from /proc. Started for run 107 -> `baseline/stall-107.txt`.
- Owed: learner runs the client with Metal on and reports what is on screen.

## Run 107 (43b8b47e2, Metal OFF by typo `C_GUEST_ENV`): League client never appears (2026-10-07 09:08)

- Learner: "I pressed play in the riot client and it just didnt load the league client".
- Live: LeagueClient 104716 main thread at ~98 % CPU for minutes, own log stops at 1.347 s ("StorePlugin not ready"; yesterday the next line was "HandleRsoAccessToken calling CheckReady"). UX 106694 logs "Trying to connect to app process" and waits; no renderer or GPU helper started. No cores.
- Main thread stack (gcore in scratchpad, not kept): polling loop calling `std::chrono::steady_clock::now`, callers all in LeagueClient's own text (+0x5489f0, +0x68ec19, ... up to +0x23046); no Foundation/AppKit frames. What it waits for: not found.
- Not known: regression from the three builds never run on League (26a1939b3, 1f062abe8, 43b8b47e2) or an intermittent start hang. New stop point; runs 102 and 00-36-33 stopped elsewhere.
- `stall.sh` weakness: a main thread waiting in its event loop and one stuck on a lost reply both show `S 47 __skb_wait_for_more_packets`; it only separates busy from waiting.
- Owed: learner quits the Riot Client and reruns with `RC_GUEST_ENV` spelled right.

## Run 108 (43b8b47e2, Metal on): start hang did not recur; renderer fatal reproduced; server saturated (2026-10-07 09:18)

- Learner: "LeagueClientUX loaded, and was loading for a split second without the freeze bug. After a while of waiting on a frozen LeagueClientUX, it auto restarted itself."
- Run 107's start hang did not recur: intermittent, not a regression (1 of 2 runs).
- Renderer 335613 SIGTRAP at +97 s: `FATAL:frame_impl.cc(640)] frame 21474836507 [5,27] connection retry failed`. Same as run 106, different frame. Same timeline: jade-home timeout 79 s, terminated logged 114 s.
- Verified in League: no browser core on close ("Shutting down CEF. Exiting ux."), so the weak-observer fix held; no GPU helper core, so 26a1939b3 held. One run each.
- Sampler: UX browser main thread about 20 % CPU and mostly waiting through the freeze; NOT busy. darlingserver 280-370 ticks per 2 s (about 1.5-1.8 cores).
- Server log, 14.5 s window: 32k lines/s, 4 MB/s (run log 2.6 GB). 263k `PthreadCanceled` TODO lines = about 18k calls/s; 73k each "Trying to lock/unlock mutex without an active thread"; 22k `proc_get_effective_thread_policy` stub.
- Code read: `CANCELATION_POINT()` (xnu emulation `cancelable.h`, used in 29 files) calls `sys_pthread_canceled(0)` = one server round trip before every cancellable syscall; the server always answers -ENOSYS, and `PthreadMarkcancel` is also -ENOSYS, so no thread can ever be cancelled.
- NOT proven: that this load causes the freeze / the renderer fatal. Confounder: runs use DSERVER_LOG_LEVEL=info.
- Pending decision (asked 2026-10-07): answer the cancellation check in the guest without the server call.

## Cancellation query answered in the guest (2026-10-07, xnu commit under superproject ddc161d4b, installed, NOT run)

- Learner chose "Implement this step" without questions.
- `xnu/.../bsdthread/pthread_canceled.c`: `sys_pthread_canceled(0)` returns -EINVAL (not cancelled) with no server call; actions 1/2 unchanged. Ceiling in code: needs a shared flag once the server can cancel threads.
- Run 108's container shut down by Claude before the install (learner had not quit the client).
- Owed: one run, then compare `PthreadCanceled` lines/s (was about 18k) and darlingserver ticks (was 280-370 per 2 s), and whether the freeze / renderer fatal remains.

## BOUNDARY: "VANGUARD ERROR: VAN -101" popup (learner report, run 108, after the UX restart)

- Client's own log, runs 106 and 108 alike: `MvgInit returned 0`, `MvgRegisterLogin returned 0`, `MvgState: 7` -> `0` (about 20 s) -> `4` (about 111 s). Meaning of the states: not known, not looked up inside mvg.
- Not known: whether state 4 / VAN -101 is the protection module refusing the environment, or a failure caused by a Darling defect (network, a system call), or a result of the UX restart. State 4 came at the same time in both runs.
- Rule (learner's, 2026-10-04) stands: compatibility only; no look inside mvg, no making Darling answer falsely to it. Told the learner again on 2026-10-07.

## Run 109 (ddc161d4b, Metal on): server load down, freeze unchanged; both ends of the app<->UX link spin (2026-10-07 09:26)

- Learner (on VAN -101): "Lets not give the mvg module false answers, Lets try to get it to a working state." Claude reads this as: fix Darling's real defects, honest answers only.
- `PthreadCanceled` lines: 0 (was about 18k/s). darlingserver average 238 ticks per 2 s (was 280-370). Server log still 2.4 MB/s: mutex "without an active thread" 6k/s each, `shim___pthread_testcancel` 3.4k/s, `proc_get_effective_thread_policy` 3k/s.
- Same failure, same clock: jade-home timeout 80 s; LeagueClient log has ZERO lines from 80 s to 110 s; `MvgState: 4` at 112 s (111/112 s in runs 106, 108, 109); renderers FATAL `connection retry failed` (frames [5,26] and [6,6]); "Render process terminated" 131 s. Claude's gdb/strace attaches in this run may have shifted times.
- strace: LeagueClientUx main thread and LeagueClient thread "ExternalClient" both loop `select(fd, timeout 0)` + 4 `clock_gettime` + 1 `gettimeofday`, about 5000 loops/s, a full core each. LeagueClient's fd is a non-blocking TCP connection 127.0.0.1:47378 -> 127.0.0.1:38751. No sleep call in the loop.
- NOT known: whether the app polls like this on a Mac (its design) or a wait in Darling returns at once (timed condition wait / sleep / run loop). `callcount.py` returned 0 for every symbol incl. select: result not trusted.
- `tools/livestack.py <pid> <tid> <linux syscall>`: stops a live thread at a syscall and names the stack. Written, NOT yet used successfully (target had exited).
- Pending decision (asked 2026-10-07): what to look at in the next run.

## Run 110 (ddc161d4b, Metal on): A and B looked at (2026-10-07 09:54)

- Learner chose "A and then B" (no reason given). Claude shut down run 109's container first (Riot Client had lost its renderer; learner's rerun was refused).
- A (LeagueClient in its quiet window): per-thread CPU every 5 s from +47 s to +147 s shows only `ExternalClient` busy (about 74 % of a core); every other thread idle. Main thread at +95 s: `condition_variable::__do_timed_wait` -> `__psynch_cvwait`. So the backend is waiting, not stuck computing. The quiet log is not a stall of that process.
- B (`ExternalClient` spin): stack at +95 s is `select$DARWIN_EXTSN` called straight from the app's own code under `_pthread_start`; no Darling sleep/condition wait between. strace shows the app passes timeout {0,0}. As far as Darling's calls show, the polling is the app's own. Not proven that a Mac behaves the same.
- Pattern seen across runs: the renderer FATAL frame is the cross-site iframe `https://embed.rgpub.io/wwpub-hall-of-legends-embed-2026/` (frame ids match in run 109: [5,26] then [6,6]). Metal-off run 00-49-41 loaded the same iframe, got `ERR_SOCKET_NOT_CONNECTED` after 40 s and did NOT crash.
- Network check from the guest with curl: IPv4 200 in 0.23 s; IPv6 fails at once (host has no IPv6 route either). curl does not show a network fault. Chromium's own network path not tested.
- `livestack.py` fixed (lines without ':'); core lc-110 deleted after reading (1.6 GB).

## Learner question and choice (2026-10-07 ~10:00)

- Learner: "Is Darling using the hosts cpu and cores for processing? I am unsure." Explained (Concept): native code, Linux threads, darlingserver for kernel-side calls. Verified: host 12 cores, guest `hw.ncpu: 12`, Cpus_allowed 0-11, 81 threads over 11 cores. Gap seen, not chased: `sysctl hw.activecpu` = unknown oid.
- Asked which single change separates "Metal off" from "older build"; learner answered by action: reran with `DARLING_ENABLE_METAL=0` on today's build (run 111). No stated expectation.
- Fact told to the learner: `=0` is off (`MTLDevice.mm:72`, only 1/y/Y enable). But Metal off does NOT turn off GPU page drawing: the window-less OpenGL contexts (02c4009c4 onward) are unconditional, and yesterday's no-crash run was on 55f757314, before them.
- Claude shut down run 110's container for the learner's rerun.

## Run 111 (ddc161d4b, Metal OFF) and stock Electron iframe test (2026-10-07 10:06-10:25)

- Run 111: "Mac Metal is not supported" logged (switch really off). Still crashes: iframe frame 21474836507 created 80.6 s, renderer FATAL `frame 21474836507 [5,27] connection retry failed` at 114.9 s (34 s later), then `[6,6]`; restarted UX died the same way. So the Metal switch is NOT the cause. Learner shut the container down themselves; reported nothing on screen.
- `electron-iframe-hw-1` (new `electron-22.3.27/iframetest`, same Chromium 108, GPU drawing on): the same cross-site iframe loads (200, did-frame-finish-load, separate renderer process), no crash in 70 s. So a cross-site iframe as such works; the failure is specific to the League UI (CEF, app-driven message loop).
- Claude's reading, NOT verified (CEF source recalled from memory, not read): the renderer asks the browser process to attach the frame and gives up when no answer comes; 34 s without the browser's main thread serving that request.
- Tools: `livestack.py` takes `now`; `uxwatch.sh <file>` samples the UI main thread (strace summary + named stack) 5 times after the iframe appears.
- Owed: one run with uxwatch armed.

## Run 112 (ddc161d4b, Metal on): CAUSE OF THE FREEZE located in the guest (2026-10-07 10:21)

- `uxwatch` samples after the iframe (10:22:47): samples 1-4 the UI main thread runs its usual loop; the `select(fd, timeout 0)` loop is called from inside Chromium Embedded Framework, not app code (corrects run 109's note). Sample 5 (10:23:12): no syscall returns for a whole second; stack `close$NOCANCEL` -> `sys_close_nocancel` -> `kqueue_closed_fd` (libkqueue) -> `pthread_mutex_lock` -> `__psynch_mutexwait`. Still blocked 80 s later (0 ticks); core taken then.
- Core, all 43 threads: global `kq_mtx` is free (owner 0). Threads 2341359 and 2341364 (Chromium threads in `kevent64`) are at `evfilt_machport_copyout+0x137`, return chain `linux_kevent_copyout+0xbc` -> `kevent64_impl+0x20c`, i.e. inside copy-out, which runs with that kqueue's lock held (`kevent.c:431-440`). Both are in a blocking `recvfrom` (orig_rax 45, flags 0) on fds 24 and 90 = the read of the server's reply to `mach_port_read` (`libkqueue/src/linux/machport.c`).
- Chain: reply never arrives -> thread sleeps holding its kqueue lock -> any `close()` in the process walks all kqueues and takes each lock (`kqueue_closed_fd`) -> UI main thread blocks for good -> renderer's frame attach is never answered -> renderer FATAL `connection retry failed` -> client restarts the UI.
- Earlier local commit 458101b (libkqueue, branch local/league-machport-nowait) already made the FIRST read in that function non-blocking for the same lock reason; the reply read was left blocking.
- NOT known: why the server sends no reply. Server code read: `Kqchan::MachPort::_read` (`darlingserver/src/kqchan.cpp:345`) answers from a kernel microthread after `dtape_kqchan_mach_port_fill`; it throws (no reply) if the thread lookup fails. No such error in the log; kqchan lines are debug-level, so the info-level log shows nothing either way. Thread 2341364's last server-logged activity: 10:23:06. The 16 `procmem ... Operation not permitted` errors are from container boot, unrelated.
- Core deleted after reading (1.2 GB).
- Pending decision (asked 2026-10-07): how to go after the missing reply.

## Server side of the lost reply: one defect found by reading (2026-10-07 ~10:35, no code yet)

- Learner: "Lets fix both of them, starting with the server side ... And then the next one."
- Live, on the still-stuck UI process 2341339 (stuck 8 min): guest fds 24 and 90 and their server ends (darlingserver fds 1416, 1548) all show Recv-Q 0 / Send-Q 0. So the server took the read request off the socket and has not sent a reply; nothing is in transit.
- Read `darlingserver/src/kqchan.cpp` (`_read`, outbox send path) and `src/message.cpp` (`push`, `sendMany`): no fault seen there.
- Defect by reading, `darlingserver/duct-tape/src/locks.c` `dtape_mutex_try_lock` (lines 116-127): the normal path reads and writes `dtape_owner` WITHOUT taking `dtape_queue_lock`, then UNLOCKS `dtape_queue_lock`, which it never locked. Effects: can "acquire" while a no-thread holder owns the mutex (those hold it via the queue lock and never set owner); can release the queue lock under a thread that is inside `dtape_mutex_lock`/`unlock` editing the wait queue (lost wake-up = a kernel microthread never resumes). Same as origin/main; code from 2022.
- Reached through `lck_mtx_try_lock` and `lck_spin_try_lock`, hence XNU's `io_lock_try`/`ip_lock_try`/`imq_lock_try`/`waitq_lock_try` (8 call sites in ipc/, waitq.c, kern_event.c) and `memory.c:1972`.
- NOT proven: that this defect is why the reply went missing. Related noise: "Trying to lock/unlock mutex without an active thread" about 6k/s each (the no-thread path).
- Pending decision (asked 2026-10-07): implementation checkpoint for the try-lock repair.

## Try-lock repaired (2026-10-07 10:31, darlingserver commit under superproject deb463101, installed, NOT run)

- Learner chose "Implement this step" without questions.
- `duct-tape/src/locks.c` `dtape_mutex_try_lock`: takes the queue lock with a try (busy = not acquired), checks/sets owner under it, unlocks on both outcomes. No unit test exists for this code; none added.
- Claude shut down run 112's container before installing. Installed server binary compared equal to the built one.
- Owed: one League run (Metal on). Then step 2, the guest-side trap (blocking reply read under the kqueue lock), still to be designed with the learner.

## Run 113 (deb463101, Metal on): try-lock repair did NOT remove the freeze (2026-10-07 10:32)

- Renderer terminated at 118 s (first UI) and 88 s (second UI); cores 2354609, 2355225 SIGTRAP. `MvgState: 4` at 201 s. So the try-lock defect was real but not (alone) the cause.
- Run ended oddly: app exit 137, server log stops 10:39:59, `shutdown.out` = "Cannot verify this prefix's server for shutdown.", no server core. Learner had typed `darlingserver shutdown` (command not found); how the server stopped is not known. `.init.pid` left stale.
- Server code read further: `Kqchan::MachPort::_read` fills the reply in a kernel microthread while impersonating the guest thread; `dtape_kqchan_mach_port_fill` -> `filt_machportprocess` receives the message and copies it out to the guest. If that needs memory in the guest (out-of-line data), the server must make a server-to-client call (`Thread::_s2cPerform`, `src/thread.cpp:1066`): with no active call on the target thread it sends `SIGRTMIN+1` and waits for the thread's signal handler to check in.
- HYPOTHESIS, not verified: the guest thread is asleep in a raw `recvfrom` on the kqchan socket (not its RPC socket); if it cannot take that signal, the server waits for the guest and the guest waits for the server. Guest `sigprocmask` converts only BSD signals 1-31, so an app cannot block the signal that way; who would block it is unknown.
- `tools/stuckwatch.sh <prefix>`: when the UI main thread is idle 6 s, records every thread's syscall, SigPnd, SigBlk, and a core. Armed for run 114 (`baseline/stuck-114`).

## Gap: work by another tool, 2026-10-07 10:58-12:19 (reconstructed by Claude from git and run folders only)

- Learner (12:19): "Continue from where GPT 6 Astra has stopped." That tool left no notes here; nothing below is from its own account. Learner's decisions during that stretch: not recorded.
- Superproject commits after deb463101: a3880aa35 and 9283da214 (darlingserver S2C: release impersonation while exchanging client memory calls; select the impersonated receiver for kqchan memory operations; `tests/machport-ool.c`), 7bcb27d15 (startup: inherited hard FD limit), 5f985b25f (xnu c2a9161: workqueue stack reset on x86_64 thread reuse, with a test), c15ea415e (darlingserver 7c34b3a: serialize wait suspension with wakeup delivery; `tests/wait-suspend.c`). These follow Claude's unverified S2C hypothesis; whether that hypothesis was ever confirmed is not known.
- Runs 114-123 exist. Run 123 (c15ea415e, 12:11): renderers still die, `FATAL:frame_impl.cc(640)] frame ... connection retry failed` ([6,6], [5,36], [5,28]); "Render process terminated" at 152 s and 91 s; `MvgState: 4` at 109 s. So the freeze is NOT fixed as of run 123.
- PROVENANCE FAULT found by Claude at 12:20: `ninja -n` showed 611 pending edges (xnu change built for part of the tree only; dyld's copy and 600 relinks pending). Runs 119-123 therefore ran on an incomplete build. Claude finished the build (`ninja -n` = no work), reinstalled c15ea415e at about 12:30; installed server and dyld compare equal to the build.
- The stuckwatch for run 114 was stopped by the learner; no `stuck-114` output exists.
- Not known: what the mechanism of the freeze is on this build (same kqueue-lock chain or different).

## Run 124 (c15ea415e, COMPLETE build, Metal on): same stall; signal hypothesis refuted (2026-10-07 12:24)

- `stuckwatch` at 12:26:12 (UI age 107 s): main thread in `close` -> `kqueue_closed_fd` -> `__psynch_mutexwait`; `Chrome_IOThread` 2544472 at `evfilt_machport_copyout+0x137` in blocking `recvfrom` on fd 92, inside the locked copy-out. Same chain as run 112. So the other tool's five commits did not remove it.
- Every thread: `SigPnd` 0 and `SigBlk` 0, process `ShdPnd` 0. Claude's hypothesis (server's signal cannot reach the thread) is REFUTED as stated: no signal is waiting.
- darlingserver at that time: 2 OS threads, one in epoll_wait, one in futex_wait = idle. So the server is not busy or spinning; whatever should produce the reply is suspended or was never started. gdb cannot attach to the server (ptrace: Operation not permitted, yama scope 1).
- Server's info log shows nothing of the read (kqchan lines are debug level).
- Added a diagnostic knob (darlingserver commit under superproject HEAD, installed): `DSERVER_LOG_ALWAYS=<category,...>` logs those categories at every level. Done without a checkpoint: instrumentation to get evidence, no behaviour change. `rc-run.sh` now honours a preset `DSERVER_LOG_LEVEL`.
- Claude shut down run 124's container for the install.
- Owed: one run with `DSERVER_LOG_LEVEL=err DSERVER_LOG_ALWAYS=kqchan,kqchan:mach_port,s2c`, stuckwatch armed; then read the server's last lines for the stuck thread's read request.


## Resume 2026-10-12: upstream libkqueue integrated; wake-before-block repaired

- User requested resuming League compatibility work, then pointed out a pushed kqueue fix. Fetched remote histories without recursive submodule checkout.
- Integrated upstream libkqueue `96cee19` (listener allocation bounds, removal bounds, fork mutex reset) via local merge `836115e`, superproject `1302912a3`. Preserved local `458101b` Mach notification nonblocking behavior. ASan reproduces old overflow; fixed registry passes 1,000-entry growth/removal exercise.
- Real Play test on `1302912a3` still stalled: run `resume-20261012-3`, backend 61643, UX 61743, 12 received-but-not-started server reads. Thus the pushed fix alone did not resolve the stall.
- Found separate wake-before-block bug: thread_unblock left TH_WAIT set when resume happened before host suspension. Old harness manually cleared state, masking it. New failing regression plus production flag clear committed as darlingserver `a241c35`, superproject `82a289812`. O0/O2 ASan/UBSan wake and timer checks pass; old `919ab32` fails the corrected test.
- Full builds and subsequent dry runs complete. Staged server/kernel byte comparisons pass. Guest out-of-line Mach stress passes 4,000 deliveries after each change; disposable IPC prefix stopped.
- Run `resume-20261012-4` on `82a289812`: user pressed Play. Backend 71185, UX 71307, network helper 71488, network thread 71540, server 69995 (run-specific IDs). Earlier unhandled-worker pileup not reproduced during several minutes of inspection; main thread returns to CFRunLoop, sampled kqchan reads complete, no CEF connection-retry fatal in app stderr.
- Still NOT playable: League remains at logo/loading page, browser WebSocket errors every 30s. Backend reports MvgState 4 at 112.737s. Host read-only gameflow API returns 200 and WebSocket upgrade returns 101 in ~46ms. Guest network trace connects locally, writes 517 bytes, reads 2,877 bytes, then no further socket data in the captured interval. TLS/certificate completion or callback delivery is the next hypothesis, not a diagnosis.
- Live prefix left running for inspection; all tracers detached. Root server attach unavailable due Yama and sudo password requirement. No fake anti-cheat answers or app patches. Riot itself auto-updated on the first launch.
- Detailed handoff and evidence: `build/resume-20261012/REPORT.md`. No remote pushes made.


## 2026-10-12 09:42 follow-up: UI loads, renderer still fails

- User reported possible sleep, full League client with VAN error, then automatic restart. Journal queries found no suspend entries, so do not attribute failures to sleep.
- Correct prior provisional TLS blocker: initial UX connected at +940s; later UX instances connect in 4-8s and load Home Hubs in 24-55s. Backend/server stayed up.
- Latest renderer core 80120 confirms CEF deliberate int3/ud2, `frame_impl.cc(640) ... [5,10] connection retry failed`. This persistent CEF acknowledgement failure remains the UI restart blocker; Vanguard also remains disconnected/disabled.
- Repeated early small cores belong to trustd (sample70555), not League; potential relevance to first TLS delay is unproven.
- Details appended to `build/resume-20261012/REPORT.md`; no source/runtime change this follow-up.

## 2026-10-12 10:02 EDT — insert_member wait investigated

No production changes; root82a289812/server a241c35. User clicked Play for
port-member-20261012-1. Frame [5,27] timeout at14:00:34.946 UTC; browser92227
main thread progresses through timer callbacks, Mach receive and extract_member
in four spaced captures. Server window14:00:24–45 contains12,905 main-thread
insert_member request/reply pairs, max3.990ms, no errors/unmatched requests.
Thus old one-core insert_member wait is not an established stall. Independent
port-set queued-message/concurrent receiver test passed4000 cycles twice;
Mach absolute-time run max1.203ms. First CLOCK_MONOTONIC timings had apparent
one-second steps and are invalid, a separate unproven clock lead.
Details: build/resume-20261012/port-member/REPORT.md. Verbose calls/replies run
stopped and wrapper cleanup completed; Riot reopened with normal diagnostics
as port-member-quiet-20261012-1. Disposable ipc-prefix stopped. Next: CEF
FrameAttached delivery/UI dispatch correlation. User clicks Play; do not
click it automatically. No upstream publication.

## 2026-10-12 10:37 EDT — CEF diagnostic controls built

Runtime unchanged root82a289812/server a241c35. See
build/resume-20261012/cef-probe/{REPORT,README}.md. Standalone CAPI probe loads
League's unchanged CEF framework; single-process/in-process-GPU60-second final
control exited0 with attached4, loaded3, ready_origins3,118 child-tick callbacks,
clean shutdown and no frame timeout despite SCDynamicStore1001. Basic separate
parent/child Mach rendezvous (bootstrap lookup, dispatch source, queued receive
right transfer, kqueue readiness) passes. Multi-process CEF native/external pump
variants fail initial child connection before attachment; NOT yet the same
failure as League's subframe timeout. Some untraced pre-exec child stalls;
traced children reach exec. No inherited-lock deadlock proven. Original League
helper comparison died at GPU startup and is invalid as a frame control.
Next: capture renderer before EnsureConnected deadline, distinguish bootstrap
rendezvous from ChannelMac handshake and Mach kqueue delivery. Diagnostic
source is in build/resume-20261012/cef-probe; no production patch or upstream
publication. HTTP fixture and disposable ipc-prefix are stopped after tests.


## 2026-10-12 11:12 EDT — CEF spawn-child deadlock fixed

New root ccd1e09e5, XNU a1189d6; server still a241c35. Full build plus follow-up
pass, ninja-n clean, staged install complete. See cef-probe/REPORT.md.
Ancestor tracer captured pre-exec child117941 stuck at sys_posix_spawn ->
sys_dup2 -> kqueue_dup -> pthread mutex wait. Server check-in succeeded. The
spawn path skips fork callbacks, inheriting kq_mtx held by vanished threads.
Descriptor-only dup/dup2 helpers now serve forked spawn file actions/error-pipe
relocation. Ordinary dup and SETEXEC bookkeeping preserved. Regression
src/external/xnu/darling/tests/spawn-kqueue-lock.rb passes fixed code and fails
original spawn control (real fork + inherited modeled mutex).

Real multi-process CEF first fixed run:4 attachments,3 loads,117 ticks over60s,
no connection timeout/fatal. Two untraced30s startup repeats:4 attachments,
3 loads,45 ticks EACH, zero connection timeout/fatal; repeat-startup-results.json.
CEF shutdown still hangs after window closure in full run. Prefix and fixture
stopped cleanly via darling shutdown / HTTP fixture termination. No publication.
Next: real League fresh Play test to assess its later FrameAttachedAck timeout;
do not claim this spawn fix cures VAN-101 or every CEF failure. User clicks Play.
