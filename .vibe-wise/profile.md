# Learner Profile

Learning mode: active
Onboarding: complete

## Project
Situation: Existing
Building: VibeDarling, a fork of Darling (runs macOS software on Linux without a VM); fork goal is launching unmodified Homebrew cask apps
Codebase familiarity: A little experience (self-reported; no commits, PRs, or issues by the learner found in this repo or fork as of 2026-10-04)
Learning scope: Whole system

## Experience
Overall programming: Intermediate (self-reported)
Stack familiarity (self-reported): not a C/Objective-C developer but reads code; Linux strong and a
favourite; Mach-O/Darwin never used, knows the history; CMake used before.

Background evidence (local repos and earlier session prompts, 2026-07-26 to 2026-10-04; shows what
the learner has directed and tested, not demonstrated understanding):
- X11 C forks: openbox (8 commits), fastcompmgr (5 commits): hardening and perf fixes.
- ac-sandbox (40 commits): Wine/Proton sandbox for anti-cheat, syscall denial, TPM attestation.
- vkmercs: Vulkan PC port of a 2005 game, run through Proton.
- Working pattern in those sessions: states outcomes, AI implements; learner tests hands-on and
  reports symptoms with controlled observations. Cost-conscious about tokens.

## Goals
Primary: Understand Darling end to end (from "Whole system" scope)
Capability goal: Play League of Legends, ARAM specifically, (unmodified macOS client) in Darling on Linux
(stated 2026-10-04). Hopes a working path might lead Riot to unofficially tolerate Linux players.
No anti-cheat bypass: learner dropped that on request; work is compatibility only, and whether
Riot's anti-cheat accepts the environment is outside scope.

## Preferences
Checkpoint frequency: Normal
Question style: Mixed (open-ended where Linux/Wine/X11/Vulkan knowledge transfers,
multiple choice first in new Darwin territory)
Implementation style: AI writes code
Explanations: short by default; more depth on request

## Learning plan (confirmed by learner 2026-10-04; no unit started)
Bridge from known systems to Darling, one unit at a time, each attached to a real issue:
1. Container and prefix: launcher, namespaces, overlay, darlingserver per prefix (bridge: Wine prefix + wineserver).
2. Loading: Mach-O vs ELF/PE, mldr, dyld handoff.
3. The "kernel": Darwin syscalls and Mach IPC served by darlingserver.
4. Userland: libSystem, objc4, CoreFoundation/Foundation.
5. Windows on screen: Cocotron AppKit, X11/Wayland backends (bridge: openbox/fastcompmgr).
6. Graphics: Metal-to-Vulkan (bridge: vkmercs).
7. Diagnosis loop: coredumps, log levels, classifying an app failure.
Progress evidence per unit: learner explains the flow in own words and predicts a failure's owner
before the code is read.

## Strong Concepts
No demonstrated understanding recorded yet.

## Developing Concepts
- Picking a fix from stated options and saying why (did so for memory entries, sandbox, NSWindow init).
- Using recorded evidence (coredumps) before changing code: proposed it unprompted, 2026-10-04.

## Revisit
None recorded yet.
