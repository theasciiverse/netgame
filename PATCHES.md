# Changes from upstream

This is the Asciiverse packaging of [Netgame](https://github.com/zed0/Netgame)
by Ben Falconer, the net (netwalk) tile-rotation puzzle in one C++ file
that draws with ANSI escape codes and reads keys through termios (2010,
one include fix by RuthRainbow in 2013, MIT licence added 2020).
Upstream is the `upstream` remote; the `asciiverse` branch starts from
its last commit, `fc6d7f4` (2020-10-02). Upstream has no tags or version,
so the version here is 2020.10.02.

## Licence

MIT, "Copyright (c) 2020 Ben Falconer" (LICENSE, added by the author in
2020 to cover the whole program).

## Code

- **Sizes optional and checked.** Without sizes the board is 11 by 11 (a
  single size makes a square one); upstream only printed its usage.
  Sizes are kept between 2 and 99: a 1 by 1 board never finished
  generating and 0 crashed.
- **No undefined behaviour in cursor moves.** `currentX =
  (--currentX%sizeX + sizeX)%sizeX` and the like modified a variable
  twice without a sequence point; they use `wrapX()`/`wrapY()` now.
- **Centred, two columns per tile, redrawn on resize.** The board was
  drawn at the top left with one column per tile, so pieces looked tall
  and thin and horizontal links did not meet. Each tile is now two
  columns, the second continuing a right arm; the board is centred with
  its count and a line of keys below; a resize re-centres it and a
  terminal too small for the board says so. It runs on the alternate
  screen with the cursor hidden, keys are read with `read()` (a resize
  does not end the wait) and the arrows work in both cursor key modes.
- **Ctrl+C, SIGTERM and SIGHUP quit like q**, with the terminal put back
  (they left it without echo and in non-canonical mode).
- **n starts a new puzzle** of the same size.
- **Windows**: keys from `_getch()`, drawing through the console's
  virtual terminal mode with UTF-8 output, Ctrl+C and closing through
  `SetConsoleCtrlHandler`, resizes noticed while polling for keys.

## Packaging (new files)

- `asciiverse/build.sh`, `asciiverse/package.sh`: one compiler call, one
  archive per target with the binary, README, LICENSE and this file.
- `.github/workflows/release.yml`: static Linux in Alpine (x86_64,
  aarch64), macOS (x86_64, arm64) and Windows x86_64 (MinGW), plus
  SHA256SUMS, on our tags (`*-av.*`).
- `.gitignore`: the build folders.
- `asciiverse/netgame.cast`: the catalog preview, 100x30, 30 s, recorded
  by `asciiverse/record-preview.sh` with hub's `castrec.py` and a fixed
  key script (tiles turned, one locked, a new puzzle with n, then q).

## Tested

Linux: native build at 100x30 and resized; q, Ctrl+C, SIGTERM and SIGHUP
exit with status 0, the alternate screen left and the cursor shown; n
makes a new puzzle. ASan and UBSan: soaks of random keys and resizes
(90 s on random sizes with and without wrapping, then 45 s each on
13x40 and 2x2 wrapping) were clean; a 99x99 wrapping board under ASan
was still generating when its 45 s soak ended, so that size is
unchecked under ASan. Windows: cross-built with MinGW-w64 and run under
Wine: keys, rotation, n and q work (three redraws seen, exit status 0),
but Wine's console prints the escape codes literally, so the drawing on
a real Windows console is unchecked. macOS: not built here.
