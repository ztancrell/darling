#!/usr/bin/env python3
# Count calls into chosen Darling functions in a live guest process, without rebuilding anything.
# Usage: callcount.py <pid> <seconds> <image basename>:<symbol substring> ...
#   e.g. callcount.py 1234 30 'AppKit:-[NSWindow orderWindow:relativeTo:]' 'IOSurface:_IOSurfaceCreate'
# gdb does not read Mach-O symbols, so addresses are worked out here from /proc/<pid>/maps and
# llvm-nm, and gdb only gets raw breakpoints that count and continue.
# ponytail: a substring matching several symbols takes the first; the process runs slower while
# attached (every hit stops it), so timing-sensitive behaviour may change.
import os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import corelib

pid, secs, specs = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3:]
base = {}   # path -> lowest mapped address
for line in open(f"/proc/{pid}/maps"):
    p = line.split(None, 5)
    if len(p) == 6:
        path = p[5].strip(); start = int(p[0].split("-")[0], 16)
        base[path] = min(base.get(path, start), start)

cmds, labels = ["set pagination off", "set confirm off", "handle all nostop noprint pass"], []
for spec in specs:
    image, want = spec.split(":", 1)
    path = next((p for p in base if os.path.basename(p) == image), None)
    if not path:
        print(f"not mapped: {image}"); continue
    addrs, names, text0 = corelib.symbols(path)
    hit = next(((a, n) for a, n in zip(addrs, names) if want in n), None)
    if not hit:
        print(f"no symbol: {spec}"); continue
    labels.append(hit[1])
    cmds += [f"break *{base[path] + hit[0] - text0:#x}", "commands", "silent", "continue", "end"]
cmds += ["continue"]
with tempfile.NamedTemporaryFile("w", suffix=".gdb", delete=False) as f:
    f.write("\n".join(cmds) + "\n")
g = subprocess.Popen(["gdb", "-q", "-p", str(pid), "-x", f.name], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
try:
    g.wait(timeout=secs)
except subprocess.TimeoutExpired:
    g.send_signal(2)   # SIGINT: back to the gdb prompt
out, _ = g.communicate("info breakpoints\ndetach\nquit\n", timeout=60)
os.unlink(f.name)
hits, n = {}, None
for line in out.splitlines():
    p = line.split()
    if p and p[0].isdigit() and "breakpoint" in line:
        n = int(p[0]); hits[n] = 0
    elif "breakpoint already hit" in line and n:
        hits[n] = int(line.split("hit")[1].split()[0])
for i, label in enumerate(labels, 1):
    print(f"{hits.get(i, '?'):>6}  {label}")
