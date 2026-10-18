#!/bin/sh
# Builds netgame into out/: one C++ file, no libraries (it draws with ANSI
# escape sequences itself), so one compiler call serves every target:
#
#   asciiverse/build.sh                                        # Linux
#   LDFLAGS="-static -s" asciiverse/build.sh                   # Alpine, static
#   CXXFLAGS="-O2 -arch arm64" LDFLAGS="-arch arm64" asciiverse/build.sh  # macOS
#   CXX=x86_64-w64-mingw32-g++ LDFLAGS="-static -s" EXE=.exe asciiverse/build.sh  # Windows
set -eu
cd "$(dirname "$0")/.."
CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--O2}"
LDFLAGS="${LDFLAGS:-}"
EXE="${EXE:-}"
mkdir -p out
rm -f "out/netgame$EXE"
# shellcheck disable=SC2086
nice -n 10 $CXX -std=c++11 -Wall $CXXFLAGS main.cpp $LDFLAGS -o "out/netgame$EXE"
echo "built netgame$EXE"
