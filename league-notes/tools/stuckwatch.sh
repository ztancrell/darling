#!/usr/bin/env bash
# Wait for a League UI process whose main thread stops using CPU, then record every thread's
# syscall and signal masks (is a signal pending but blocked?) and save a core.
# Usage: stuckwatch.sh <output prefix>. Start before pressing Play.
set -uo pipefail
out=$1; old=" $(for p in /proc/[0-9]*; do [[ $(readlink $p/exe 2>/dev/null) == */mldr ]] && echo -n "${p#/proc/} "; done)"
ticks() { awk '{sub(/.*\) /,""); print $12+$13}' "$1" 2>/dev/null; }
while :; do
  for p in /proc/[0-9]*; do
    pid=${p#/proc/}
    [[ $(readlink $p/exe 2>/dev/null) == */mldr && $old != *" $pid "* ]] || continue
    c=$(tr '\0' ' ' < $p/cmdline 2>/dev/null); [[ $c == *MacOS/LeagueClientUx\ * && $c != *--type=* ]] || continue
    (( $(ps -o etimes= -p $pid) > 40 )) || continue
    a=$(ticks $p/task/$pid/stat); sleep 6; b=$(ticks $p/task/$pid/stat); [[ -n $a && -n $b ]] || continue
    (( b - a == 0 )) || continue
    { echo "UX $pid main thread idle 6 s at $(date +%T), age $(ps -o etimes= -p $pid)s"; grep -E 'ShdPnd' $p/status
      for t in $p/task/*; do
        read sc a1 a2 a3 a4 rest < $t/syscall 2>/dev/null
        echo "${t##*/} $(tr ' ' _ < $t/comm) sc=$sc fd=$((a1)) flags=${a4:-} $(grep -E 'SigPnd|SigBlk' $t/status | tr '\n\t' '  ')"
      done; } > $out.txt
    timeout 180 gcore -o $out.core $pid > /dev/null 2>&1; echo "core done $(date +%T)" >> $out.txt; exit 0
  done; sleep 2
done
