#!/usr/bin/env bash
# Run stock Electron 22.3.27 (the Riot Client's version) with a one-window test app and Chromium's
# verbose log, which the Riot Client gives no way to turn on. Always shuts the container down.
# Usage: electron-test.sh <label> <seconds> [extra Electron flags...]
# ET_PRE: guest shell command run first (e.g. launchctl setenv). ET_GUEST_ENV: VAR=value words. ET_APP: testapp (default, one window) or nettest (page requests;
# needs NET_HTTP/NET_HTTPS ports of servers run from ../electron-22.3.27/srv).
set -uo pipefail
label=$1; secs=$2; shift 2
repo=/home/zpt/projects/darling; L=$repo/build/src/startup/darling
E=$HOME/.local/share/darling/macos-apps/electron-22.3.27
export DARLING_INSTALL_PREFIX=$repo/build/destdir/usr/local DPREFIX=$HOME/.darling-league-run2 DSERVER_LOG_LEVEL=info
out=$HOME/.local/share/darling/macos-apps/league/baseline/electron-$label
[[ ! -e $out ]] || { echo "$out exists" >&2; exit 2; }
pgrep -a darlingserver | grep -qF "$DPREFIX" && { echo "container already running" >&2; exit 2; }
mkdir -p "$out"
echo "darling commit: $(git -C $repo rev-parse HEAD) dirty=$(git -C $repo status --porcelain --untracked-files=no | wc -l); flags: $*; env: ${ET_GUEST_ENV:-none}" > "$out/meta.txt"
[[ -z ${ET_PRE:-} ]] || "$L" shell /bin/sh -c "$ET_PRE" > "$out/pre.txt" 2>&1
"$L" shell /usr/bin/env DARLING_TOLERATE_UNRECOGNIZED_SELECTORS=1 ${ET_GUEST_ENV:-} \
	"/Volumes/SystemRoot$E/Electron.app/Contents/MacOS/Electron" --enable-logging=stderr --v=1 "$@" "/Volumes/SystemRoot$E/${ET_APP:-testapp}" > "$out/app.out" 2> "$out/app.err" &
run=$!
for i in $(seq "$secs"); do kill -0 $run 2>/dev/null || break; sleep 1; done
n=0
for w in $(xdotool search --onlyvisible --name 'frame|Electron' 2>/dev/null); do
	xwininfo -id $w 2>/dev/null | grep -E 'xwininfo|Width|Height' | tr '\n' ' '; echo
	import -window $w "$out/window-$((n++)).png" 2>/dev/null
done
echo "screenshots: $n"
ps -eo pid,etimes,args | grep -E 'Electron' | grep -v grep | cut -c1-300 > "$out/processes.txt"
"$L" shutdown > /dev/null 2>&1; wait $run
coredumpctl list --since "-$((secs + 30))s" --no-pager 2>/dev/null | grep mldr | awk '{print $5, $9}' > "$out/coredumps.txt"
echo "stdout: $(tr '\n' '|' < "$out/app.out" | cut -c1-200)"; echo "cores: $(wc -l < "$out/coredumps.txt"); stderr lines: $(wc -l < "$out/app.err")"
