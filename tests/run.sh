#!/usr/bin/env bash
# Runs each test program in Fuse and compares its final screen with the
# reference image in tests/expected.
#
# usage: tests/run.sh [--update] [TestName...]
#
# --update rewrites the reference images: check them by eye afterwards.
# Fuse windows open on the display while this runs. References depend on how
# Fuse draws its window, so regenerate them if Fuse or its display settings change.
set -u

cd "$(dirname "$0")/.." || exit 1

ZX_RUN=${ZX_RUN:-.claude/skills/zx-spectrum-run/scripts/zx-run.sh}
BUILD=build/tests
# Must cover the menu and status bars; the status bar's speed readout varies.
BARS=0x30

update=0
if [ "${1:-}" = "--update" ]; then
    update=1
    shift
fi
if [ $# -gt 0 ]; then
    names=("$@")
else
    names=(Fibonacci Edge Scroll BadOpcode BadMethod NoMain)
fi

[ -x "$ZX_RUN" ] || { echo "cannot run $ZX_RUN (set ZX_RUN)" >&2; exit 1; }
mkdir -p "$BUILD"

failed=0
for name in "${names[@]}"; do
    dir=tests
    [ "$name" = Fibonacci ] && dir=examples
    if ! make --no-print-directory CLASS="$name" JAVA_DIR="$dir" BUILD="$BUILD" >"$BUILD.log" 2>&1; then
        cat "$BUILD.log"
        echo "FAIL $name: build failed"
        failed=1
        continue
    fi

    raw=$BUILD/$name.raw.png
    actual=$BUILD/$name.png
    expected=tests/expected/$name.png
    # Every test should finish, so a screen that never settles is a failure.
    if ! "$ZX_RUN" --out "$raw" "$BUILD/$name.tap" >/dev/null; then
        echo "FAIL $name: did not settle (screen: $raw)"
        failed=1
        continue
    fi
    convert "$raw" -strip -gravity North -chop "$BARS" -gravity South -chop "$BARS" "$actual"

    if [ "$update" -eq 1 ]; then
        cp "$actual" "$expected"
        echo "UPDATED $name"
    elif [ ! -f "$expected" ]; then
        echo "FAIL $name: no reference image"
        failed=1
    elif compare -metric AE "$expected" "$actual" null: 2>/dev/null; then
        echo "ok   $name"
    else
        echo "FAIL $name: screen differs (see $actual)"
        failed=1
    fi
done
rm -f "$BUILD.log"
exit "$failed"
