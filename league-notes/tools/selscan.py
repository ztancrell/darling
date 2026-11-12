#!/usr/bin/env python3
# Find Objective-C methods a Mac app is likely to call that Darling does not implement.
# Usage: selscan.py <darling libexec root> <mach-o> [<mach-o> ...]
#
# Tier A: selectors the app's code references that no class in Darling implements, and the app does
#         not implement itself. Sending one of these always raises "unrecognized selector".
# Tier B: system classes the app references that are hollow stubs in Darling (only the generated
#         forwarding pair), so every instance message is swallowed and every class message raises.
#
# ponytail: selector-level only. A selector that exists on some other Darling class but not on the
# receiver's class (setView: on NSMenuItem) is NOT found; that needs the receiver's type, which is
# not recoverable from the binary without Apple's headers. Guarded calls (respondsToSelector:) and
# dead code are reported too.
import os, re, subprocess, sys, collections, json

ARCH = "x86_64"
root, apps = sys.argv[1], sys.argv[2:]

def run(*a):
    return subprocess.run(a, capture_output=True, text=True, errors="replace").stdout

def is_macho(p):
    try:
        with open(p, "rb") as f:
            return f.read(4) in (b"\xca\xfe\xba\xbe", b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbf")
    except OSError:
        return False

METHOD = re.compile(r" [tT] ([+-])\[(\w+)(?:\([^)]*\))? ([^\]]+)\]$")

def darling_methods():
    cache = os.path.join(os.path.dirname(os.path.abspath(__file__)), "darling-methods.json")
    stamp = os.path.getmtime(os.path.join(root, "System/Library/Frameworks/AppKit.framework/Versions/C/AppKit"))
    if os.path.exists(cache):
        d = json.load(open(cache))
        if d["stamp"] == stamp:
            return d["sel"], d["cls"]
    sel = collections.defaultdict(list)          # selector -> ["+Class", "-Class"]
    cls = collections.defaultdict(list)          # class -> ["-sel", "+sel"]
    seen = set()
    for top in ("System/Library/Frameworks", "System/Library/PrivateFrameworks", "usr/lib"):
        for d, _, fs in os.walk(os.path.join(root, top)):
            for f in fs:
                p = os.path.realpath(os.path.join(d, f))
                if p in seen or not is_macho(p):
                    continue
                seen.add(p)
                for l in run("llvm-nm", f"--arch={ARCH}", p).splitlines():
                    m = METHOD.search(l)
                    if m:
                        sel[m[3]].append(m[1] + m[2])
                        cls[m[2]].append(m[1] + m[3])
    json.dump({"stamp": stamp, "sel": sel, "cls": cls}, open(cache, "w"))
    return sel, cls

def app_selrefs(p):
    out = run("llvm-otool", "-arch", ARCH, "-v", "-s", "__DATA", "__objc_selrefs", p)
    return set(re.findall(r"__objc_methname:(\S+)", out))

def app_own_methods(p):
    out = run("llvm-objdump", "--macho", f"--arch={ARCH}", "--objc-meta-data", p)
    return set(re.findall(r"name 0x[0-9a-f]+ (\S+)\n\s+types 0x", out))

def app_system_classes(p):
    return set(re.findall(r"_OBJC_CLASS_\$_(\w+)", run("llvm-nm", f"--arch={ARCH}", "-u", p)))

sel, cls = darling_methods()
print(f"Darling: {len(sel)} distinct selectors implemented across {len(cls)} classes")
HOLLOW = {"-methodSignatureForSelector:", "-forwardInvocation:"}
for p in apps:
    refs, own, classes = app_selrefs(p), app_own_methods(p), app_system_classes(p)
    missing = sorted(s for s in refs if s not in sel and s not in own)
    hollow = sorted(c for c in classes if c in cls and set(cls[c]) <= HOLLOW)
    absent = sorted(c for c in classes if c not in cls)
    print(f"\n== {os.path.basename(p)}")
    print(f"selectors referenced: {len(refs)}; implemented by the app itself: {len(refs & own)}")
    print(f"Tier A, implemented nowhere in Darling: {len(missing)}")
    for s in missing:
        print(f"  {s}")
    print(f"Tier B, hollow stub classes the app references: {len(hollow)}")
    print("  " + " ".join(hollow))
    print(f"system classes referenced with no methods found in Darling: {len(absent)}")
    print("  " + " ".join(absent))
