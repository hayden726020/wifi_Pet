#!/bin/sh
# Run PetCore while holding only the stock-UI threads (not the watchdog/AP).
# The stock AP (hostapd/udhcpd) and zte_watchdog keep running.
SNAP=/tmp/petcard.fb0
LOCK=/tmp/petcard.lock
BIN=/tmp/petcard
HOLD=/tmp/ui_hold
LOG=/tmp/petcard.log
HLOG=/tmp/ui_hold.log

if ! mkdir "$LOCK" 2>/dev/null; then
    echo "petcard already running (remove $LOCK if stale)" >&2
    exit 1
fi

PID=$(ps | grep zte_mifi | grep -v grep | awk '{print $1; exit}')

# Snapshot the stock UI before taking its display/key threads away.
cat /dev/fb0 > "$SNAP" 2>/dev/null

CHILD=
HOLDER=
DONE=0
cleanup() {
    [ "$DONE" = 1 ] && return
    DONE=1
    trap '' HUP INT TERM
    if [ -n "$CHILD" ] && kill -0 "$CHILD" 2>/dev/null; then
        kill "$CHILD" 2>/dev/null
        sleep 1
        kill -9 "$CHILD" 2>/dev/null
        wait "$CHILD" 2>/dev/null
    fi
    if [ -n "$HOLDER" ] && kill -0 "$HOLDER" 2>/dev/null; then
        kill "$HOLDER" 2>/dev/null
        sleep 1
        kill -9 "$HOLDER" 2>/dev/null
        wait "$HOLDER" 2>/dev/null
    fi
    if [ -n "$PID" ]; then
        kill -CONT "$PID" 2>/dev/null
    fi
    SIZE=$(wc -c < "$SNAP" 2>/dev/null)
    if [ "$SIZE" = "32768" ]; then
        "$BIN" --restore "$SNAP" >/dev/null 2>&1
    fi
    rmdir "$LOCK" 2>/dev/null
}
trap cleanup EXIT
trap 'exit 143' HUP INT TERM

if [ -n "$PID" ] && [ -x "$HOLD" ]; then
    rm -f "$HLOG"
    "$HOLD" "$PID" > "$HLOG" 2>&1 &
    HOLDER=$!
    OK=0
    for i in 1 2 3 4 5; do
        sleep 1
        if grep -q HOLDING "$HLOG" 2>/dev/null; then
            OK=1
            break
        fi
        kill -0 "$HOLDER" 2>/dev/null || break
    done
    if [ "$OK" != "1" ] || ! kill -0 "$HOLDER" 2>/dev/null; then
        echo "ui_hold failed to attach UI threads; aborting" >&2
        exit 1
    fi
elif [ ! -x "$HOLD" ]; then
    echo "ui_hold missing; refusing to freeze watchdog" >&2
    exit 1
fi

"$BIN" "$@" > "$LOG" 2>&1 &
CHILD=$!

while kill -0 "$CHILD" 2>/dev/null; do
    if [ -n "$HOLDER" ] && ! kill -0 "$HOLDER" 2>/dev/null; then
        echo "ui_hold died; stopping petcard" >&2
        kill "$CHILD" 2>/dev/null
        break
    fi
    sleep 2
done
wait "$CHILD" 2>/dev/null
RC=$?
cleanup
trap - EXIT
exit $RC
