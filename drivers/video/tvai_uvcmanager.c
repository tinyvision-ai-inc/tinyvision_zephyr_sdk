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

#include "udc_dwc3.h"
#include "udc_common.h"
#include "tvai_uvcmanager.h"

LOG_MODULE_REGISTER(uvcmanager, CONFIG_VIDEO_LOG_LEVEL);

struct uvcmanager_config {
	const struct device *source_dev;
	const struct device *dwc3_dev;
	uintptr_t base;
	uintptr_t fifo;
	uint8_t usb_endpoint;
};

struct uvcmanager_data {
	const struct device *dev;
	struct video_buffer *vbuf;
	size_t id;
	struct k_work work;
	struct k_fifo fifo_in;
	struct k_fifo fifo_out;
};

/*
 * Direct connection APIs with DWC3 register information.
 */

uint32_t dwc3_get_trb_addr(const struct device *dev, uint8_t ep_addr)
{
	struct dwc3_ep_data *ep_data = (void *)udc_get_ep_cfg(dev, ep_addr);

	return (uint32_t)ep_data->trb_buf;
}

uint32_t dwc3_get_depcmd(const struct device *dev, uint8_t ep_addr)
{
	const struct dwc3_config *cfg = dev->config;

	return cfg->base + DWC3_DEPCMD(EP_PHYS_NUMBER(ep_addr));
}

uint32_t dwc3_get_depupdxfer(const struct device *dev, uint8_t ep_addr)
{
	struct dwc3_ep_data *ep_data = (void *)udc_get_ep_cfg(dev, ep_addr);

	return DWC3_DEPCMD_DEPUPDXFER |
		FIELD_PREP(DWC3_DEPCMD_XFERRSCIDX_MASK, ep_data->xferrscidx);
}

static int uvcmanager_set_stream(const struct device *dev, bool on)
{
	const struct uvcmanager_config *cfg = dev->config;
	uint32_t trb_addr = dwc3_get_trb_addr(cfg->dwc3_dev, cfg->usb_endpoint);
	uint32_t depupdxfer = dwc3_get_depupdxfer(cfg->dwc3_dev, cfg->usb_endpoint);
	uint32_t depcmd = dwc3_get_depcmd(cfg->dwc3_dev, cfg->usb_endpoint);
	int ret;

	if (on) {
		LOG_DBG("Starting %s, then %s", cfg->source_dev->name, dev->name);
		LOG_DBG("trb addr 0x%08x, depupdxfer 0x%02x, depcmd 0x%08x",
			trb_addr, depupdxfer, depcmd);

		ret = video_stream_start(cfg->source_dev);
		if (ret < 0) {
			LOG_ERR("%s: failed to start %s", dev->name, cfg->source_dev->name);
			return ret;
		}

		uvcmanager_lib_start(cfg->base, trb_addr, depupdxfer, depcmd);
	} else {
		LOG_DBG("Stopping %s, then %s", dev->name, cfg->source_dev->name);

		uvcmanager_lib_stop(cfg->base);

		ret = video_stream_stop(cfg->source_dev);
		if (ret < 0) {
			LOG_ERR("%s: failed to stop %s", dev->name, cfg->source_dev->name);
			return ret;
		}
	}

	return 0;
}

static int uvcmanager_get_caps(const struct device *dev, enum video_endpoint_id ep,
			       struct video_caps *caps)
{
	const struct uvcmanager_config *cfg = dev->config;

	return video_get_caps(cfg->source_dev, ep, caps);
}

static int uvcmanager_set_format(const struct device *dev, enum video_endpoint_id ep,
				 struct video_format *fmt)
{
	const struct uvcmanager_config *cfg = dev->config;
	const struct device *source_dev = cfg->source_dev;
	struct video_format source_fmt = *fmt;
	int ret;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	if (source_dev == NULL) {
		LOG_DBG("%s: no source, skipping format selection", dev->name);
		return 0;
	}

	LOG_INF("setting %s to %ux%u", source_dev->name, source_fmt.width, source_fmt.height);

	ret = video_set_format(source_dev, VIDEO_EP_OUT, &source_fmt);
	if (ret < 0) {
		LOG_ERR("failed to set %s format", source_dev->name);
		return ret;
	}

	uvcmanager_lib_set_format(cfg->base, fmt->pitch, fmt->height);
	return 0;
}

static int uvcmanager_get_format(const struct device *dev, enum video_endpoint_id ep,
				 struct video_format *fmt)
{
	const struct uvcmanager_config *cfg = dev->config;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	return video_get_format(cfg->source_dev, VIDEO_EP_OUT, fmt);
}

static int uvcmanager_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct uvcmanager_config *cfg = dev->config;
	int ret;

	switch (cid) {
	case VIDEO_CID_TEST_PATTERN:
		if ((int)value == 0) {
			LOG_DBG("Disabling the test pattern");
			uvcmanager_lib_set_test_pattern(cfg->base, 0, 0, 0);
		} else {
			struct video_format fmt;

			ret = uvcmanager_get_format(dev, VIDEO_EP_OUT, &fmt);
			if (ret < 0) {
				return ret;
			}

			LOG_DBG("Setting test pattern to %ux%u", fmt.width, fmt.height);
			uvcmanager_lib_set_test_pattern(cfg->base, fmt.width, fmt.height, (int)value);
		}
		return 0;
	default:
		return video_set_ctrl(cfg->source_dev, cid, value);
	}
}

static int uvcmanager_get_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct uvcmanager_config *cfg = dev->config;

	return video_get_ctrl(cfg->source_dev, cid, value);
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
	struct video_buffer *vbuf;

	while ((vbuf = k_fifo_get(&data->fifo_in, K_NO_WAIT)) != NULL) {
		vbuf->bytesused = vbuf->size;
		vbuf->line_offset = 0;

		LOG_DBG("Inserting %u bytes into buffer %p", vbuf->size, vbuf->buffer);
		uvcmanager_lib_read(cfg->base, vbuf->buffer, vbuf->size);
		k_fifo_put(&data->fifo_out, vbuf);
	}
}

int uvcmanager_cmd_show(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	const struct uvcmanager_config *cfg;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Device %s not found", argv[1]);
		return -ENODEV;
	}

	cfg = dev->config;
	uvcmanager_lib_cmd_show(cfg->base, sh);

	return 0;
}

static int uvcmanager_init(const struct device *dev)
{
	const struct uvcmanager_config *cfg = dev->config;
	struct uvcmanager_data *data = dev->data;

	k_fifo_init(&data->fifo_in);
	k_fifo_init(&data->fifo_out);
	k_work_init(&data->work, &uvcmanager_worker);

	uvcmanager_lib_init(cfg->base, cfg->fifo);
	return 0;
}

static const DEVICE_API(video, uvcmanager_driver_api) = {
	.set_format = uvcmanager_set_format,
	.get_format = uvcmanager_get_format,
	.get_caps = uvcmanager_get_caps,
	.set_frmival = uvcmanager_set_frmival,
	.get_frmival = uvcmanager_get_frmival,
	.enum_frmival = uvcmanager_enum_frmival,
	.set_stream = uvcmanager_set_stream,
	.set_ctrl = uvcmanager_set_ctrl,
	.get_ctrl = uvcmanager_get_ctrl,
	.enqueue = uvcmanager_enqueue,
	.dequeue = uvcmanager_dequeue,
};

#define SRC_EP(inst) DT_INST_ENDPOINT_BY_ID(inst, 0, 0)
#define UVC_EP(inst) DT_INST_ENDPOINT_BY_ID(inst, 0, 1)

#define UVCMANAGER_DEVICE_DEFINE(inst)                                                             \
	const struct uvcmanager_config uvcmanager_cfg_##inst = {                                   \
		.source_dev = DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(SRC_EP(inst))),                  \
		.dwc3_dev = DEVICE_DT_GET(DT_INST_PHANDLE(inst, usb_controller)),                  \
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
