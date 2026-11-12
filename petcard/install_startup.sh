#!/bin/sh
# Install early artwork and display-takeover hooks in /etc/rc.
BASE=/mnt/userdata/petcard
BACKUP=$BASE/rollback
RC=/etc/rc
TMP=/tmp/petcard-rc.new
LOG=$BASE/install.log
SOURCE=$RC

remount_root() {
    mode=$1
    mount -o "remount,$mode" /dev/root / || return 1
    awk -v mode="$mode" '$1 == "/dev/root" && $2 == "/" && $3 == "jffs2" && $4 ~ ("^" mode) { found = 1 } END { exit !found }' /proc/mounts
}

restore_original_rc() {
    if ! remount_root rw; then
        echo "Could not remount JFFS2 for startup rollback" >> "$LOG"
        return 1
    fi
    cp "$BACKUP/rc" "$RC.new" && chmod 744 "$RC.new" && mv "$RC.new" "$RC"
}

if [ ! -x "$BASE/prepare_boot.sh" ] || [ ! -x "$BASE/autostart.sh" ]; then
    echo "PetCard boot scripts are missing" >&2
    exit 1
fi
if [ -s "$BACKUP/rc" ]; then
    SOURCE=$BACKUP/rc
fi
if grep -q PETCARD_BOOT_ART_HOOK "$RC" && grep -q PETCARD_BOOT_EARLY_UI_HOOK "$RC" && grep -q PETCARD_BOOT_DISPLAY_READY_WAIT "$RC"; then
    echo "PetCard boot hooks already installed"
    exit 0
fi
if [ "$(grep -Fxc 'zte_mifi $bootreason $bootmode &' "$SOURCE")" != 1 ] ||
   [ "$(grep -Fxc 'netdog_init_set.sh' "$SOURCE")" != 1 ] ||
   [ "$(grep -Fxc 'fs_check "normal"' "$SOURCE")" != 1 ]; then
    echo "Unexpected /etc/rc layout; refusing to patch" >&2
    exit 1
fi

mkdir -p "$BACKUP" || exit 1
if [ ! -s "$BACKUP/rc" ]; then cp "$RC" "$BACKUP/rc" || exit 1; fi
if [ ! -s "$BACKUP/inittab" ]; then cp /etc/inittab "$BACKUP/inittab" || exit 1; fi

awk '
    index($0, "fs_check \"normal\"") {
    print $0
    print "# PETCARD_BOOT_ART_HOOK: mount branded power screens before MMI starts"
    print "if [ -x /mnt/userdata/petcard/prepare_boot.sh ]; then"
    print "    /mnt/userdata/petcard/prepare_boot.sh"
    print "fi"
    print "# PETCARD_BOOT_EARLY_UI_HOOK: start PetCard before MMI and wait for zte_mifi to hold its UI"
    print "if [ -x /mnt/userdata/petcard/autostart.sh ]; then"
    print "    /mnt/userdata/petcard/autostart.sh </dev/null >/mnt/userdata/petcard/autostart-launch.log 2>&1 &"
    print "fi"
    print "# PETCARD_BOOT_DISPLAY_READY_WAIT: let PetCard paint before zte_mifi starts"
    print "for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do"
    print "    [ -f /tmp/petcard-early-ready ] && break"
    print "    sleep 0.1"
    print "done"
    next
}
{ print }
' "$SOURCE" > "$TMP" || exit 1

if ! sh -n "$TMP"; then
    echo "Patched /etc/rc failed shell syntax validation" >&2
    exit 1
fi
if [ "$(grep -c PETCARD_BOOT_ART_HOOK "$TMP")" != 1 ] ||
   [ "$(grep -c PETCARD_BOOT_EARLY_UI_HOOK "$TMP")" != 1 ] ||
   [ "$(grep -c PETCARD_BOOT_DISPLAY_READY_WAIT "$TMP")" != 1 ]; then
    echo "Patched /etc/rc hook count is incorrect" >&2
    exit 1
fi

if ! remount_root rw; then
    if remount_root ro >/dev/null 2>&1; then
        echo "JFFS2 root remains read-only after remount refusal" >> "$LOG"
    else
        echo "CRITICAL: JFFS2 root may remain writable after remount refusal" >> "$LOG"
    fi
    echo "Could not remount the JFFS2 root filesystem read/write" >&2
    exit 1
fi
if ! cp "$TMP" "$RC.new" || ! chmod 744 "$RC.new" || ! mv "$RC.new" "$RC"; then
    if restore_original_rc; then
        echo "Original /etc/rc restored after install failure" >> "$LOG"
    else
        echo "CRITICAL: /etc/rc restoration failed" >> "$LOG"
    fi
    if remount_root ro; then
        echo "JFFS2 root is read-only after install failure" >> "$LOG"
    else
        echo "CRITICAL: JFFS2 root may remain writable" >> "$LOG"
    fi
    echo "Could not install /etc/rc; inspect $LOG over ADB" >&2
    exit 1
fi
if ! remount_root ro; then
    echo "Could not restore read-only mount; rolling back startup file" >> "$LOG"
    if restore_original_rc; then
        echo "Original /etc/rc restored after remount failure" >> "$LOG"
    else
        echo "CRITICAL: /etc/rc rollback failed; check over ADB" >> "$LOG"
    fi
    if remount_root ro; then
        echo "JFFS2 root restored read-only after rollback" >> "$LOG"
    else
        echo "CRITICAL: JFFS2 root remains writable; check over ADB" >> "$LOG"
    fi
    exit 1
fi

if ! sh -n "$RC" || [ "$(grep -c PETCARD_BOOT_ART_HOOK "$RC")" != 1 ] ||
   [ "$(grep -c PETCARD_BOOT_EARLY_UI_HOOK "$RC")" != 1 ] ||
   [ "$(grep -c PETCARD_BOOT_DISPLAY_READY_WAIT "$RC")" != 1 ]; then
    echo "Installed hook validation failed; use rollback_startup.sh" >&2
    exit 1
fi
echo "Installed and validated $(date)" >> "$LOG"
echo "PetCard startup hooks installed"
