TTY = /dev/ttyUSB1
RTL = rtl010
BAUD = 90000

all:
	west twister -T tests --integration --inline-logs --device-testing --log-level DEBUG \
		--platform tinyclunx33@rev2/$(RTL) --timeout-multiplier 10 \
		--device-serial $(TTY) --device-serial-baud $(BAUD) \
		--west-flash --flash-before --west-runner ecpprog_hook
