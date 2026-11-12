#!/usr/bin/env bash
# Build a small Mac test program against the locally built Darling. Usage: build.sh <name> [-framework X ...]
# Run it with: darling shell /Volumes/SystemRoot/<absolute host path to the output>
set -euo pipefail
R=/home/zpt/projects/darling; D=$R/build/destdir/usr/local/libexec/darling
here=$(dirname "$(readlink -f "$0")"); name=$1; shift
clang -target x86_64-apple-darwin20 -mmacosx-version-min=11.0 \
  -isysroot $R/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk \
  -fuse-ld=$R/build/src/external/cctools-port/cctools/ld64/src/x86_64-apple-darwin20-ld \
  -Wl,-syslibroot,$D -Wl,-F,$D/System/Library/Frameworks -L$D/usr/lib "$@" -o "$here/$name" "$here/$name.c"
