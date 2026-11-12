#!/usr/bin/env python3
# What a Darling function returns in a live guest: stop at its entry, run to its return, print rax.
# Usage: retval.py <pid> <seconds> <image basename>:<exact symbol> [hits=3]
# Companion of callcount.py. If the function never returns within <seconds>, prints where the
# thread is instead (gdb's unwinder; deep frames can be wrong).
import os, re, subprocess, sys, bisect
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import corelib
pid, secs, spec = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
hits = int(sys.argv[4]) if len(sys.argv) > 4 else 3
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
image, want = spec.split(":", 1)
path = next(p for p in base if os.path.basename(p) == image)
addrs, names, text0 = corelib.symbols(path)
addr = next(a for a, n in zip(addrs, names) if n == want)
cmds = ["set pagination off", "set confirm off", "handle all nostop noprint pass", f"break *{base[path] + addr - text0:#x}"]
cmds += ["continue", "echo ENTERED\\n", "finish", "printf \"RETURNED rax=0x%lx\\n\", $rax"] * hits
g = subprocess.Popen(["gdb", "-q", "-p", str(pid)] + [x for c in cmds for x in ("-ex", c)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
try: g.wait(timeout=secs)
except subprocess.TimeoutExpired: g.send_signal(2)
out, _ = g.communicate("bt 25\ndetach\nquit\n", timeout=120)
for line in out.splitlines():
    if line.startswith(("ENTERED", "RETURNED")): print(line)
    m = re.match(r"#(\d+)\s+(0x[0-9a-f]+) in ", line)
    if m: print("   ", m[1], name(int(m[2], 16)))
