#!/usr/bin/env bash
# Runs a ZX Spectrum program in Fuse and saves a screenshot of the emulator.
#
# Exit status: 0 screen settled, 3 screen still changing at the timeout (a
# screenshot is saved anyway), 1 emulator failure, 2 bad usage.
set -u

usage() {
    cat <<'EOF'
usage: zx-run.sh [options] program [-- extra fuse args]

  --out FILE       screenshot to write (default: zx-screen.png)
  --machine TYPE   fuse machine: 48, 128, plus2, plus3, ... (default: 48)
  --min-wait SECS  always run at least this long (default: 8)
  --settle SECS    screen must be unchanged this long to count as done (default: 5)
  --timeout SECS   stop waiting after this long, per stage (default: 60)
  --keys "K1 K2"   after the screen settles, press these xdotool keys in order
                   and wait again; repeat the option for further stages
  --scale PERCENT  enlarge the screenshot with sharp pixels (default: 100)
EOF
    exit 2
}

out=zx-screen.png
machine=48
min_wait=8
settle=5
timeout=60
scale=100
stages=()

while [ $# -gt 0 ]; do
    case "$1" in
        --out) out=${2:?}; shift 2 ;;
        --machine) machine=${2:?}; shift 2 ;;
        --min-wait) min_wait=${2:?}; shift 2 ;;
        --settle) settle=${2:?}; shift 2 ;;
        --timeout) timeout=${2:?}; shift 2 ;;
        --keys) stages+=("${2:?}"); shift 2 ;;
        --scale) scale=${2:?}; shift 2 ;;
        --) usage ;;
        -*) usage ;;
        *) break ;;
    esac
done
[ $# -ge 1 ] || usage
program=$1
shift
if [ $# -gt 0 ]; then
    [ "$1" = "--" ] || usage
    shift
fi

fail() {
    echo "zx-run: $1" >&2
    exit 1
}

[ -f "$program" ] || fail "no such file: $program"
[ -n "${DISPLAY:-}" ] || fail "DISPLAY is not set; Fuse needs an X display"
for tool in fuse xdotool import convert md5sum; do
    command -v "$tool" >/dev/null || fail "missing tool: $tool"
done

ext=${program##*.}
case "${ext,,}" in
    tap|tzx|pzx|csw|wav|spc|sta|ltp) source_args=(--auto-load --tape "$program") ;;
    sna|z80|szx|snp|sp|zxs|plusd|mdr) source_args=(--snapshot "$program") ;;
    *) source_args=(--auto-load "$program") ;;
esac

log=$(mktemp)
fuse --machine "$machine" --no-sound "${source_args[@]}" "$@" >"$log" 2>&1 &
pid=$!

cleanup() {
    kill -KILL "$pid" 2>/dev/null   # fuse does not exit on SIGTERM
    wait "$pid" 2>/dev/null
    rm -f "$log"
}
trap cleanup EXIT

die_if_gone() {
    kill -0 "$pid" 2>/dev/null && return
    cat "$log" >&2
    fail "fuse exited early"
}

win=""
for _ in $(seq 1 40); do
    die_if_gone
    win=$(xdotool search --onlyvisible --pid "$pid" 2>/dev/null | tail -1)
    [ -n "$win" ] && break
    sleep 0.25
done
[ -n "$win" ] || fail "no Fuse window appeared"

# import reads pixels from the screen, so another window on top would be captured.
xdotool windowactivate --sync "$win" 2>/dev/null

# The menu and status bars are left out: the speed readout changes constantly.
frame_hash() {
    import -silent -window "$win" miff:- 2>/dev/null \
        | convert miff:- -gravity North -chop 0x30 -gravity South -chop 0x30 rgb:- \
        | md5sum
}

# Returns 0 once the screen stops changing, 1 if it is still changing at the timeout.
wait_settled() {
    local minimum=$1 start=$SECONDS changed=$SECONDS last="" hash
    while :; do
        die_if_gone
        hash=$(frame_hash)
        if [ "$hash" != "$last" ]; then
            last=$hash
            changed=$SECONDS
        fi
        if [ $((SECONDS - start)) -ge "$minimum" ] && [ $((SECONDS - changed)) -ge "$settle" ]; then
            return 0
        fi
        [ $((SECONDS - start)) -lt "$timeout" ] || return 1
        sleep 0.5
    done
}

status=0
wait_settled "$min_wait" || status=3

for stage in ${stages[@]+"${stages[@]}"}; do
    xdotool windowactivate --sync "$win" 2>/dev/null
    for key in $stage; do
        xdotool key --delay 80 "$key"
        sleep 0.2
    done
    wait_settled 0 || status=3
done

xdotool windowactivate --sync "$win" 2>/dev/null
import -silent -window "$win" "$out" || fail "screenshot failed"
if [ "$scale" != 100 ]; then
    convert "$out" -filter point -resize "$scale%" "$out" || fail "scaling failed"
fi

if [ "$status" -eq 0 ]; then
    echo "settled: $out"
else
    echo "timeout (screen still changing): $out"
fi
[ -s "$log" ] && cat "$log"
exit "$status"
