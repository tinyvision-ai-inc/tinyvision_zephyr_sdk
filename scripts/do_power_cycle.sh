#!/bin/sh
set -eu

GPIO="gpio@48000000 0"
TTY="${TTY:-/dev/ttyACM0}"
NL="
"
picocom --quiet --exit-after 10 --initstring "gpio conf $GPIO id $NL" "$TTY" </dev/null >&2
picocom --quiet --exit-after 10 --initstring "gpio conf $GPIO iu $NL" "$TTY" </dev/null >&2
echo
