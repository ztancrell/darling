#!/usr/bin/env python3
# Who calls a Darling function in a live guest process: stop at it a few times and name the callers.
# Usage: whocalls.py <pid> <image basename>:<symbol substring> [hits=5] [frames=10]
# Companion of callcount.py (same address arithmetic, same caveats). Frames come from gdb's own
# unwinder, which has no symbols for Mach-O code, so deep frames can be wrong.
import os, re, subprocess, sys, bisect
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import corelib

pid, spec = int(sys.argv[1]), sys.argv[2]
hits = int(sys.argv[3]) if len(sys.argv) > 3 else 5
frames = int(sys.argv[4]) if len(sys.argv) > 4 else 10
maps = []   # (start, end, path)
for line in open(f"/proc/{pid}/maps"):
    p = line.split(None, 5)
    if len(p) == 6:
        a, b = (int(x, 16) for x in p[0].split("-")); maps.append((a, b, p[5].strip()))
base = {}
for a, b, path in maps:
    base[path] = min(base.get(path, a), a)

def name(addr):
    for a, b, path in maps:
        if a <= addr < b:
            addrs, names, text0 = corelib.symbols(path)
            i = bisect.bisect_right(addrs, addr - base[path] + text0) - 1
            return f"{os.path.basename(path)}`{names[i] if i >= 0 else '?'}"
    return hex(addr)

image, want = spec.split(":", 1)
path = next(p for p in base if os.path.basename(p) == image)
addrs, names, text0 = corelib.symbols(path)
sym = next((a, n) for a, n in zip(addrs, names) if want in n)
cmds = ["set pagination off", "set confirm off", "handle all nostop noprint pass",
        f"break *{base[path] + sym[0] - text0:#x}"]
cmds += ["continue", f"bt {frames}"] * hits + ["detach", "quit"]
out = subprocess.run(["gdb", "-q", "-batch", "-p", str(pid)] + [x for c in cmds for x in ("-ex", c)],
                     capture_output=True, text=True, timeout=300).stdout
print("callers of", sym[1])
seen = {}
stack = []
for line in out.splitlines() + ["#0"]:
    m = re.match(r"#(\d+)\s+(?:(0x[0-9a-f]+) in )?", line)
    if not m:
        continue
    if m[1] == "0" and stack:
        key = tuple(stack); seen[key] = seen.get(key, 0) + 1; stack = []
    if m[2]:
        stack.append(name(int(m[2], 16)))
for key, n in seen.items():
    print(f"--- {n}x")
    for f in key:
        print("   ", f)
