#!/usr/bin/env bash
# Start the Riot Client under the next free run number, so the label never has to be picked by hand.
T=$(dirname "$(readlink -f "$0")"); B=$HOME/.local/share/darling/macos-apps/league/baseline
n=$(ls "$B" | sed -n 's/^riot-client-riot-client-\([0-9]*\)$/\1/p' | sort -n | tail -1)
echo "run riot-client-$((n + 1))"
RC_GUEST_ENV="${RC_GUEST_ENV:-DARLING_TOLERATE_UNRECOGNIZED_SELECTORS=1}" exec bash "$T/rc-run.sh" "riot-client-$((n + 1))"
