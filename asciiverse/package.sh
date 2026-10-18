#!/bin/sh
# Packs the build into dist/netgame-<target>.tar.gz (.zip for Windows) for
# av: the binary at the top, with README, LICENSE and PATCHES.md.
#
#   asciiverse/package.sh x86_64-unknown-linux-musl
set -eu
cd "$(dirname "$0")/.."
target="$1"
case "$target" in *windows*) exe=.exe ;; *) exe= ;; esac
stage=stage/netgame
rm -rf "$stage" && mkdir -p "$stage" dist
cp "out/netgame$exe" README LICENSE PATCHES.md "$stage/"
if [ -n "$exe" ]; then
  (cd "$stage" && zip -qr "../../dist/netgame-$target.zip" .)
else
  tar -czf "dist/netgame-$target.tar.gz" -C "$stage" .
fi
ls dist
