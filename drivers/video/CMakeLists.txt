# Copyright (c) 2024 tinyVision.ai Inc.
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

if (NOT DEFINED UVCMANAGER)
  set(UVCMANAGER ${ZEPHYR_BASE}/../zephyr_private/uvcmanager.6.o)
endif()

zephyr_include_directories(.)
zephyr_library_sources(video_common.c)
zephyr_library_sources_ifdef(CONFIG_VIDEO_IMX219 imx219.c)
zephyr_library_sources_ifdef(CONFIG_VIDEO_IMX296 imx296.c)
zephyr_library_sources_ifdef(CONFIG_VIDEO_IMX477 imx477.c)
zephyr_library_sources_ifdef(CONFIG_VIDEO_LATTICE_DPHY_RX dphy_rx.c)
zephyr_library_sources_ifdef(CONFIG_VIDEO_SHELL video_shell.c)
zephyr_library_sources_ifdef(CONFIG_VIDEO_TINYVISION_DEBAYER debayer.c)
zephyr_library_sources_ifdef(CONFIG_VIDEO_TINYVISION_TESTPATTERN testpattern.c)
zephyr_library_sources_ifdef(CONFIG_VIDEO_TINYVISION_UVCMANAGER uvcmanager.c ${UVCMANAGER})
