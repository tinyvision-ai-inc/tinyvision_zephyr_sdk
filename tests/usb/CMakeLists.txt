# Copyright (c) 2025 tinyVision.ai Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app_usb_enumeration_test)

target_sources(app PRIVATE src/enumeration.c)
