#!/bin/sh
# End-to-end test: fake device on loopback + hidden scripted UI.
#
#   tests/e2e.sh 7105|7106 [build-dir] [out-dir]
#
# No broadcast is sent; discovery is unicast to the fake device.
set -eu

MODEL=${1:-7105}
BUILD=${2:-build}
OUT=${3:-$(mktemp -d)}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)

case "$MODEL" in
    7105) CODE=0x21 ;;
    7106) CODE=0x22 ;;
    *) echo "model must be 7105 or 7106" >&2; exit 2 ;;
esac

mkdir -p "$OUT"
rm -f "$OUT/rec.jsonl"
sed "s|@OUT@|$OUT|g" "$HERE/e2e_$MODEL.script" > "$OUT/script.txt"

python3 "$ROOT/tools/fakedev.py" --ip 127.0.0.2 --his-ip 127.0.0.3 \
    --model "$CODE" --record "$OUT/rec.jsonl" > "$OUT/fakedev.log" 2>&1 &
FAKE=$!
trap 'kill $FAKE 2>/dev/null || true' EXIT INT TERM
sleep 0.5

timeout 120 "$ROOT/$BUILD/switcher" --hidden --no-broadcast \
    --probe 127.0.0.2 --script "$OUT/script.txt" > "$OUT/app.log" 2>&1

python3 "$HERE/check_rec.py" "$OUT/rec.jsonl" "$HERE/e2e_$MODEL.expect"
echo "artifacts: $OUT"
