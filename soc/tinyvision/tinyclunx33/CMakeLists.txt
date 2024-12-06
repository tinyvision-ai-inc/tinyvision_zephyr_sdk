# Copyright (c) 2024 tinyVision.ai Inc.
# SPDX-License-Identifier: Apache-2.0

zephyr_sources(soc.c)
zephyr_sources(${ZEPHYR_BASE}/soc/common/riscv-privileged/soc_irq.S)
zephyr_sources(${ZEPHYR_BASE}/soc/common/riscv-privileged/vector.S)

zephyr_include_directories(.)

set(SOC_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld CACHE INTERNAL "")
