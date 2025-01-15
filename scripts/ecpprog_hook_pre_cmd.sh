#!/bin/sh
set -eu

# Reboot the board in the foreground
sh "${0%/*}/do_power_cycle.sh"
