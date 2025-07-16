# Minimal Example

This is a minimal example of the tinyCLUNX33 SoC adapted for use by MSSC on a CertusPro-NX100 FPGA
board instead of a CrossLinkU-NX33.

This is a normal Zephyr application running on top of the lastest Zephyr version
(v4.2.0-rc3 at the time of writing this).

## Building

The indication from Zephyr's [Getting Started][1] will give a complete overview of how to get an
environment setup.

The [Applilcation Development][2] environment will show how to initialize a workspace with this
application (with `west init -m <this-repo-url>`).

[1]: https://docs.zephyrproject.org/latest/develop/getting_started/index.html
[2]: https://docs.zephyrproject.org/latest/develop/application/index.html#creating-an-application

Then, navigate to this directory and run `build.sh` or:

```
west build --board tinyclunx33
```

If running on Linux, you might be able to compile and install
[`ecpprog`](https://github.com/gregdavill/ecpprog):

```
west flash
```

## Running

After programming the device and power cycling the board, use the serial console provided by the
FTDI such as `/dev/ttyUSB1` on Linux, or any system, the 2nd serial interface out of the pair that
shows-up when connecting the board.

And observe the presence of the boot banner and/or UART prompt:

```
$ picocom -q -b 115200 /dev/ttyUSB1
*** Booting Zephyr OS build v4.0.0-3411-g589229cc5e12 ***

uart:~$
```

Note that you might have to hit the reset switch of your board to catch the early boot logs.

Hitting `<enter>` is expected to make the shell prompt show-up.
