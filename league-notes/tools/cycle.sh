#!/usr/bin/env bash
# Install the committed build and run the Riot Client once. Refuses a dirty tree or an unfinished build.
# Usage: cycle.sh <run label> <seconds> <framework path under System/Library/Frameworks> <symbol that must be in it>
set -euo pipefail
repo=/home/zpt/projects/darling; here=$(dirname "$(readlink -f "$0")")
[[ -z $(git -C $repo status --porcelain --untracked-files=no) ]] || { echo "tree has uncommitted changes" >&2; exit 2; }
cd $repo/build
[[ $(ninja -n | tail -1) == "ninja: no work to do." ]] || { echo "build not finished" >&2; exit 2; }
export DARLING_INSTALL_PREFIX=$PWD/destdir/usr/local DPREFIX=$HOME/.darling-league-run2
src/startup/darling shutdown > /dev/null
DESTDIR=$PWD/destdir ninja install > /dev/null 2>&1
llvm-nm "destdir/usr/local/libexec/darling/System/Library/Frameworks/$3" | grep -c -- "$4" > /dev/null || { echo "installed $3 lacks $4" >&2; exit 2; }
bash "$here/rc-shot.sh" "$1" "$2" 2>&1 | tail -14
