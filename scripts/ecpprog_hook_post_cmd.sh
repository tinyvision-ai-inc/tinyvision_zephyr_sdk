#!/bin/sh
set -eu

# Power cycle the board as the FPGA chip seems to be put in "evaluation mode" where it needs
# to be power cycled every 4 hours or after programming.
sh "${0%/*}/do_power_cycle.sh"

# Drop the output as it is used by the parent script to detect the end of the script execution.
# Run in the background to let the board start, and then only perform a reset.
sh "${0%/*}/do_reset.sh" >/dev/null 2>&1 &
