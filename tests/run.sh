#!/usr/bin/env bash
# Runs each test program in Fuse, reads the text on its final screen and
# compares it with tests/expected/<name>.txt.
#
# usage: tests/run.sh [--update] [TestName...]
#
# --update rewrites the expected text: check the diff before committing it.
# Fuse windows open on the display while this runs.
set -u

cd "$(dirname "$0")/.." || exit 1

ZX_RUN=${ZX_RUN:-.claude/skills/zx-spectrum-run/scripts/zx-run.sh}
# The 48K ROM, for its font
ZX_ROM=${ZX_ROM:-/usr/share/spectrum-roms/48.rom}
BUILD=build/tests

update=0
if [ "${1:-}" = "--update" ]; then
    update=1
    shift
fi
if [ $# -gt 0 ]; then
    names=("$@")
else
    names=(Fibonacci HelloWorld Edge Scroll Strings LongString References BadOpcode BadMethod NoMain)
fi

[ -x "$ZX_RUN" ] || { echo "cannot run $ZX_RUN (set ZX_RUN)" >&2; exit 1; }
[ -r "$ZX_ROM" ] || { echo "cannot read $ZX_ROM (set ZX_ROM)" >&2; exit 1; }
mkdir -p "$BUILD"

failed=0
for name in "${names[@]}"; do
    dir=tests
    [ -f "examples/$name.java" ] && dir=examples
    if ! make --no-print-directory CLASS="$name" JAVA_DIR="$dir" BUILD="$BUILD" >"$BUILD.log" 2>&1; then
        cat "$BUILD.log"
        echo "FAIL $name: build failed"
        failed=1
        continue
    fi

    screen=$BUILD/$name.png
    actual=$BUILD/$name.txt
    expected=tests/expected/$name.txt
    # Every test should finish, so a screen that never settles is a failure.
    if ! "$ZX_RUN" --out "$screen" "$BUILD/$name.tap" >/dev/null; then
        echo "FAIL $name: did not settle (screen: $screen)"
        failed=1
        continue
    fi
    if ! java tests/ScreenText.java "$screen" "$ZX_ROM" >"$actual"; then
        echo "FAIL $name: cannot read the screen (see $screen)"
        failed=1
        continue
    fi

    if [ "$update" -eq 1 ]; then
        cp "$actual" "$expected"
        echo "UPDATED $name"
    elif [ ! -f "$expected" ]; then
        echo "FAIL $name: no expected text"
        failed=1
    elif diff -u "$expected" "$actual"; then
        echo "ok   $name"
    else
        echo "FAIL $name: screen differs (screen: $screen)"
        failed=1
    fi
done
rm -f "$BUILD.log"
exit "$failed"
