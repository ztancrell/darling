#!/usr/bin/env bash
# Watch the next LeagueClient (backend) process through its quiet window without stopping it,
# except: one stack of the "ExternalClient" polling thread at 40 s, one core at 95 s.
# Usage: lcwatch.sh <output prefix>   -> <prefix>.txt, <prefix>.core.<pid>
set -uo pipefail
out=$1; here=$(dirname "$(readlink -f "$0")"); pid=
old=" $(for p in /proc/[0-9]*; do [[ $(readlink $p/exe 2>/dev/null) == */mldr ]] && echo -n "${p#/proc/} "; done)"
until [[ -n $pid ]]; do
  for p in /proc/[0-9]*; do
    [[ $(readlink $p/exe 2>/dev/null) == */mldr && $old != *" ${p#/proc/} "* ]] || continue
    [[ $(tr '\0' ' ' < $p/cmdline 2>/dev/null) == *MacOS/LeagueClient\ * ]] && pid=${p#/proc/}
  done; sleep 1
done
t0=$SECONDS; echo "LeagueClient $pid seen $(date +%T)" > $out.txt; declare -A last; did40= did95=
ticks() { awk '{sub(/.*\) /,""); print $12+$13}' "$1" 2>/dev/null; }
while [[ -d /proc/$pid ]] && (( SECONDS - t0 < 150 )); do
  e=$((SECONDS - t0)); echo "== +${e}s $(date +%T)" >> $out.txt
  for t in /proc/$pid/task/*; do
    k=${t##*/}; n=$(ticks $t/stat); [[ -n $n ]] || continue
    d=$((n - ${last[$k]:-$n})); last[$k]=$n
    (( d > 8 )) && echo "$k $(tr ' ' _ < $t/comm) ticks=$d $(awk '{sub(/.*\) /,""); print $1}' $t/stat) sc=$(cut -d' ' -f1 $t/syscall 2>/dev/null) $(cat $t/wchan 2>/dev/null)" >> $out.txt
  done
  if [[ -z $did40 ]] && (( e >= 40 )); then did40=1
    x=$(grep -l ExternalClient /proc/$pid/task/*/comm 2>/dev/null | head -1 | cut -d/ -f5)
    [[ -n $x ]] && { echo "-- stack of ExternalClient $x at select" >> $out.txt; timeout 60 python3 $here/livestack.py $pid $x select >> $out.txt 2>&1; }
  fi
  if [[ -z $did95 ]] && (( e >= 95 )); then did95=1; echo "-- core at +${e}s" >> $out.txt; timeout 120 gcore -o $out.core $pid > /dev/null 2>&1; fi
  sleep 5
done
echo "done $(date +%T)" >> $out.txt
