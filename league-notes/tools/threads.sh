#!/usr/bin/env bash
# Every thread of a guest core: the named return addresses on its stack (a scan, so stale slots
# show up too). Usage: threads.sh <pid with a coredump> [slots per thread=14] [only threads whose stack matches REGEX]
set -uo pipefail
pid=$1; n=${2:-14}; want=${3:-.}; S=${TMPDIR:-/tmp}/threads-$$; mkdir -p $S; trap 'rm -rf $S' EXIT
coredumpctl dump $pid -o $S/core >/dev/null 2>&1 || { echo "no core"; exit 1; }
exe=$(coredumpctl info $pid --no-pager 2>/dev/null | sed -n 's/^ *Executable: //p')
gdb -q -batch -c $S/core "$exe" -ex 'thread apply all info registers rsp rip' 2>/dev/null | grep -E '^Thread|^rsp|^rip' > $S/regs.txt
cd "$(dirname "$(readlink -f "$0")")" && python3 - $S/core $S/regs.txt "$n" "$want" <<'PY'
import corelib, re, sys
c = corelib.Core(sys.argv[1]); lim = int(sys.argv[3]); want = re.compile(sys.argv[4])
threads, cur = [], None
for l in open(sys.argv[2]):
    if l.startswith("Thread"): cur = {"name": l.split("(")[0].strip()}; threads.append(cur)
    elif cur is not None: cur[l.split()[0]] = int(l.split()[1], 16)
def nm(v):
    try: return str(c.name(v))
    except Exception: return ""
for t in threads:
    out = [f"rip {nm(t.get('rip', 0))}"]
    for i in range(6000):
        try: v = c.q(t["rsp"] + 8 * i)
        except Exception: break
        x = nm(v)
        if "+" in x and "Electron Framework" not in x and len(out) <= lim: out.append(x[:110])
    if want.search("\n".join(out)): print(t["name"]); print("    " + "\n    ".join(out))
PY
