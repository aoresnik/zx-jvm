#!/usr/bin/env bash
# Runs each test program headlessly in SkoolKit's simulator, reads the text on
# its final screen and compares it with tests/expected/<name>.txt.
#
# A program that uses java.awt also has what it drew compared with what Java2D
# draws for it on the desktop (see compare_graphics.py). The numbers of pixels
# that differ are compared with tests/expected/<name>.pixels, so that any
# change shows.
#
# usage: tests/run.sh [--update] [TestName...]
#
# A test that reads input types the lines of tests/<name>.in (see tap2text.py).
#
# --update rewrites the expected text: check the diff before committing it.
# Needs the Python environment that `make venv` creates; set PYTHON to use
# another interpreter with SkoolKit installed.
set -u

cd "$(dirname "$0")/.." || exit 1

PYTHON=${PYTHON:-.venv/bin/python}
BUILD=build/tests

update=0
if [ "${1:-}" = "--update" ]; then
    update=1
    shift
fi
if [ $# -gt 0 ]; then
    names=("$@")
else
    names=(Fibonacci HelloWorld Adder Loan Edge Arithmetic Doubles PrintDoubles Scroll Strings LongString
           References Methods Input InputInts BadOpcode BadMethod BadInput BadDouble DivideByZero NoMain
           StackOverflow Shapes GraphicsLines GraphicsOvals GraphicsText)
fi

[ -x "$PYTHON" ] || { echo "cannot run $PYTHON (run make venv, or set PYTHON)" >&2; exit 1; }
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
    input=()
    [ -f "tests/$name.in" ] && input=(--input "tests/$name.in")
    graphics=()
    grep -q java.awt "$dir/$name.java" && graphics=(--screen "$BUILD/$name.bin")
    rm -f "$BUILD/$name.bin"
    "$PYTHON" tests/tap2text.py --png "$screen" "${input[@]}" "${graphics[@]}" "$BUILD/$name.tap" >"$actual"
    status=$?
    # Every test should finish, so running out of time is a failure.
    if [ "$status" -eq 3 ]; then
        echo "FAIL $name: did not finish (screen: $screen)"
        failed=1
        continue
    elif [ "$status" -ne 0 ]; then
        echo "FAIL $name: could not run the tape"
        failed=1
        continue
    fi

    pixels=()
    if [ ${#graphics[@]} -gt 0 ]; then
        pixels=("$BUILD/$name.pixels" "tests/expected/$name.pixels")
        if [ ! -f "$BUILD/$name.bin" ] || ! "$PYTHON" tests/compare_graphics.py \
                --diff "$BUILD/$name-diff.png" "$BUILD/$name.bin" "$BUILD" "$name" >"${pixels[0]}"; then
            echo "FAIL $name: no graphics to compare"
            failed=1
            continue
        fi
    fi

    if [ "$update" -eq 1 ]; then
        cp "$actual" "$expected"
        [ ${#pixels[@]} -gt 0 ] && cp "${pixels[@]}"
        echo "UPDATED $name"
    elif [ ! -f "$expected" ]; then
        echo "FAIL $name: no expected text"
        failed=1
    elif ! diff -u "$expected" "$actual"; then
        echo "FAIL $name: screen differs (screen: $screen)"
        failed=1
    elif [ ${#pixels[@]} -gt 0 ] && ! diff -u "${pixels[1]}" "${pixels[0]}"; then
        echo "FAIL $name: pixels differ from Java2D's in other numbers (diff: $BUILD/$name-diff.png)"
        failed=1
    else
        echo "ok   $name"
    fi
done
rm -f "$BUILD.log"
exit "$failed"
