/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

struct uvcmanager_config {
	const struct device *source_dev;
	const struct device *dwc3_dev;
	const struct device *uvc_dev;
	uintptr_t base;
	uintptr_t fifo;
	uint8_t usb_endpoint;
};

struct uvcmanager_data {
	struct video_format format;
	struct video_buffer *vbuf;
};

void uvcmanager_lib_init(const struct uvcmanager_config *cfg);
void uvcmanager_lib_start(const struct uvcmanager_config *cfg);
int uvcmanager_lib_set_format(const struct uvcmanager_config *cfg, uint32_t pitch, uint32_t height);
int uvcmanager_cmd_conf(const struct shell *sh, size_t argc, char **argv);
