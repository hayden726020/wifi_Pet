#!/bin/sh
# Emergency recovery: stop any live game, wake/restore the panel, resume the stock UI.
killall snake 2>/dev/null
sleep 1
F=/sys/kernel/debug/lcd_zx29/lcd_lead
if [ -e "$F" ]; then
    echo sleepout > "$F" 2>/dev/null
    echo blgon > "$F" 2>/dev/null
fi
PID=$(ps | grep zte_mifi | grep -v grep | awk '{print $1; exit}')
SIZE=$(wc -c < /tmp/fb0.orig 2>/dev/null)
if [ "$SIZE" = "32768" ]; then
    /tmp/snake --restore /tmp/fb0.orig >/dev/null 2>&1
fi
if [ -n "$PID" ]; then
    kill -CONT "$PID" 2>/dev/null
fi
rmdir /tmp/snake.lock 2>/dev/null
exit 0
