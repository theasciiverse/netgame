#!/bin/sh
# Records the catalog preview at 100x30 with hub/tools/castrec.py and a fixed
# key script: three rows of tiles turned on the default 11x11 board, a tile
# locked, n for a new puzzle, a column turned, q. About 30 s.
#
#   asciiverse/record-preview.sh out/netgame asciiverse/netgame.cast
set -eu
binary=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
out=$(pwd)/$2
castrec=$(cd "$(dirname "$0")/../../../hub/tools" && pwd)/castrec.py
home=$(mktemp -d)
trap 'rm -rf "$home"' EXIT
mkdir "$home/bin" && cp "$binary" "$home/bin/netgame"
script="w2"
for row in 1 2 3; do
  for i in 1 2 3 4 5 6 7 8 9; do script="$script;kspace;w0.25;kright;w0.25"; done
  script="$script;kdown;w0.3"
done
script="$script;kspace;w0.3;kspace;w0.3;kl;w0.4;kleft;w0.3;k,;w0.3;kl;w0.6;w1.5;kn;w2.5"
for i in 1 2 3 4 5 6 7 8; do script="$script;kspace;w0.3;kdown;w0.3"; done
script="$script;w1.5;kq;w0.5"
HOME="$home" PATH="$home/bin:$PATH" python3 "$castrec" "$out" "$home" "$script" -- netgame
