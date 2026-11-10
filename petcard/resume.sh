#!/bin/sh
# Emergency recovery: stop PetCore and the UI hold, restore the stock framebuffer.
# Do not touch hostapd/udhcpd; the local AP must stay up.
killall petcard 2>/dev/null
killall ui_hold 2>/dev/null
sleep 1
killall -9 petcard 2>/dev/null
killall -9 ui_hold 2>/dev/null
PID=$(ps | grep zte_mifi | grep -v grep | awk '{print $1; exit}')
if [ -n "$PID" ]; then
    kill -CONT "$PID" 2>/dev/null
fi
F=/sys/kernel/debug/lcd_zx29/lcd_lead
if [ -e "$F" ]; then
    echo sleepout > "$F" 2>/dev/null
    echo blgon > "$F" 2>/dev/null
fi
SIZE=$(wc -c < /tmp/petcard.fb0 2>/dev/null)
if [ "$SIZE" = "32768" ]; then
    /tmp/petcard --restore /tmp/petcard.fb0 >/dev/null 2>&1
fi
rmdir /tmp/petcard.lock 2>/dev/null
exit 0
