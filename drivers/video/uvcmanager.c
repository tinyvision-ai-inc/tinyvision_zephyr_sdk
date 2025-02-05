/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT tinyvision_uvcmanager

#include <stdint.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

#include "uvcmanager.h"

LOG_MODULE_REGISTER(uvcmanager, CONFIG_VIDEO_LOG_LEVEL);

static int uvcmanager_stream_start(const struct device *dev)
{
	const struct uvcmanager_config *cfg = dev->config;
	int ret;

	LOG_INF("%s: starting %s", dev->name, cfg->source_dev->name);
	ret = video_stream_start(cfg->source_dev);
	if (ret < 0) {
		LOG_ERR("%s: failed to start %s", dev->name, cfg->source_dev->name);
		return ret;
	}

	k_sleep(K_MSEC(1));
	uvcmanager_lib_start(cfg);

	return 0;
}

static int uvcmanager_stream_stop(const struct device *dev)
{
	const struct uvcmanager_config *cfg = dev->config;
	int ret;

	uvcmanager_lib_stop(cfg);

	LOG_INF("%s: stopping %s", dev->name, cfg->source_dev->name);
	ret = video_stream_stop(cfg->source_dev);
	if (ret < 0) {
		LOG_ERR("%s: failed to stop %s", dev->name, cfg->source_dev->name);
		return ret;
	}

	return 0;
}

static int uvcmanager_get_caps(const struct device *dev, enum video_endpoint_id ep, struct video_caps *caps)
{
	const struct uvcmanager_config *cfg = dev->config;

	return video_get_caps(cfg->source_dev, ep, caps);
}

static int uvcmanager_set_format(const struct device *dev, enum video_endpoint_id ep, struct video_format *fmt)
{
	struct uvcmanager_data *data = dev->data;
	const struct uvcmanager_config *cfg = dev->config;
	const struct device *sdev = cfg->source_dev;
	struct video_format sfmt = *fmt;
	int ret;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	if (sdev == NULL) {
		LOG_DBG("%s: no source, skipping format selection", dev->name);
		return 0;
	}

	LOG_INF("setting %s to %ux%u", sdev->name, sfmt.width, sfmt.height);
	ret = video_set_format(sdev, VIDEO_EP_OUT, &sfmt);
	if (ret < 0) {
		LOG_ERR("%s: failed to set %s format", dev->name, sdev->name);
		return ret;
	}

	uvcmanager_lib_set_format(cfg, fmt->pitch, fmt->height);

	data->format = *fmt;
	return 0;
}

static int uvcmanager_get_format(const struct device *dev, enum video_endpoint_id ep, struct video_format *fmt)
{
	struct uvcmanager_data *data = dev->data;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	*fmt = data->format;
	return 0;
}

static int uvcmanager_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct uvcmanager_config *cfg = dev->config;
	int ret;

	switch (cid) {
	case VIDEO_CID_TEST_PATTERN:
		if ((int)value == 0) {
			LOG_DBG("Disabling the test pattern");
			uvcmanager_lib_set_test_pattern(cfg, 0, 0, 0);
		} else {
			struct video_format fmt;

			ret = uvcmanager_get_format(dev, VIDEO_EP_OUT, &fmt);
			if (ret < 0) {
				return ret;
			}

			LOG_DBG("Setting test pattern to %ux%u", fmt.width, fmt.height);
			uvcmanager_lib_set_test_pattern(cfg, fmt.width, fmt.height, (int)value);
		}
		return 0;
	default:
		LOG_WRN("Control not supported");
		return -ENOTSUP;
	}
}

static int uvcmanager_set_frmival(const struct device *dev, enum video_endpoint_id ep,
			       struct video_frmival *frmival)
{
	const struct uvcmanager_config *cfg = dev->config;

	return video_set_frmival(cfg->source_dev, VIDEO_EP_OUT, frmival);
}

static int uvcmanager_get_frmival(const struct device *dev, enum video_endpoint_id ep,
			       struct video_frmival *frmival)
{
	const struct uvcmanager_config *cfg = dev->config;

	return video_get_frmival(cfg->source_dev, VIDEO_EP_OUT, frmival);
}

static int uvcmanager_enum_frmival(const struct device *dev, enum video_endpoint_id ep,
				struct video_frmival_enum *fie)
{
	const struct uvcmanager_config *cfg = dev->config;

	return video_enum_frmival(cfg->source_dev, VIDEO_EP_OUT, fie);
}

static int uvcmanager_enqueue(const struct device *dev, enum video_endpoint_id ep,
			   struct video_buffer *vbuf)
{
	struct uvcmanager_data *data = dev->data;

	/* Can only enqueue a buffer to get data out, data input is from hardware */
	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	/* The buffer has not been filled yet: flag as emtpy */
	vbuf->bytesused = 0;

	/* Submit the buffer for processing in the worker, where everything happens */
	k_fifo_put(&data->fifo_in, vbuf);
	k_work_submit(&data->work);

	return 0;
}

static int uvcmanager_dequeue(const struct device *dev, enum video_endpoint_id ep,
			   struct video_buffer **vbufp, k_timeout_t timeout)
{
	struct uvcmanager_data *data = dev->data;

	/* Can only dequeue a buffer to get data out, data input is from hardware */
	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	/* All the processing is expected to happen in the worker */
	*vbufp = k_fifo_get(&data->fifo_out, timeout);
	if (*vbufp == NULL) {
		return -EAGAIN;
	}

	return 0;
}

static void uvcmanager_worker(struct k_work *work)
{
	struct uvcmanager_data *data = CONTAINER_OF(work, struct uvcmanager_data, work);
	const struct device *dev = data->dev;
	const struct uvcmanager_config *cfg = dev->config;
	struct video_buffer *vbuf = vbuf;
	int ret;

	while ((vbuf = k_fifo_get(&data->fifo_in, K_NO_WAIT)) != NULL) {
		vbuf->bytesused = vbuf->size;
		vbuf->line_offset = 0;

		LOG_DBG("Inserting %u bytes into buffer %p", vbuf->size, vbuf->buffer);

		ret = uvcmanager_lib_read(cfg, data, vbuf->buffer, vbuf->size);
		if (ret < 0) {
			LOG_ERR("Reading data from the UVC Manager failed: %s", strerror(-ret));
		}

		/* Once the buffer is completed, submit it to the video buffer */
		k_fifo_put(&data->fifo_out, vbuf);
	}
}

static int uvcmanager_init(const struct device *dev)
{
	const struct uvcmanager_config *cfg = dev->config;
	struct uvcmanager_data *data = dev->data;
	struct video_format fmt;
	int ret;

	if (!device_is_ready(cfg->dwc3_dev)) {
		LOG_ERR("%s is not ready", cfg->dwc3_dev->name);
		return -ENODEV;
	}
	if (!device_is_ready(cfg->uvc_dev)) {
		LOG_ERR("%s is not ready", cfg->uvc_dev->name);
		return -ENODEV;
	}
	if (!device_is_ready(cfg->source_dev)) {
		LOG_ERR("%s is not ready", cfg->source_dev->name);
		return -ENODEV;
	}

	/* Query the source format */
	ret = video_get_format(cfg->source_dev, VIDEO_EP_OUT, &fmt);
	if (ret < 0) {
		LOG_ERR("Failed to query %s format", cfg->source_dev->name);
		return ret;
	}

	/* Set our own format to match it */
	ret = video_set_format(dev, VIDEO_EP_OUT, &fmt);
	if (ret < 0) {
		return ret;
	}

	k_fifo_init(&data->fifo_in);
	k_fifo_init(&data->fifo_out);
	k_work_init(&data->work, &uvcmanager_worker);

	uvcmanager_lib_init(cfg);
	return 0;
}

static const DEVICE_API(video, uvcmanager_driver_api) = {
	.set_format = uvcmanager_set_format,
	.get_format = uvcmanager_get_format,
	.get_caps = uvcmanager_get_caps,
	.set_frmival = uvcmanager_set_frmival,
	.get_frmival = uvcmanager_get_frmival,
	.enum_frmival = uvcmanager_enum_frmival,
	.stream_start = uvcmanager_stream_start,
	.stream_stop = uvcmanager_stream_stop,
	.set_ctrl = uvcmanager_set_ctrl,
	.enqueue = uvcmanager_enqueue,
	.dequeue = uvcmanager_dequeue,
};

#define SRC_EP(inst) DT_INST_ENDPOINT_BY_ID(inst, 0, 0)
#define UVC_EP(inst) DT_INST_ENDPOINT_BY_ID(inst, 0, 1)

#define UVCMANAGER_DEVICE_DEFINE(inst)                                                             \
                                                                                                   \
	const struct uvcmanager_config uvcmanager_cfg_##inst = {                                   \
		.source_dev = DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(SRC_EP(inst))),                  \
		.dwc3_dev = DEVICE_DT_GET(DT_BUS(DT_NODE_REMOTE_DEVICE(UVC_EP(inst)))),            \
		.uvc_dev = DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(UVC_EP(inst))),                     \
		.usb_endpoint = DT_INST_PROP(inst, usb_endpoint),                                  \
		.base = DT_INST_REG_ADDR_BY_NAME(inst, base),                                      \
		.fifo = DT_INST_REG_ADDR_BY_NAME(inst, fifo),                                      \
	};                                                                                         \
                                                                                                   \
	struct uvcmanager_data uvcmanager_data_##inst = {                                          \
		.dev = DEVICE_DT_INST_GET(inst),                                                   \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, uvcmanager_init, NULL, &uvcmanager_data_##inst,                \
			      &uvcmanager_cfg_##inst, POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY,     \
			      &uvcmanager_driver_api);

DT_INST_FOREACH_STATUS_OKAY(UVCMANAGER_DEVICE_DEFINE)

#ifdef CONFIG_SHELL

static bool device_is_video_and_ready(const struct device *dev)
{
	return device_is_ready(dev) && DEVICE_API_IS(video, dev);
}

static void complete_video_device(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_filter(idx, device_is_video_and_ready);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}
SHELL_DYNAMIC_CMD_CREATE(dsub_video_device, complete_video_device);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_uvcmanager,
	SHELL_CMD_ARG(show, &dsub_video_device,
		     "Show statistics about the uvcmanager core\n" "Usage: show <device>",
		     uvcmanager_cmd_show, 2, 0),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(uvcmanager, &sub_uvcmanager, "UVC Manager debug commands", NULL);

#endif /* CONFIG_SHELL */
