#!/bin/sh
set -eu

exec >/tmp/log 2>&1

echo "Running ${0%/*}/reboot.sh in the foreground" >&2
sh "${0%/*}/reboot.sh"

echo "Running ${0%/*}/reset.sh in the background after a few seconds" >&2
sleep 2 && sh "${0%/*}/reset.sh" &
