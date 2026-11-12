#!/usr/bin/env bash
# A bounded run of the Riot Client: rc-run.sh in the background, screenshots of its windows after
# <seconds>, then a container shutdown so the run ends. Only the client's own windows are captured.
# Usage: rc-shot.sh <label> <seconds>     RC_STRACE=1: also trace failed file/socket calls of the
# whole container (the container is booted under strace; needs no privileges).
set -uo pipefail
label=$1; secs=$2; T=$(dirname "$(readlink -f "$0")")
repo=/home/zpt/projects/darling; L=$repo/build/src/startup/darling
export DARLING_INSTALL_PREFIX=$repo/build/destdir/usr/local DPREFIX=$HOME/.darling-league-run2 DSERVER_LOG_LEVEL=info
out=$HOME/.local/share/darling/macos-apps/league/baseline/riot-client-$label
trace=${TMPDIR:-/tmp}/rc-shot-strace-$$.txt
if [[ -n ${RC_STRACE:-} ]]; then
	strace -f -qq --seccomp-bpf -Z -e trace=openat,connect,socket,access,stat,readlink -o "$trace" "$L" shell /usr/bin/true > /dev/null 2>&1 &
	for i in $(seq 60); do [[ -e $DPREFIX/.shellspawn.pid ]] && break; sleep 1; done; sleep 3
	export RC_ALLOW_RUNNING=1
fi
bash "$T/rc-run.sh" "$label" > "${TMPDIR:-/tmp}/rc-shot-$$.out" 2>&1 &
run=$!
for i in $(seq "$secs"); do kill -0 $run 2>/dev/null || break; sleep 1; done
n=0
for w in $(xdotool search --onlyvisible --name '^Riot' 2>/dev/null); do
	xwininfo -id $w 2>/dev/null | grep -E 'xwininfo|Absolute|Width|Height|Map State' | tr '\n' ' '; echo
	xdotool windowmove $w 0 0 2>/dev/null; sleep 1   # import only captures what is on screen
	import -window $w "$out/window-$((n++)).png" 2>/dev/null
done
echo "screenshots: $n"
# guest processes at this moment (mldr rewrites its command line to the guest program's)
ps -eo pid,etimes,args | grep -E 'Riot Client|RiotClient' | grep -v grep | cut -c1-2000 > "$out/processes.txt"
"$L" shutdown > /dev/null 2>&1
wait $run; cat "${TMPDIR:-/tmp}/rc-shot-$$.out"; rm -f "${TMPDIR:-/tmp}/rc-shot-$$.out"
[[ -e $trace ]] && { wait; mv "$trace" "$out/strace.txt"; ls -la "$out/strace.txt"; }
