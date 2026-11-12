#!/usr/bin/env bash
# One untimed run of the installed Riot Client. Collects evidence and always shuts the container
# down afterwards, so a later reinstall is never served stale frameworks.
# Usage: rc-run.sh <label>
set -uo pipefail
repo=/home/zpt/projects/darling
L=$repo/build/src/startup/darling
export DARLING_INSTALL_PREFIX=$repo/build/destdir/usr/local DPREFIX=$HOME/.darling-league-run2 DSERVER_LOG_LEVEL=${DSERVER_LOG_LEVEL:-info}
out=$HOME/.local/share/darling/macos-apps/league/baseline/riot-client-$1
logs="$DPREFIX/Users/zpt/Library/Logs/Riot Games/Riot Client"
[[ ! -e $out ]] || { echo "$out exists" >&2; exit 2; }
if [[ -z ${RC_ALLOW_RUNNING:-} ]] && pgrep -a darlingserver | grep -qF "$DPREFIX"; then echo "container already running; shut it down first" >&2; exit 2; fi
mkdir -p "$out"; started=$(date '+%Y-%m-%d %H:%M:%S'); touch "$out/.start"
logstart=$(stat -c %s "$DPREFIX/private/var/log/dserver.log" 2>/dev/null || echo 0)   # the prefix log only grows; keep this run's part
{ echo "started: $started"
  echo "darling commit: $(git -C $repo rev-parse HEAD) dirty=$(git -C $repo status --porcelain --untracked-files=no | wc -l)"
  echo "prefix: $DPREFIX (reused, Riot Client installed)"; } > "$out/meta.txt"
# Some container boots come up with no usable bootstrap port for shell-spawned processes (seen 2 of
# ~12 boots on 2026-10-04, cause unknown). A run on such a boot only measures that, so check with
# the probe first and reboot the container if it is one of those. Each bad boot is recorded.
probe="/Volumes/SystemRoot$(dirname "$(readlink -f "$0")")/probes/bstest"
for attempt in 1 2 3 4; do
	if "$L" shell "$probe" 2>/dev/null | grep -q 'notification_center .*kr=0 '; then
		echo "boot check: services reachable (attempt $attempt)" >> "$out/meta.txt"; break
	fi
	echo "boot check: services NOT reachable (attempt $attempt), rebooting container" >> "$out/meta.txt"
	"$L" shutdown > /dev/null 2>&1
done
# RC_GUEST_ENV: optional VAR=value words set inside the guest for this run (word-split on purpose).
echo "guest env: ${RC_GUEST_ENV:-none}" >> "$out/meta.txt"
"$L" shell /usr/bin/env ${RC_GUEST_ENV:-} "/Users/Shared/Riot Games/Riot Client.app/Contents/MacOS/RiotClientServices" > "$out/app.out" 2> "$out/app.err"
echo "app exit: $? at $(date '+%H:%M:%S')" >> "$out/meta.txt"
find "$logs" -type f -newer "$out/.start" -exec cp {} "$out/" \; 2>/dev/null
"$L" shutdown > "$out/shutdown.out" 2>&1; echo "shutdown exit: $?" >> "$out/meta.txt"
tail -c +$((logstart + 1)) "$DPREFIX/private/var/log/dserver.log" > "$out/dserver.log" 2>/dev/null
coredumpctl list --since "$started" --no-pager > "$out/coredumps.txt" 2>&1
cat "$out/meta.txt"
echo "--- exception / crash"
grep -oE "reason: '[^']+'" "$out/app.err" | tail -2
grep -E 'mldr' "$out/coredumps.txt" | awk '{print $5, $9}' | tail -3
ls "$out"
