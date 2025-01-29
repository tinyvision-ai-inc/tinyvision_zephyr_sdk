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
	const struct device *dev;
	struct video_format format;
	struct video_buffer *vbuf;
	size_t id;
	struct k_work work;
	struct k_fifo fifo_in;
	struct k_fifo fifo_out;
};

void uvcmanager_lib_init(const struct uvcmanager_config *cfg);
void uvcmanager_lib_start(const struct uvcmanager_config *cfg);
void uvcmanager_lib_stop(const struct uvcmanager_config *cfg);
void uvcmanager_lib_set_test_pattern(const struct uvcmanager_config *cfg, bool on);
void uvcmanager_lib_set_format(const struct uvcmanager_config *cfg, uint32_t pitch, uint32_t height);
int uvcmanager_lib_read(const struct uvcmanager_config *cfg, struct uvcmanager_data *data,
			uint8_t *buf_data, size_t buf_size);
int uvcmanager_cmd_show(const struct shell *sh, size_t argc, char **argv);
