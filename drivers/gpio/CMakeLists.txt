# SPDX-License-Identifier: Apache-2.0

zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/gpio.h)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_GPIO_PCAL64XXA  gpio_pcal64xxa.c)


