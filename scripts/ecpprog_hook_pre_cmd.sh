#!/bin/sh
set -eu

# Reboot the board in the foreground
mpremote run "${0%/*}/mpremote_power_cycle.py"
