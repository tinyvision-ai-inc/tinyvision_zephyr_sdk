/*
 * Copyright (c) 2024-2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT tinyvision_debayer

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tvai_debayer, CONFIG_VIDEO_LOG_LEVEL);

#define TVAI_DEBAYER_PIX_FMT VIDEO_PIX_FMT_SBGGR8

struct tvai_debayer_config {
	const struct device *source_dev;
};

/* Used to tune the video format caps from the source at runtime */
static struct video_format_cap fmts[10];

static int tvai_debayer_get_caps(const struct device *dev, struct video_caps *caps)
{
	const struct tvai_debayer_config *cfg = dev->config;
	int ret;

	ret = video_get_caps(cfg->source_dev, caps);
	if (ret < 0) {
		return ret;
	}

	/* Adjust the formats according to the conversion done in hardware */
	for (int i = 0; caps->format_caps[i].pixelformat != 0; i++) {
		if (i + 1 >= ARRAY_SIZE(fmts)) {
			LOG_WRN("not enough format capabilities");
		}

		fmts[i].pixelformat = VIDEO_PIX_FMT_YUYV;
		fmts[i].width_min = MAX(caps->format_caps[i].width_min - 2, 0);
		fmts[i].width_max = MAX(caps->format_caps[i].width_max - 2, 0);
		fmts[i].height_min = MAX(caps->format_caps[i].height_min - 2, 0);
		fmts[i].height_max = MAX(caps->format_caps[i].height_max - 2, 0);
	}

	caps->format_caps = fmts;
	return 0;
}

static int tvai_debayer_set_format(const struct device *dev, struct video_format *fmt)
{
	const struct tvai_debayer_config *cfg = dev->config;
	const struct device *source_dev = cfg->source_dev;
	struct video_format source_fmt = *fmt;
	int ret;

	if (fmt->pixelformat != VIDEO_PIX_FMT_YUYV) {
		LOG_ERR("Only YUYV is supported as output format");
		return -ENOTSUP;
	}

	/* Apply the conversion done by hardware to the format */
	source_fmt.width += 2;
	source_fmt.height += 2;
	source_fmt.pixelformat = TVAI_DEBAYER_PIX_FMT;

	LOG_DBG("setting %s to %ux%u", source_dev->name, source_fmt.width, source_fmt.height);

	ret = video_set_format(source_dev, &source_fmt);
	if (ret < 0) {
		LOG_ERR("failed to set %s format", source_dev->name);
		return ret;
	}

	return 0;
}

static int tvai_debayer_get_format(const struct device *dev, struct video_format *fmt)
{
	const struct tvai_debayer_config *cfg = dev->config;
	int ret;

	ret = video_get_format(cfg->source_dev, fmt);
	if (ret < 0) {
		LOG_ERR("failed to get %s format", cfg->source_dev->name);
		return ret;
	}

	LOG_DBG("%s format is %ux%u, stripping 2 pixels vertically and horizontally",
		cfg->source_dev->name, fmt->width, fmt->height);

	/* Apply the conversion done by hardware to the format */
	fmt->width -= 2;
	fmt->height -= 2;
	fmt->pixelformat = VIDEO_PIX_FMT_YUYV;

	return 0;
}

static int tvai_debayer_set_frmival(const struct device *dev, struct video_frmival *frmival)
{
	const struct tvai_debayer_config *cfg = dev->config;

	return video_set_frmival(cfg->source_dev, frmival);
}

static int tvai_debayer_get_frmival(const struct device *dev, struct video_frmival *frmival)
{
	const struct tvai_debayer_config *cfg = dev->config;

	return video_get_frmival(cfg->source_dev, frmival);
}

static int tvai_debayer_enum_frmival(const struct device *dev, struct video_frmival_enum *fie)
{
	const struct tvai_debayer_config *cfg = dev->config;
	const struct video_format *prev_fmt = fie->format;
	struct video_format fmt = *fie->format;
	int ret;

	if (fie->format->pixelformat != VIDEO_PIX_FMT_YUYV) {
		LOG_ERR("Only YUYV is supported");
		return -ENOTSUP;
	}

	fmt.width += 2;
	fmt.height += 2;
	fmt.pixelformat = TVAI_DEBAYER_PIX_FMT,

	fie->format = &fmt;
	ret = video_enum_frmival(cfg->source_dev, fie);
	fie->format = prev_fmt;

	return ret;
}

static int tvai_debayer_set_stream(const struct device *dev, bool on, enum video_buf_type type)
{
	const struct tvai_debayer_config *cfg = dev->config;

	return on ? video_stream_start(cfg->source_dev, type)
		  : video_stream_stop(cfg->source_dev, type);
}

static const DEVICE_API(video, tvai_debayer_driver_api) = {
	.set_format = tvai_debayer_set_format,
	.get_format = tvai_debayer_get_format,
	.get_caps = tvai_debayer_get_caps,
	.set_frmival = tvai_debayer_set_frmival,
	.get_frmival = tvai_debayer_get_frmival,
	.enum_frmival = tvai_debayer_enum_frmival,
	.set_stream = tvai_debayer_set_stream,

};

#define TVAI_DEBAYER_INIT(n)                                                                       \
	const static struct tvai_debayer_config tvai_debayer_cfg_##n = {                           \
		.source_dev =                                                                      \
			DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(DT_INST_ENDPOINT_BY_ID(n, 0, 0))),     \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &tvai_debayer_cfg_##n, POST_KERNEL,             \
			      CONFIG_VIDEO_INIT_PRIORITY, &tvai_debayer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TVAI_DEBAYER_INIT)
