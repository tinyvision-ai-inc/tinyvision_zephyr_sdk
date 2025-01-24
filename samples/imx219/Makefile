TTY = /dev/ttyUSB1
BOARD = tinyclunx33@rev2/rtl011
SHIELD = tinyclunx33_devkit_rev2
UVCMANAGER = $$(readlink -f ../../../zephyr_internal/uvcmanager.c)

all:
	west build --board $(BOARD) --shield $(SHIELD) -- -DUVCMANAGER=$(UVCMANAGER)
	find build -path '*/zephyr_internal/uvcmanager.c.obj' -exec cp {} ../../../uvcmanager.o \;

clean:
	rm -rf build

flash:
	west $@

picocom:
	picocom -b 156200 $(TTY)
