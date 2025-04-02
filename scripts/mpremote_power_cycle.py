# This script is to be sent to a MicroPython-enabled board connected to the tinyCLUNX33 devkit
#
#	mpremote run mpremote_power_cycle.py

import machine, time

pin = machine.Pin(22, machine.Pin.OUT)

pin.value(0)
time.sleep(0.5)
pin.value(1)
