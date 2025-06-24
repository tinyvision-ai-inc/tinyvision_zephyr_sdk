Camera Shield Example
#####################

This example is there to allow use any supported Zephyr image sensor shield by passing an extra
``--shield ...`` argument when building.


.. code-block:: console

   . ./env.sh
   west build --shield <zephyr_shield_name>
   west flash
