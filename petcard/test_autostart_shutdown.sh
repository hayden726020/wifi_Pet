#!/bin/sh
# Host-only manager fixture. No device binaries, devices, or real poweroff.
set -eu
ROOT=$(mktemp -d /tmp/petcard-manager-test.XXXXXX)
MANAGER=
cleanup() {
    if [ -n "$MANAGER" ]; then kill -TERM -- "-$MANAGER" 2>/dev/null || :; fi
    rm -rf "$ROOT"
}
trap cleanup EXIT HUP INT TERM
mkdir -p "$ROOT/base/bin" "$ROOT/tmp/petcard-www" "$ROOT/proc/123"
printf 'zte_mifi\n' > "$ROOT/proc/123/comm"
: > "$ROOT/tmp/petcard-www/.runtime-ready"
: > "$ROOT/tmp/petcard-shutdown"
cat > "$ROOT/base/bin/ui_hold" <<'EOF'
#!/bin/sh
echo HOLDING
while :; do sleep 1; done
EOF
cat > "$ROOT/base/bin/petcard" <<'EOF'
#!/bin/sh
echo launch >> "$FIXTURE/launches"
# Simulate an accepted shutdown followed by init terminating the UI process.
: > "$FIXTURE/tmp/petcard-shutdown"
exit 0
EOF
chmod +x "$ROOT/base/bin/ui_hold" "$ROOT/base/bin/petcard"
sed -e "s|/tmp/|$ROOT/tmp/|g" \
    -e "s|/mnt/userdata/petcard|$ROOT/base|g" \
    -e "s|/proc/|$ROOT/proc/|g" \
    "$(dirname "$0")/autostart.sh" > "$ROOT/manager.sh"
FIXTURE=$ROOT setsid sh "$ROOT/manager.sh" > "$ROOT/output" 2>&1 &
MANAGER=$!
sleep 1
test ! -e "$ROOT/launches"
test ! -e "$ROOT/tmp/ui_hold.log"
echo 'PASS existing shutdown guard suppresses UI and holder startup'
rm "$ROOT/tmp/petcard-shutdown"
for i in 1 2 3 4 5 6 7 8 9 10; do
    [ -f "$ROOT/launches" ] && break
    sleep 0.2
done
test "$(wc -l < "$ROOT/launches")" -eq 1
sleep 3
test "$(wc -l < "$ROOT/launches")" -eq 1
test -e "$ROOT/tmp/petcard-shutdown"
kill -0 "$MANAGER"
echo 'PASS terminated shutdown child is not respawned and guard is retained'
