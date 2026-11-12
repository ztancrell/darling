#!/usr/bin/env python3
# Backtraces of a live guest's threads (gdb's unwinder, named here), only threads matching REGEX.
# Usage: livebt.py <pid> [regex=.] [frames=30]
import os, re, subprocess, sys, bisect
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import corelib
pid = int(sys.argv[1]); want = re.compile(sys.argv[2] if len(sys.argv) > 2 else "."); n = sys.argv[3] if len(sys.argv) > 3 else "30"
maps = []
for line in open(f"/proc/{pid}/maps"):
    p = line.split(None, 5)
    if len(p) == 6:
        a, b = (int(x, 16) for x in p[0].split("-")); maps.append((a, b, p[5].strip()))
base = {}
for a, b, path in maps: base[path] = min(base.get(path, a), a)
def name(addr):
    for a, b, path in maps:
        if a <= addr < b:
            addrs, names, text0 = corelib.symbols(path)
            i = bisect.bisect_right(addrs, addr - base[path] + text0) - 1
            return f"{os.path.basename(path)}`{names[i] if i >= 0 else '?'}"
    return hex(addr)
out = subprocess.run(["gdb", "-q", "-batch", "-p", str(pid), "-ex", f"thread apply all bt {n}"], capture_output=True, text=True, timeout=300).stdout
cur = []
def flush():
    if cur and want.search("\n".join(cur)): print("\n".join(cur))
for line in out.splitlines():
    if line.startswith("Thread "): flush(); cur = [line.split("(")[0].strip()]
    else:
        m = re.match(r"#(\d+)\s+(0x[0-9a-f]+) in ", line)
        if m and cur: cur.append(f"   {m[1]:>2} {name(int(m[2], 16))[:110]}")
flush()
