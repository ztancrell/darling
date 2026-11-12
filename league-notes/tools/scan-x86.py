#!/usr/bin/env python3
# Static check of one Mach-O's x86_64 slice against an installed Darling tree:
# which linked libraries are absent, and which imported symbols no linked library exports.
# The repo's scan-imported-apps.py does this for arm64 slices only.
# Usage: scan-x86.py <mach-o> <darling libexec root>
# ponytail: follows re-exports by install path only; no weak-import or two-level-namespace
# attribution. Good enough to list gaps; use dyld's own messages as the final word.
import re, subprocess, sys

binary, root = sys.argv[1], sys.argv[2]
ARCH = "x86_64"

def run(*a):
    return subprocess.run(a, capture_output=True, text=True).stdout

def slice_cmds(path):
    out = run("llvm-otool", "-l", path)
    parts = re.split(r"^\S.*\(architecture (\S+)\):$", out, flags=re.M)
    if len(parts) > 1:
        out = next((parts[i + 1] for i in range(1, len(parts), 2) if parts[i] == ARCH), "")
    return re.findall(r"cmd (LC_\w*DYLIB)\n\s+cmdsize \d+\n\s+name (\S+)", out)

def exports(path, seen):
    if path in seen:
        return set()
    seen.add(path)
    syms = set(run("llvm-nm", "-gU", "-j", f"--arch={ARCH}", path).split())
    for cmd, name in slice_cmds(path):
        if cmd == "LC_REEXPORT_DYLIB":
            import os
            p = root + name
            if os.path.exists(p):
                syms |= exports(os.path.realpath(p), seen)
    return syms

import os
libs = [(c, n) for c, n in slice_cmds(binary) if c != "LC_ID_DYLIB"]
missing_libs, available, seen = [], set(), set()
for cmd, name in libs:
    p = root + name
    if os.path.exists(p):
        available |= exports(os.path.realpath(p), seen)
    else:
        missing_libs.append((name, "weak" if "WEAK" in cmd else "required"))

undefined = set(run("llvm-nm", "-u", "-j", f"--arch={ARCH}", binary).split())
missing_syms = sorted(undefined - available)

print(f"linked libraries: {len(libs)}, absent: {len(missing_libs)}")
for name, kind in missing_libs:
    print(f"  ABSENT ({kind}) {name}")
print(f"imported symbols: {len(undefined)}, not exported by any present linked library: {len(missing_syms)}")
for s in missing_syms:
    print(f"  {s}")
