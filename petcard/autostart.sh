#!/bin/sh
# Persistent init-launched manager. It never freezes zte_mifi as a whole.
BASE=/mnt/userdata/petcard
WEB=/tmp/petcard-www
LOG=$BASE/autostart.log
HOLDER_BIN=$BASE/bin/ui_hold
PETCARD_BIN=$BASE/bin/petcard
SHUTDOWN=/tmp/petcard-shutdown
EARLY_ATTEMPTS=0
trap '' HUP

BOOT_ID=$(cat /proc/sys/kernel/random/boot_id 2>/dev/null) || BOOT_ID=unknown
UPTIME=$(cat /proc/uptime 2>/dev/null) || UPTIME=unknown
echo "PetCard manager start boot_id=$BOOT_ID uptime=$UPTIME $(date)" >> "$LOG"

prepare_web_runtime() {
    [ -f "$BASE/www/index.html" ] || return 1
    [ -d "$BASE/www/assets" ] || return 1
    for emotion in happy hungry sleepy grumpy sleep; do
        for frame in 1 2 3 4; do
            rgba="$BASE/www/assets/${emotion}_${frame}.rgba"
            png="$BASE/www/assets/${emotion}_${frame}.png"
            [ -s "$rgba" ] || return 1
            [ -s "$png" ] || return 1
        done
    done
    wc -c "$BASE/www/assets/"*.rgba | awk '$2 ~ /\.rgba$/ && $1 != 16384 { bad = 1 } $2 ~ /\.rgba$/ { count++ } END { exit bad || count != 20 }' || return 1
    mkdir -p "$WEB" || return 1
    if [ ! -f "$WEB/index.html" ]; then
        ln -s "$BASE/www/index.html" "$WEB/index.html" || return 1
    fi
    if [ ! -d "$WEB/assets" ]; then
        ln -s "$BASE/www/assets" "$WEB/assets" || return 1
    fi
    : > "$WEB/.runtime-ready" || return 1
}

while :; do
    # Deliberately retained for the rest of this boot once poweroff succeeds.
    # A failed request removes it; /tmp clears it on a real reboot. Do not
    # infer failure from a dead PetCard PID: init may already be stopping it.
    if [ -e "$SHUTDOWN" ]; then
        sleep 0.2
        continue
    fi
    if [ ! -x "$HOLDER_BIN" ] || [ ! -x "$PETCARD_BIN" ]; then
        echo "PetCard binaries missing $(date)" >> "$LOG"
        sleep 1
        continue
    fi

    # Paint PetCard as soon as the kernel devices and prechecked web assets
    # exist. The manager itself is launched before zte_mifi; UI holding still
    # waits until zte_mifi and its MMI threads appear.
    if { [ -z "$CHILD" ] || ! kill -0 "$CHILD" 2>/dev/null; } && [ "$EARLY_ATTEMPTS" -lt 3 ]; then
        if [ -c /dev/fb0 ] && [ -c /dev/event0 ] && [ -f "$WEB/.runtime-ready" ] && [ ! -e "$SHUTDOWN" ]; then
            rm -f /tmp/petcard.log /tmp/petcard-early-ready
            "$PETCARD_BIN" --debug > /tmp/petcard.log 2>&1 &
            CHILD=$!
            case "$EARLY_ATTEMPTS" in
                0) EARLY_ATTEMPTS=1 ;;
                1) EARLY_ATTEMPTS=2 ;;
                *) EARLY_ATTEMPTS=3 ;;
            esac
            echo "PetCard early launch pid=$CHILD $(date)" >> "$LOG"
            for i in 1 2 3 4 5 6 7 8 9 10; do
                if [ -f /tmp/petcard-early-ready ]; then
                    echo "PetCard first frame ready pid=$CHILD $(date)" >> "$LOG"
                    break
                fi
                kill -0 "$CHILD" 2>/dev/null || break
                sleep 0.1
            done
        fi
    fi

    PID=
    for proc in /proc/[0-9]*; do
        [ -r "$proc/comm" ] || continue
        IFS= read -r name < "$proc/comm"
        if [ "$name" = "zte_mifi" ]; then
            PID=${proc##*/}
            break
        fi
    done
    if [ -z "$PID" ]; then
        sleep 0.2
        continue
    fi

    [ -e "$SHUTDOWN" ] && continue
    if [ -z "$HOLDER" ] || ! kill -0 "$HOLDER" 2>/dev/null; then
        rm -f /tmp/ui_hold.log
        "$HOLDER_BIN" "$PID" > /tmp/ui_hold.log 2>&1 &
        HOLDER=$!
        READY=0
        for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
            if grep -q HOLDING /tmp/ui_hold.log 2>/dev/null; then
                READY=1
                echo "UI held pid=$PID $(date)" >> "$LOG"
                break
            fi
            kill -0 "$HOLDER" 2>/dev/null || break
            sleep 0.1
        done
        if [ "$READY" != 1 ] && grep -q HOLDING /tmp/ui_hold.log 2>/dev/null; then
            READY=1
            echo "UI held pid=$PID $(date)" >> "$LOG"
        fi
        if [ "$READY" != 1 ]; then
            kill "$HOLDER" 2>/dev/null
            wait "$HOLDER" 2>/dev/null
            HOLDER=
            echo "UI hold retry $(date)" >> "$LOG"
            sleep 0.2
            continue
        fi
    fi

    if [ ! -f "$WEB/.runtime-ready" ]; then
        if ! prepare_web_runtime; then
            echo "web asset links failed $(date)" >> "$LOG"
            sleep 2
            continue
        fi
        echo "web assets linked $(date)" >> "$LOG"
    fi

    if [ -z "$CHILD" ] || ! kill -0 "$CHILD" 2>/dev/null; then
        [ -e "$SHUTDOWN" ] && continue
        "$PETCARD_BIN" --debug > /tmp/petcard.log 2>&1 &
        CHILD=$!
        echo "PetCard launched pid=$CHILD $(date)" >> "$LOG"
    fi

    sleep 2
done
