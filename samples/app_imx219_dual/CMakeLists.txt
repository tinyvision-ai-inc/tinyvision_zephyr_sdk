# Copyright (c) 2024 tinyVision.ai Inc.
# SPDX-License-Identifier: Apache-2.0

set(UVCMANAGER "../../../zephyr_internal/uvcmanager.c")

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app)
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/usb.c)

# Run some code from RAM for performance during enumeration
zephyr_code_relocate(LIBRARY drivers__usb__udc LOCATION RAM)
zephyr_code_relocate(LIBRARY drivers__usbd LOCATION RAM)
