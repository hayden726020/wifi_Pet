#!/bin/sh
# Pause the stock UI, snapshot its framebuffer once, run the game, restore on exit.
SNAP=/tmp/fb0.orig
LOCK=/tmp/snake.lock
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "snake already running (remove $LOCK if stale)" >&2
    exit 1
fi
PID=$(ps | grep zte_mifi | grep -v grep | awk '{print $1; exit}')
if [ -n "$PID" ]; then
    kill -STOP "$PID" 2>/dev/null
fi
SIZE=$(wc -c < "$SNAP" 2>/dev/null)
if [ "$SIZE" != "32768" ]; then
    cat /dev/fb0 > "$SNAP" 2>/dev/null
fi
CHILD=
DONE=0
resume() {
    [ "$DONE" = 1 ] && return
    DONE=1
    if [ -n "$CHILD" ]; then
        if kill -0 "$CHILD" 2>/dev/null; then
            kill "$CHILD" 2>/dev/null
            sleep 1
            kill -9 "$CHILD" 2>/dev/null
        fi
        wait "$CHILD" 2>/dev/null
    fi
    SIZE=$(wc -c < "$SNAP" 2>/dev/null)
    if [ "$SIZE" = "32768" ]; then
        /tmp/snake --restore "$SNAP" >/dev/null 2>&1
    fi
    if [ -n "$PID" ]; then
        kill -CONT "$PID" 2>/dev/null
    fi
    rmdir "$LOCK" 2>/dev/null
}
trap resume EXIT
trap 'exit 143' HUP INT TERM
/tmp/snake "$@" &
CHILD=$!
wait "$CHILD"
RC=$?
resume
trap - EXIT
exit $RC
