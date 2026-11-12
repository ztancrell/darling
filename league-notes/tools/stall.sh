#!/usr/bin/env bash
# Log what the League UI browser process's main thread is doing, every 2 s, without stopping it.
# Usage: stall.sh <output file> [seconds, default 1800]. Start it before or during a run; it waits for the process.
# Columns: time pid state cpu-ticks-since-last syscall-nr(-1 = running in user space) wchan | darlingserver ticks-since-last
set -uo pipefail
out=$1; end=$((SECONDS + ${2:-1800})); declare -A last
ticks() { awk '{sub(/.*\) /,""); print $12+$13}' "$1" 2>/dev/null; }   # utime+stime, after the comm field
while (( SECONDS < end )); do
  for p in /proc/[0-9]*; do
    [[ $(readlink $p/exe 2>/dev/null) == */mldr ]] || continue
    cmd=$(tr '\0' ' ' < $p/cmdline 2>/dev/null) pid=${p#/proc/}
    [[ $cmd == *MacOS/LeagueClientUx\ * && $cmd != *--type=* ]] || continue
    t=$(ticks $p/task/$pid/stat); [[ -n $t ]] || continue
    st=$(awk '{sub(/.*\) /,""); print $1}' $p/task/$pid/stat 2>/dev/null)
    sc=$(cut -d' ' -f1 $p/task/$pid/syscall 2>/dev/null)
    ds=$(pgrep -x darlingserver | head -1); dt=$(ticks /proc/$ds/stat)
    echo "$(date +%T) $pid $st $((t - ${last[$pid]:-$t})) ${sc:-?} $(cat $p/task/$pid/wchan 2>/dev/null) | $((dt - ${last[ds]:-$dt}))" >> "$out"
    last[$pid]=$t; last[ds]=$dt
  done
  sleep 2
done
