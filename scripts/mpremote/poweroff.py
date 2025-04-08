import machine
import time

# RPiPico.GPIO20 <--> tinyCLUNX33.PROGN
reset = machine.Pin(20, machine.Pin.OPEN_DRAIN)

# RPiPico.GPIO22 <--> tinyCLUNX33.EN
power = machine.Pin(22, machine.Pin.OUT)

power.off()
