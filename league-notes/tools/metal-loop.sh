#!/usr/bin/env bash
# One turn of the Metal loop: build, install the COMMITTED tree, run eglprobe on ANGLE's Metal backend
# drawing a frame, and decode a crash if there is one. Usage: metal-loop.sh [extra VAR=value ...]
set -uo pipefail
repo=/home/zpt/projects/darling; cd $repo/build
T=$HOME/.local/share/darling/macos-apps/league/tools; S=${TMPDIR:-/tmp}
export DARLING_INSTALL_PREFIX=$PWD/destdir/usr/local DPREFIX=$HOME/.darling-league-run2 DSERVER_LOG_LEVEL=info
L=$PWD/src/startup/darling
dirty=$(git -C $repo status --porcelain --untracked-files=no --ignore-submodules=none | wc -l)
[[ $dirty == 0 ]] || { echo "uncommitted changes; commit before installing" >&2; git -C $repo status --short --ignore-submodules=none | head; exit 2; }
$L shutdown >/dev/null 2>&1
for i in 1 2 3; do ninja 2>&1 | grep -B2 -A14 "error:\|Undefined symbols" | grep -v "Wl,-dylib_file\|/usr/sbin/clang" | head -40; ninja -n | tail -1 | grep -q "no work" && break; done
ninja -n | tail -1 | grep -q "no work" || { echo "BUILD NOT COMPLETE"; exit 1; }
DESTDIR=$PWD/destdir ninja install >/dev/null 2>&1 || { echo "install failed"; exit 1; }
Lib="/Users/Shared/Riot Games/Riot Client.app/Contents/Frameworks/Riot Client.app/Contents/Frameworks/Electron Framework.framework/Versions/A/Libraries"
start=$(date '+%Y-%m-%d %H:%M:%S')
$L shell /usr/bin/env DARLING_ENABLE_METAL=1 DARLING_METAL_CLAIM_MAC2=1 PROBE_DRAW=1 "$@" /Volumes/SystemRoot$T/probes/eglprobe "$Lib" metal 2>&1 \
  | grep -v "implemented in both\|^dlopen\|^  egl\|^  gl\|^  EGL_\|client extensions\|MESA_\|^  vendor\|get display\|initialize:" | cut -c1-260 | awk '!seen[$0]++' | head -60
pid=$(coredumpctl list --since "$start" --no-pager 2>/dev/null | grep mldr | awk '{print $5}' | tail -1)
if [[ -n ${pid:-} ]]; then
	echo "--- crash: pid $pid $(coredumpctl info $pid --no-pager 2>/dev/null | sed -n 's/^ *Signal: //p')"
	grep -a "P:$pid(" $DPREFIX/private/var/log/dserver.log | grep -a "sigexc" | grep -a " r[a-z0-9]*: " | sed 's/.*\(T:[0-9]*\).*sigexc: */\1 /' | awk '!seen[$0]++' | tail -12 > $S/metal-loop-regs.txt
	coredumpctl dump $pid -o $S/metal-loop.core >/dev/null 2>&1
	(cd $T && python3 - "$S/metal-loop.core" "$S/metal-loop-regs.txt" <<'PY'
import corelib,re,sys
c=corelib.Core(sys.argv[1]); regs={}
for l in open(sys.argv[2]):
    m=re.match(r"(T:\d+) +(r\w+): (0x[0-9A-Fa-f]+)",l)
    if m: regs[m[2]]=int(m[3],16)
def nm(v):
    try: return c.name(v)
    except Exception: return None
for k in ("rip","rdi","rsi","rax"):
    if k in regs:
        v=regs[k]; s=""
        try: s=c.cstr(v)[:60]
        except Exception: pass
        print("  ",k,hex(v),nm(v),repr(s) if s.isprintable() and len(s)>3 else "")
rsp=regs.get("rsp",0); n=0
for i in range(0,500):
    try: v=c.q(rsp+8*i)
    except Exception: break
    x=str(nm(v))
    if '+' in x and n<26: print(f"     rsp+{8*i:#x}: {x[:120]}"); n+=1
PY
	)
	rm -f $S/metal-loop.core
fi
$L shutdown >/dev/null 2>&1
