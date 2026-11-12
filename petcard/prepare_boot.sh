#!/bin/sh
# Bind PetCard power-on/off artwork over the stock MMI paths before zte_mifi starts.
BASE=/mnt/userdata/petcard
SOURCE=$BASE/mmi
TARGET=/etc_ro/mmi
LOG=$BASE/boot-assets.log
WEB=/tmp/petcard-www

prepare_web_runtime() {
    [ -f "$BASE/www/index.html" ] || return 1
    [ -d "$BASE/www/assets" ] || return 1
    for emotion in happy hungry sleepy grumpy sleep; do
        for frame in 1 2 3 4; do
            [ -s "$BASE/www/assets/${emotion}_${frame}.rgba" ] || return 1
            [ -s "$BASE/www/assets/${emotion}_${frame}.png" ] || return 1
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

echo "prepare boot artwork $(date)" >> "$LOG"
MOUNTED=
for name in adol.png background.png background-poweroff.png first_background.png second_background.png \
    image_poweron_0.png image_poweron_1.png image_poweron_2.png image_poweron_3.png \
    image_poweron_4.png image_poweron_5.png image_poweron_6.png image_poweron_7.png \
    image_poweron_8.png image_poweron_9.png image_poweron_10.png \
    image_poweroff_0.png image_poweroff_1.png image_poweroff_2.png image_poweroff_3.png \
    image_poweroff_4.png image_poweroff_5.png image_poweroff_6.png image_poweroff_7.png \
    image_poweroff_8.png image_poweroff_9.png; do
    src=$SOURCE/$name
    dst=$TARGET/$name
    if [ ! -s "$src" ] || [ ! -e "$dst" ]; then
        echo "missing artwork path: $src -> $dst" >> "$LOG"
        exit 1
    fi
done

for name in adol.png background.png background-poweroff.png first_background.png second_background.png \
    image_poweron_0.png image_poweron_1.png image_poweron_2.png image_poweron_3.png \
    image_poweron_4.png image_poweron_5.png image_poweron_6.png image_poweron_7.png \
    image_poweron_8.png image_poweron_9.png image_poweron_10.png \
    image_poweroff_0.png image_poweroff_1.png image_poweroff_2.png image_poweroff_3.png \
    image_poweroff_4.png image_poweroff_5.png image_poweroff_6.png image_poweroff_7.png \
    image_poweroff_8.png image_poweroff_9.png; do
    if awk -v target="$TARGET/$name" '$2 == target { found = 1 } END { exit !found }' /proc/mounts; then
        continue
    fi
    if ! mount -o bind "$SOURCE/$name" "$TARGET/$name"; then
        echo "bind failed: $name" >> "$LOG"
        for mounted in $MOUNTED; do
            umount "$TARGET/$mounted" 2>/dev/null
        done
        exit 1
    fi
    MOUNTED="$name $MOUNTED"
done

# Complete web asset checks before starting zte_mifi so the display takeover
# does not wait on userdata reads after the OEM MMI begins drawing.
if prepare_web_runtime; then
    echo "web assets ready before MMI $(date)" >> "$LOG"
else
    echo "web assets deferred to autostart $(date)" >> "$LOG"
fi

echo "PetCard power artwork mounted" >> "$LOG"
exit 0
