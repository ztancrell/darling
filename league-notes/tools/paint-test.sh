#!/usr/bin/env bash
# Does a window repaint? Runs stock Electron with paintapp (moving box, frame counter, text field) and reports:
# the page's own frame rate, distinct pictures seen on screen, frames sent per surface, frames the browser
# took, and whether typed text shows. Usage: paint-test.sh <label> [seconds=40]   PAINT_HW=1: GPU path.
set -uo pipefail
label=$1; secs=${2:-40}; T=$(dirname "$(readlink -f "$0")"); B=$HOME/.local/share/darling/macos-apps/league/baseline
d=$B/electron-$label; S=$d-shots; mkdir -p "$S"
(ET_APP=paintapp ET_GUEST_ENV="DARLING_IOSURFACE_TRACE=1 ${PAINT_HW:+PAINT_HW=1}" bash "$T/electron-test.sh" "$label" "$secs" > "$S/et.out" 2>&1 &)
W=; for i in $(seq 40); do W=$(xdotool search --onlyvisible --name 'painttest' 2>/dev/null | head -1); [[ -n $W ]] && break; sleep 1; done
[[ -n $W ]] || { echo "no window"; exit 1; }
sleep 8
for i in $(seq -w 1 16); do import -window "$W" "$S/f$i.png" 2>/dev/null; done
echo "distinct pictures in 16 grabs: $(md5sum "$S"/f*.png | awk '{print $1}' | sort -u | wc -l)"
xdotool mousemove --window "$W" 200 280 click 1 2>/dev/null; sleep 0.5
xdotool type --window "$W" --delay 150 'hello' 2>/dev/null; sleep 2
import -window "$W" "$S/typed.png" 2>/dev/null
for i in $(seq $((secs + 30))); do grep -q 'cores:' "$S/et.out" 2>/dev/null && break; sleep 1; done
tail -1 "$S/et.out" | cut -c1-60
echo "page fps (per second): $(grep -a -o 'page: fps [0-9.]*' "$d/app.out" | awk '{print $3}' | tr '\n' ' ' | cut -c1-200)"
echo "frames sent per surface: $(grep -a -o 'for id 0x[0-9a-f]*' "$d/app.err" | sort | uniq -c | awk '{printf "%s:%s ", $4, $1}')"
echo "frames the browser took (lookups): $(grep -a -c 'lookup from port' "$d/app.err")"
echo "typed text reached the page: $(grep -a -o 'page: input 5 hello' "$d/app.out" | head -1)"
echo "clicks the page saw: $(grep -a -c "page: mousedown" "$d/app.out"); mouse-move reports: $(grep -a -c "page: mousemove" "$d/app.out"); mouse exceptions: $(grep -a -c "No event subtype" "$d/app.err")"
echo "shots: $S"
