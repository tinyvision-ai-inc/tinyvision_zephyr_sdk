# This script is to be sent to a MicroPython-enabled board connected to the tinyCLUNX33 devkit
#
#	mpremote run mpremote_soft_reset.py

import machine, time

pin = machine.Pin(20, machine.Pin.OPEN_DRAIN)

pin.value(0)
time.sleep(0.5)
pin.value(1)
