#!/usr/bin/env bash
# One baseline run of Riot's installer under the locally built Darling.
# Usage: run-baseline.sh <run-number>
# Fresh disposable prefix per run; always stopped with `darling shutdown`, never killed.
set -uo pipefail

n=${1:?run number}
repo=/home/zpt/projects/darling
launcher=$repo/build/src/startup/darling            # build tree's own non-setuid launcher
export DARLING_INSTALL_PREFIX=$repo/build/destdir/usr/local
export DPREFIX=$HOME/.darling-league-run$n
export DSERVER_LOG_LEVEL=info
league=$HOME/.local/share/darling/macos-apps/league
app="Install League of Legends na.app"
out=$league/baseline/run$n
hang_limit=${HANG_LIMIT:-180}

[[ -x $launcher ]] || { echo "no launcher at $launcher" >&2; exit 2; }
[[ ! -e $DPREFIX ]] || { echo "$DPREFIX already exists; each run needs a fresh prefix" >&2; exit 2; }
mkdir -p "$out"
started=$(date '+%Y-%m-%d %H:%M:%S')

{
	echo "run: $n"
	echo "started: $started"
	echo "darling commit: $(git -C "$repo" rev-parse HEAD)"
	echo "tree dirty files: $(git -C "$repo" status --porcelain --untracked-files=no | wc -l)"
	echo "installer sha256: $(sha256sum "$league/live.na.zip" | cut -d' ' -f1)"
	echo "prefix: $DPREFIX (fresh)"
	echo "install prefix: $DARLING_INSTALL_PREFIX"
} > "$out/meta.txt"

xwininfo -root -tree > "$out/windows-before.txt" 2>&1

# 1. Boot the container and initialize the prefix.
"$launcher" shell /usr/bin/true > "$out/boot.out" 2> "$out/boot.err"
echo "boot exit: $?" >> "$out/meta.txt"
"$launcher" shell /usr/bin/sw_vers > "$out/sw_vers.txt" 2>&1

# 2. Stage the unmodified app into the guest's /Applications.
"$launcher" shell /bin/cp -R "/Volumes/SystemRoot$league/extracted/$app" /Applications/ > "$out/stage.out" 2> "$out/stage.err"
echo "stage exit: $?" >> "$out/meta.txt"

# 3. Launch it and watch.
cmd="/Applications/$app/Contents/MacOS/RiotClientServices"
echo "command: darling shell \"$cmd\"" >> "$out/meta.txt"
"$launcher" shell "$cmd" > "$out/app.out" 2> "$out/app.err" &
pid=$!
elapsed=0; result=
while (( elapsed < hang_limit )); do
	if ! kill -0 "$pid" 2>/dev/null; then wait "$pid"; result="exited $?"; break; fi
	if (( elapsed == 30 )); then
		xwininfo -root -tree > "$out/windows-30s.txt" 2>&1
		import -window root "$out/screen-30s.png" 2>/dev/null
	fi
	sleep 5; elapsed=$((elapsed + 5))
done
if [[ -z $result ]]; then
	result="hung (still running after ${hang_limit}s)"
	xwininfo -root -tree > "$out/windows-end.txt" 2>&1
	import -window root "$out/screen-end.png" 2>/dev/null
fi
echo "result: $result after ${elapsed}s" >> "$out/meta.txt"

# 4. Stop the container cleanly, then collect what it left behind.
"$launcher" shutdown > "$out/shutdown.out" 2>&1
echo "shutdown exit: $?" >> "$out/meta.txt"
wait "$pid" 2>/dev/null

cp "$DPREFIX/private/var/log/dserver.log" "$out/dserver.log" 2>/dev/null || echo "dserver.log: not found" >> "$out/meta.txt"
coredumpctl list --since "$started" --no-pager > "$out/coredumps.txt" 2>&1
coredumpctl info --since "$started" --no-pager > "$out/coredumps-info.txt" 2>&1
pgrep -a darlingserver | grep -F "$DPREFIX" > "$out/leftover-server.txt" || rm -f "$out/leftover-server.txt"

cat "$out/meta.txt"
