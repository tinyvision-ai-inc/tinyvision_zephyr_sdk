TTY = /dev/ttyUSB1
RTL = rtl010
BAUD = 90000

all:
	west twister -T . --device-testing --inline-logs --log-level DEBUG --hardware-map hardware-map.yml --west-flash --flash-before
