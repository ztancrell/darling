#!/usr/bin/env python3
# Two-level-namespace check of one Mach-O's x86_64 slice against an installed Darling tree:
# for every undefined symbol, is it exported by the library the binary expects it in (following
# that library's re-exports)? This is what dyld enforces. Splits results by bind table.
# Usage: scan2level.py <mach-o> <darling libexec root>
import os, re, subprocess, sys, collections, functools
binary, root = sys.argv[1], sys.argv[2]
ARCH = "x86_64"
def run(*a): return subprocess.run(a, capture_output=True, text=True, errors="replace").stdout
def cmds(path):
    out = run("llvm-otool", "-l", path)
    parts = re.split(r"^\S.*\(architecture (\S+)\):$", out, flags=re.M)
    if len(parts) > 1:
        out = next((parts[i + 1] for i in range(1, len(parts), 2) if parts[i] == ARCH), "")
    return re.findall(r"cmd (LC_\w*DYLIB)\n\s+cmdsize \d+\n\s+name (\S+)", out)
@functools.lru_cache(maxsize=None)
def exports(path):
    path = os.path.realpath(path)
    s = set(run("llvm-nm", "-gU", "-j", f"--arch={ARCH}", path).split())
    for c, n in cmds(path):
        if c == "LC_REEXPORT_DYLIB" and os.path.exists(root + n):
            s |= exports(root + n)
    return frozenset(s)
libs = {}                                  # short name used by nm -m -> install name
for c, n in cmds(binary):
    if c != "LC_ID_DYLIB":
        libs[os.path.basename(n).split(".")[0]] = n
def table(flag):
    out = set()
    for l in run("llvm-objdump", "--macho", f"--arch={ARCH}", flag, binary).splitlines():
        p = l.split()
        if len(p) > 3 and p[-1].startswith("_"):
            out.add(p[-1])
    return out
eager, lazy = table("--bind"), table("--lazy-bind")
res = collections.defaultdict(list)
for l in run("llvm-nm", f"--arch={ARCH}", "-u", "-m", binary).splitlines():
    m = re.search(r"\(undefined\) (weak )?external (\S+) \(from ([^)]+)\)", l)
    if not m: continue
    weak, sym, lib = bool(m[1]), m[2], m[3]
    name = libs.get(lib)
    if name is None or name.startswith("@"): continue
    path = root + name
    if os.path.exists(path) and sym in exports(path): continue
    when = "at load" if sym in eager else "when called" if sym in lazy else "unknown"
    elsewhere = ""
    res[("weak" if weak else "required", when)].append((lib, sym, "" if os.path.exists(path) else " [library absent]"))
for key in [("required", "at load"), ("required", "when called"), ("required", "unknown"), ("weak", "at load"), ("weak", "when called"), ("weak", "unknown")]:
    v = sorted(res.get(key, []))
    print(f"{key[0]}, {key[1]}: {len(v)}")
    if key == ("required", "at load"):
        for lib, sym, note in v: print(f"  {lib}\t{sym}{note}")
    else:
        c = collections.Counter(lib for lib, _, _ in v)
        print("  " + ", ".join(f"{k} {n}" for k, n in c.most_common()))
