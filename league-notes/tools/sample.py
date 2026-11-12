#!/usr/bin/env python3
# Where a live guest thread spends its time: attach, backtrace, detach, N times; then the most common
# stacks (innermost named frames first). Usage: sample.py <pid> [samples=15] [frames=14] [thread LWP=pid]
# ponytail: each sample stops the whole process for the attach (~0.2 s), so this shows waits and hot
# spots, not exact shares; frames come from gdb's frame-pointer unwinding of unsymbolised code.
import os, re, subprocess, sys, bisect, collections, time
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import corelib
pid = int(sys.argv[1]); n = int(sys.argv[2]) if len(sys.argv) > 2 else 15
frames = sys.argv[3] if len(sys.argv) > 3 else "14"; lwp = sys.argv[4] if len(sys.argv) > 4 else str(pid)
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
            return names[i] if i >= 0 else os.path.basename(path)
    return None
stacks = collections.Counter()
for _ in range(n):
    out = subprocess.run(["gdb", "-q", "-batch", "-p", str(pid), "-ex", f"thread find {lwp}", "-ex", "info threads"],
                         capture_output=True, text=True).stdout
    m = re.search(r"^\*?\s*(\d+)\s+Thread \S+ \(LWP %s\)" % lwp, out, re.M)
    out = subprocess.run(["gdb", "-q", "-batch", "-p", str(pid), "-ex", f"thread {m[1] if m else 1}", "-ex", "x/700gx $rsp"],
                         capture_output=True, text=True).stdout
    # gdb cannot unwind this code, so the stack is scanned for words that are code addresses (stale return
    # addresses of earlier calls show up too); runtime plumbing is left out to keep the callers visible
    st = []
    for w in re.findall(r"\t(0x[0-9a-f]{9,})", out):
        x = name(int(w, 16))
        if x and not re.search(r"^_?_?(linux_syscall|dserver_rpc|platform_|tiny_|szone|malloc|free|objc_msgSend|mh_execute_header|sig_restorer|darling_(mach|bsd)_syscall|mach_msg|OBJC_|GCC_except)|BitsetType|AsyncResource|kRootEntryName|ZTVN|^[a-zA-Z ]+$", x):
            x = re.sub(r"\(.*", "", x)[:60]
            if not st or st[-1] != x: st.append(x)
        if len(st) >= int(frames): break
    stacks[" < ".join(st)] += 1
    time.sleep(0.1)
for k, v in stacks.most_common(8): print(v, k)
