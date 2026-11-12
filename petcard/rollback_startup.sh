#!/bin/sh
# ADB recovery path: restore the pre-PetCard /etc/rc without touching NV or MTD.
BASE=/mnt/userdata/petcard
BACKUP=$BASE/rollback/rc

remount_root() {
    mode=$1
    mount -o "remount,$mode" /dev/root / || return 1
    awk -v mode="$mode" '$1 == "/dev/root" && $2 == "/" && $3 == "jffs2" && $4 ~ ("^" mode) { found = 1 } END { exit !found }' /proc/mounts
}

if [ ! -s "$BACKUP" ]; then
    echo "No saved /etc/rc rollback copy" >&2
    exit 1
fi
if ! remount_root rw; then
    if remount_root ro >/dev/null 2>&1; then
        echo "JFFS2 root remains read-only after remount refusal" >&2
    else
        echo "CRITICAL: JFFS2 root may remain writable; inspect over ADB" >&2
    fi
    echo "Could not remount the JFFS2 root filesystem read/write" >&2
    exit 1
fi
if ! cp "$BACKUP" /etc/rc.new || ! chmod 744 /etc/rc.new || ! mv /etc/rc.new /etc/rc; then
    if remount_root ro; then
        echo "JFFS2 root is read-only; /etc/rc remains unchanged" >&2
    else
        echo "CRITICAL: JFFS2 root may remain writable; inspect over ADB" >&2
    fi
    echo "Could not restore /etc/rc" >&2
    exit 1
fi
if ! remount_root ro; then
    echo "Warning: root filesystem remained writable" >&2
    exit 1
fi
echo "Original startup restored; reboot to apply"
