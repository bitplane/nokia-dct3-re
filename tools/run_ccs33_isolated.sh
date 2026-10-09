#!/bin/sh
# Optional legacy-tool research runner; not part of MAME acceptance.
set -eu
if [ "$#" -lt 2 ]; then
    printf 'usage: %s WORK_DIRECTORY COMMAND [ARG ...]\n' "$0" >&2
    exit 2
fi
work=$(realpath "$1")
shift
test -d "$work/home"
test -d "$work/prefix"
limit=${CCS33_TIMEOUT_SECONDS:-120}
case "$limit" in
    ''|*[!0-9]*) printf 'CCS33_TIMEOUT_SECONDS must be a positive integer\n' >&2; exit 2 ;;
esac
if [ "$limit" -lt 1 ] || [ "$limit" -gt 1800 ]; then
    printf 'CCS33_TIMEOUT_SECONDS must be between 1 and 1800\n' >&2
    exit 2
fi

# Hide host data, disable networking and kill all namespace children at exit.
# The command sees only this research directory as writable persistent storage.
exec timeout "$limit" bwrap --unshare-all --die-with-parent \
    --ro-bind / / --tmpfs /mnt --bind "$work" /mnt/work \
    --tmpfs /data --tmpfs /home --tmpfs /root --tmpfs /tmp \
    --proc /proc --dev /dev --chdir /mnt/work \
    --setenv HOME /mnt/work/home --setenv WINEPREFIX /mnt/work/prefix \
    --setenv TMPDIR /tmp \
    --setenv WINEPATH 'C:\CCS33Admin;C:\CCS33Admin\CCStudio_v3.1\cc\bin' \
    -- xvfb-run -a -e /dev/stderr \
        -s '-screen 0 1280x1024x24 -extension GLX' \
        sh -c '"$@"; status=$?; wineserver -k; wineserver -w; exit "$status"' sh "$@"
