#!/usr/bin/env bash
# Runs each test program headlessly in SkoolKit's simulator, reads the text on
# its final screen and compares it with tests/expected/<name>.txt.
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
           StackOverflow)
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
    "$PYTHON" tests/tap2text.py --png "$screen" "${input[@]}" "$BUILD/$name.tap" >"$actual"
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
