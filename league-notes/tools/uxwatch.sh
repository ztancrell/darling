#!/usr/bin/env bash
# After the League UI creates its cross-site iframe, look at the UI process's main thread while the
# renderer waits for it: syscall summary (1 s) and a named stack, every 6 s, 5 times.
# Usage: uxwatch.sh <output file>. Start before pressing Play.
set -uo pipefail
out=$1; here=$(dirname "$(readlink -f "$0")")
L="$HOME/.darling-league-run2/Applications/League of Legends.app/Contents/LoL/Logs/LeagueClient Logs"
start=$(date +%s); log=
until [[ -n $log ]]; do
  for f in "$L"/*LeagueClientUx.log; do
    [[ $(stat -c %Y "$f") -ge $start ]] && grep -aq 'TT_AUTO_SUBFRAME' "$f" && log=$f
  done; sleep 1
done
pid=$(basename "$log" | sed -E 's/.*_([0-9]+)_LeagueClientUx.log/\1/')
echo "iframe seen $(date +%T), UX pid $pid, log $(basename "$log")" > "$out"
for i in 1 2 3 4 5; do
  [[ -d /proc/$pid ]] || break
  echo "== sample $i $(date +%T) state=$(awk '{sub(/.*\) /,""); print $1}' /proc/$pid/stat)" >> "$out"
  timeout 1 strace -c -p $pid -o "$out.st" 2>/dev/null; awk 'NR>2 && $4+0>0 {print $4, $NF}' "$out.st" | sort -rn | head -8 | tr '\n' ';' >> "$out"; echo >> "$out"
  timeout 40 python3 "$here/livestack.py" $pid $pid now >> "$out" 2>&1
  sleep 4
done
rm -f "$out.st"; echo "done $(date +%T)" >> "$out"
