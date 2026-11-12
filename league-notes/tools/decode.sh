#!/usr/bin/env bash
# Decode one guest crash of a run: command line, faulting registers (from dserver.log), stack symbols.
# Usage: decode.sh <run directory> <pid> [max frames]
set -uo pipefail
B=$1; pid=$2; n=${3:-40}; S=${TMPDIR:-/tmp}/decode-$$; mkdir -p $S; trap 'rm -rf $S' EXIT
coredumpctl info $pid --no-pager 2>/dev/null | grep -E 'Signal:|Command Line:' | cut -c1-260
grep -a "P:$pid(" $B/dserver.log | grep -a "sigexc" | tail -60 > $S/sig.txt
grep -a " r[a-z0-9]*: " $S/sig.txt | sed 's/.*\(T:[0-9]*\).*sigexc: */\1 /' | awk '!seen[$0]++' | tail -20 > $S/regs.txt
grep -av " r[a-z0-9]*: \|gregs" $S/sig.txt | sed 's/.*sigexc: *//' | tail -6
coredumpctl dump $pid -o $S/core >/dev/null 2>&1 || { echo "no core"; exit 1; }
cd "$(dirname "$(readlink -f "$0")")" && python3 - "$S/core" "$S/regs.txt" "$n" <<'PY'
import corelib,re,sys
c=corelib.Core(sys.argv[1]); regs={}; lim=int(sys.argv[3])
for l in open(sys.argv[2]):
    m=re.match(r"(T:\d+) +(r\w+): (0x[0-9A-Fa-f]+)",l)
    if m: regs[m[2]]=int(m[3],16); tid=m[1]
def nm(v):
    try: return c.name(v)
    except Exception: return None
print("  thread", tid)
for k in ("rip","rdi","rsi","rdx","rcx","rax","rbx","r8","r9"):
    if k in regs:
        v=regs[k]; s=""
        try: s=c.cstr(v)[:80]
        except Exception: pass
        print("  ",k,hex(v),nm(v),repr(s) if s and s.isprintable() and len(s)>3 else "")
b=c.read(regs.get("rip",0)-16,32); print("  code at rip-16:",b.hex() if b else None)
rsp=regs.get("rsp",0); n=0
for i in range(0,1200):
    try: v=c.q(rsp+8*i)
    except Exception: break
    x=str(nm(v))
    if '+' in x and n<lim: print(f"     rsp+{8*i:#x}: {x[:150]}"); n+=1
PY
