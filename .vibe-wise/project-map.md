# Project Map

Evidence: README.md, CLAUDE.md, CMakeLists.txt, src/CMakeLists.txt, src/startup/darling.c,
.gitmodules, .github/workflows/actions.yaml (inspected 2026-10-04). Not a full audit.

## Purpose
VibeDarling: development fork of Darling, a compatibility layer that runs macOS software on
Linux without a VM. Fork goal: run unmodified Homebrew cask applications.

## Requirements
- Keep Homebrew and the apps unmodified; fixes go in Darling or its libraries (CLAUDE.md).
- PRs go to this fork, linked to an issue; issue claim protocol in .claude/ISSUE_COLLABORATION.md.
- Merge gate: five approvals from five different people on the current head commit.
- Never install/publish an artifact built from a tree with uncommitted changes.

## Components
- Launcher `darling` (setuid when installed): src/startup/darling.c
- Mach-O loader `mldr`: src/startup/mldr/
- Userspace kernel server `darlingserver` (Mach IPC, POSIX/Darwin syscalls): src/external/darlingserver (submodule)
- In-container shell spawner: src/shellspawn; path translation: src/vchroot; init: src/launchd
- Darwin userland (libsystem_kernel from xnu, libc, dyld, objc4, corefoundation, ...): src/external/* (149 submodules)
- Frameworks: src/frameworks, src/private-frameworks; AppKit via src/external/cocotron; Metal via src/external/metal
- Host build tools (ld64, ar, wrapgen): src/buildtools, src/hosttools, src/external/cctools-port
- Tests: tests/ (C/ObjC programs and Ruby check scripts); in-prefix tests behind ENABLE_TESTS

## Main Flow
Verified in src/startup/darling.c and CLAUDE.md:

    user: darling shell <program>
      -> launcher (darling.c) --starts--> darlingserver (one per prefix, socket <prefix>/.darlingserver.sock)
      -> launcher --exec mldr as vchroot--> /usr/libexec/shellspawn (pid in <prefix>/.shellspawn.pid)
      -> shellspawn --spawns--> guest program (process shows as mldr; cmdline rewritten to guest name)
      -> guest --syscalls / Mach IPC--> darlingserver

Unknown (not inspected): how mldr hands off to dyld; how guest syscalls reach darlingserver
(? RPC over the socket); where launchd fits in container startup; how the overlay prefix is mounted.

## Service lookup (verified 2026-10-04)
    guest app --bootstrap_look_up--> launchd (src/launchd) --finds--> daemon (notifyd, securityd, ...)
launchd creates an "anonymous job" for callers it did not start (job_new_anonymous, core.c). In
non-root mode (build-tree launcher, darling.c g_nonroot) there is no PID namespace, guest PIDs are
host PIDs, and the prefix's /proc is a symlink to the host's. Root mode has its own PID namespace.

## Data and Trust Boundaries
- Prefix (DPREFIX) holds per-container state: .init.pid, .shellspawn.pid, .darlingserver.sock, reported macOS version.
- Frameworks come from the installed runtime (libexec tree as overlay lower layer), not the prefix.
- Setuid launcher: ignores DARLING_INSTALL_PREFIX under AT_SECURE; darlingserver runs as root with an
  environment allowlist (g_darlingserverEnvAllowlist).
- Guest app trust model beyond this: unknown.

## Build and Deployment
- CMake + Ninja, ~27k edges cold; confirm completion with `ninja -n`.
- Unprivileged run: `DESTDIR=<dir> ninja install`, then build-tree launcher with DPREFIX and DARLING_INSTALL_PREFIX.
- Packaging: debian/, rpm/; CI in .github/workflows/actions.yaml builds debs and source artifacts.
- Submodules use relative URLs; worktrees share submodule gitdirs (use an independent clone to build).

## Unknowns
- Internals of mldr, dyld handoff, darlingserver RPC protocol.
- Graphics path (X11/Wayland backends, Metal-to-Vulkan) structure.
- Test harness: how tests/ programs are built and run.
- Which area the learner will work in.

## Current work
- Goal: play League of Legends in Darling. Chain: installer app -> Riot Client backend
  (RiotClientServices + libRiotClientFoundation) -> Riot Client UI (Electron 22, separate process)
  -> League game client. As of 2026-10-04 on local/league-baseline d0cf1fae5: installer completes;
  backend starts, reaches Riot's services and exits cleanly; Electron UI loads, shares memory through Mach memory entries
  (darlingserver a4b692b), creates its NSWindow, shows its splash and starts loading its page, and
  spawns GPU, network and renderer helpers; the helpers then trap and the main process crashes in
  strlen(NULL). Non-root
  mode needed launchd's PID limit raised (2dc75710a) before any guest could reach launchd services. See progress.md and
  ~/.local/share/darling/macos-apps/league/baseline/REPORT.md.
- Local, unmerged commits: xnu 4d77d4b; cocotron 02d9472b, cc0eafbf, 39deb0d4, febdba36, 8f66c126,
  edc3cc56; foundation abca9cf35; IOKitUser e63ef13; superproject 18fb68253, b2e974652, 6121a1a38, d0cf1fae5, 2dc75710a plus
  pointer bumps. Placeholders (not real fixes): font registration, LSSetDefaultHandlerForURLScheme.
  No issues, claims, or PRs filed, by the learner's choice.
- Local build: build/, DESTDIR install at build/destdir, run with the build tree's launcher.
  Shut the container down before every reinstall. Working prefix: ~/.darling-league-run2.
- Helper scripts and evidence: ~/.local/share/darling/macos-apps/league/{tools,baseline}.
- 2026-10-04: Electron bring-up paused by the learner in favour of a real sandbox. Sandbox entry
  points: src/sandbox/sandbox.c (libsystem_sandbox: sandbox_init*, sandbox_check), generated stubs
  in /usr/lib/libsandbox.1.dylib. No enforcement exists yet; design not chosen.
- Sandbox (2026-10-04, local 8df0dc853): src/sandbox/profile.c reads profiles with vendored
  TinyScheme (src/sandbox/tinyscheme); src/sandbox/landlock.c enforces allow file rules with Landlock
  via libsystem_kernel's __linux_syscall and __darling_vchroot_expand; sandbox.c ties them to
  sandbox_init*/sandbox_check. Only file access is confined. Flow:
      app --sandbox_init_with_parameters(profile, params)--> profile.c --rules--> landlock.c
          --landlock_restrict_self--> Linux kernel (refuses files outside the list from then on)
- Graphics path for Electron (verified 2026-10-04): Electron's bundled ANGLE (libEGL/libGLESv2)
  -> its Metal backend -> Darling Metal.framework (src/external/metal, over libindium)
  -> Vulkan -> host RADV. Gated by DARLING_ENABLE_METAL=1 (MTLDevice.mm). ANGLE's software
  backend (SwiftShader) already initialises; its OpenGL and Vulkan backends do not.
- Route B (2026-10-05): Electron -> ANGLE's own Vulkan backend -> ANGLE's built-in Khronos loader
  -> /usr/share/vulkan/icd.d/radeon_icd.json -> /usr/lib/native/libvulkan_darling_icd.dylib (shim,
  src/native/vulkan/icd_shim.c) -> /usr/lib/native/libvulkan_radeon.dylib (wrap_elf of host RADV)
  -> host libvulkan_radeon.so -> AMD GPU. Steered by ANGLE_DEFAULT_PLATFORM=vulkan at launch.
  Verified by eglprobe (triangle pixel-exact). Presentation into a window is not covered.
- 2026-10-05 morning (superproject 43f518955): Electron and all helpers stay up and the page loads;
  windows are blank. Display path on a Mac, as Chromium uses it (from its error messages and Darling's
  sources, not yet traced end to end):
      renderer/GPU helper --frame in an IOSurface--> Mach port --> browser process
          --CALayer.contents = IOSurface--> window
  Darling today: IOSurface.framework --IOKit call--> iokitd IOSurfaceRoot (skeleton, returns nothing);
  QuartzCore CARenderer draws CGImage contents only; CAContext/CALayerHost are stubs.
- EGL name clash (fixed 88457944e): OpenGL.framework used to link the wrapped host libEGL, so an
  app's lookup of `eglInitialize` through ANGLE's libGLESv2 found the host's. It now dlopens it.
- File ports (fileport_makeport/makefd) are served by darlingserver (duct-tape/src/fileport.c).
- 2026-10-05 afternoon (superproject 0e4791db8), verified by electron-test and run54:
      GPU helper --IOSurfaceCreate--> Mach memory entry (header page + pixels)
          --entry port in a Mach message--> browser --IOSurfaceLookupFromMachPort--> same memory
          --CALayer.contents--> CARenderer texture upload --> X11 window
  IOSurface no longer uses iokitd. Chromium only draws once -[NSWindow occlusionState] says visible
  (value 8194) and the occlusion notification is posted. libkqueue (src/external/libkqueue) backs
  kevent for libdispatch; its Mach-port filter talks to darlingserver over a per-knote socket.
  Next wall: the Riot UI's WebSocket to RiotClientServices fails. Unknown: why.
