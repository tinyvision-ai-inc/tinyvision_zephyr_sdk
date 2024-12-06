tinyCLUNX33 SDK
###############

The tinyCLUNX33 SDK is a `Zephyr <https://zephyrproject.org/>`_
library that contains extra drivers and configuration for working with
the `tinyCLUNX33`_ USB3/MIPI module and its devkit.

.. _tinyCLUNX33: https://tinyclunx33.tinyvision.ai


Getting started
***************

This SDK being a Zephyr library, the first step is to follow the `Getting Started Guide <getting_started_>`_ from Zephyr.

.. _getting_started: https://docs.zephyrproject.org/latest/develop/getting_started/index.html


Getting the SDK
***************

The tinyCLUNX33 SDK is a regular Zephyr module repository, and it can
be included into any project by adding it to your project `west.yml`
configuration file.

For instance, the `tinyclunx33_zephyr_example`_ repository is
configured to depend on this ``tinyclunx33_sdk`` in its `west.yml`_
file.

.. _tinyclunx33_zephyr_example: https://github.com/tinyvision-ai-inc/tinyclunx33_zephyr_example/tree/tinyclunx33_sdk
.. _west.yml: https://github.com/tinyvision-ai-inc/tinyclunx33_zephyr_example/blob/tinyclunx33_sdk/west.yml

.. code-block:: console

   # Reset the workspace directory you created from Zephyr's Getting Started Guide
   cd ~/zephyrproject
   rm -rf .west

   # Clone the example repository recursively
   west init -m https://github.com/tinyvision-ai-inc/tinyclunx33_zephyr_example
   west update

You should now have all resources needed to build a complete firmware image.


Building a firmware image
*************************

This guide uses the `tinyclunx33_zephyr_example`_ sample application named ``example_usb_uvc_imx219``:

The tinyCLUNX33 is a System on Module (SoM) that can be plugged on a
Devkit board or custom PCB, and the FPGA can have various systems
images loaded. These elements can be selected while building the
firmware:

* ``tinyclunx33_devkit_rev1`` and ``tinyclunx33_devkit_rev2`` shields

* ``tinyclunx33@rev1` and ``tinyclunx33@rev2`` boards

* ``rtl008``, ``rtl009``, ``rtl010`` system images

For example:

.. code-block:: console

   cd ~/zephyrproject/tinyclunx33_zephyr_example/example_usb_uvc_imx219/

   west build --board tinyclunx33@rev2/rtl010 --shield tinyclunx33_devkit_rev2

The ``ecpprog`` utility is integrated with ``west`` and can be called this way:

.. code-block:: console

   west flash


Troubleshooting
***************

The `tinyCLUNX33`_ documentation contains troubleshooting sections
scattered through the relevant topics.

tinyVision.ai is glad to offer help to get you started with this board
via the `Discord <https://discord.com/invite/3qbXujE>`_ chat server or
by `Email <sales@tinyvision.ai>`_.


Going beyond
************

Application notes are published here: <https://tinyclunx33.tinyvision.ai/md_appnote.html>`_.
