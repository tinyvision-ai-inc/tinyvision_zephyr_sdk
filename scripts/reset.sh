#!/bin/sh
set -eu

PICOCOM="picocom --quiet --exit-after 10"
GPIO="gpio@48000000 1"
TTY="${TTY:-/dev/ttyACM0}"
NL="
"

$PICOCOM --initstring "gpio set $GPIO 0 $NL" "$TTY" </dev/null >&2
$PICOCOM --initstring "gpio conf $GPIO o $NL" "$TTY" </dev/null >&2
$PICOCOM --initstring "gpio conf $GPIO i $NL" "$TTY" </dev/null >&2
echo
