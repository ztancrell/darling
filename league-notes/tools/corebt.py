#!/usr/bin/env python3
# Where a guest died, from its core alone: registers at the fault (read back from the signal frame the
# guest's handler was entered with) and the symbols found on that stack.
# Usage: corebt.py <core file> [max symbols=50] [bytes of code before the fault to disassemble=0]     (get the core with: coredumpctl dump <pid> -o file)
# ponytail: the stack listing is a scan for words that look like code addresses, not an unwind, so it
# also shows stale return addresses from earlier calls.
import corelib, re, subprocess, sys
core = sys.argv[1]; lim = int(sys.argv[2]) if len(sys.argv) > 2 else 50
dis = int(sys.argv[3]) if len(sys.argv) > 3 else 0
mldr = "/home/zpt/projects/darling/build/destdir/usr/local/libexec/darling/usr/libexec/darling/mldr"
out = subprocess.run(["gdb", "-q", "-batch", mldr, core, "-ex", "info registers"],
                     capture_output=True, text=True).stdout
live = {k: int(v, 16) for k, v in re.findall(r"^(r\w+)\s+(0x[0-9a-f]+)", out, re.M)}
rsp = live["rsp"]
c = corelib.Core(core)
def nm(v):
    try: return str(c.name(v))
    except Exception: return "None"
def show(r):
    print("rip", hex(r["rip"]), nm(r["rip"]))
    if dis:   # code before the fault, disassembled (calls show what was just tested)
        code = c.read(r["rip"] - dis, dis + 8)
        open("/tmp/corebt-code.bin", "wb").write(code)
        print(subprocess.run(["objdump", "-D", "-b", "binary", "-mi386:x86-64", f"--adjust-vma={r['rip'] - dis:#x}",
                              "/tmp/corebt-code.bin"], capture_output=True, text=True).stdout.split("<.data>:")[-1])
    for k in ("rdi", "rsi", "rdx", "rax", "rbx", "r12", "r14", "r15"):
        s = ""
        try:
            s = c.cstr(r[k])[:70]
            s = repr(s) if s.isprintable() and len(s) > 3 else ""
        except Exception: pass
        print(f"  {k} {r[k]:#x} {nm(r[k]) if nm(r[k]) != 'None' else ''} {s}")
    n = 0
    for j in range(4000):
        try: v = c.q(r["rsp"] + 8 * j)
        except Exception: break
        x = nm(v)
        if "+" in x and n < lim: print("    ", x[:140]); n += 1
names = "r8 r9 r10 r11 r12 r13 r14 r15 rdi rsi rbp rbx rdx rax rcx rsp rip".split()
for i in range(800):
    a = rsp + 8 * i
    try: v = c.q(a)
    except Exception: break
    if "sig_restorer + 0x0" not in nm(v): continue
    r = {n: c.q(a + 48 + 8 * k) for k, n in enumerate(names)}   # past the return address and ucontext head
    if not r["rip"] or nm(r["rip"]) == "None": continue   # a stale word, not a frame
    show(r)
    break
else:
    show(live)   # no handler frame: the core was taken at the fault itself
