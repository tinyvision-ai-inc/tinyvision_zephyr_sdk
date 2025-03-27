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

LOG_MODULE_REGISTER(debayer, CONFIG_VIDEO_LOG_LEVEL);

#define DEBAYER_SCRATCH    0x0000
#define DEBAYER_CONTROL    0x0004
#define DEBAYER_STATUS     0x0008
#define DEBAYER_CONFIG     0x000C
#define DEBAYER_NUM_FRAMES 0x0010
#define DEBAYER_IMAGE_GAIN 0x0018
#define DEBAYER_CHAN_AVG_0 0x001C
#define DEBAYER_CHAN_AVG_1 0x0020
#define DEBAYER_CHAN_AVG_2 0x0024
#define DEBAYER_CHAN_AVG_3 0x0028

#define DEBAYER_PIX_FMT VIDEO_PIX_FMT_BGGR8

struct debayer_config {
	uintptr_t base;
	const struct device *source_dev;
};

/* Used to tune the video format caps from the source at runtime */
static struct video_format_cap fmts[10];

static int debayer_get_caps(const struct device *dev, enum video_endpoint_id ep,
			    struct video_caps *caps)
{
	const struct debayer_config *cfg = dev->config;
	int ret;

	ret = video_get_caps(cfg->source_dev, ep, caps);
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

static int debayer_set_format(const struct device *dev, enum video_endpoint_id ep,
			      struct video_format *fmt)
{
	const struct debayer_config *cfg = dev->config;
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
	source_fmt.pixelformat = DEBAYER_PIX_FMT;

	LOG_DBG("setting %s to %ux%u", source_dev->name, source_fmt.width, source_fmt.height);

	ret = video_set_format(source_dev, ep, &source_fmt);
	if (ret < 0) {
		LOG_ERR("failed to set %s format", source_dev->name);
		return ret;
	}

	return 0;
}

static int debayer_get_format(const struct device *dev, enum video_endpoint_id ep,
			      struct video_format *fmt)
{
	const struct debayer_config *cfg = dev->config;
	int ret;

	ret = video_get_format(cfg->source_dev, ep, fmt);
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

static int debayer_set_frmival(const struct device *dev, enum video_endpoint_id ep,
			       struct video_frmival *frmival)
{
	const struct debayer_config *cfg = dev->config;

	return video_set_frmival(cfg->source_dev, ep, frmival);
}

static int debayer_get_frmival(const struct device *dev, enum video_endpoint_id ep,
			       struct video_frmival *frmival)
{
	const struct debayer_config *cfg = dev->config;

	return video_get_frmival(cfg->source_dev, ep, frmival);
}

static int debayer_enum_frmival(const struct device *dev, enum video_endpoint_id ep,
				struct video_frmival_enum *fie)
{
	const struct debayer_config *cfg = dev->config;
	const struct video_format *prev_fmt = fie->format;
	struct video_format fmt = *fie->format;
	int ret;

	if (fie->format->pixelformat != VIDEO_PIX_FMT_YUYV) {
		LOG_ERR("Only YUYV is supported");
		return -ENOTSUP;
	}

	fmt.width += 2;
	fmt.height += 2;
	fmt.pixelformat = DEBAYER_PIX_FMT,

	fie->format = &fmt;
	ret = video_enum_frmival(cfg->source_dev, ep, fie);
	fie->format = prev_fmt;

	return ret;
}

#if 0
static int debayer_get_stats(const struct device *dev, enum video_endpoint_id ep,
			     uint16_t type_flags, struct video_stats *stats_in)
{
	const struct debayer_config *cfg = dev->config;
	uint8_t ch0 = sys_read32(cfg->base + DEBAYER_CHAN_AVG_0);
	uint8_t ch1 = sys_read32(cfg->base + DEBAYER_CHAN_AVG_1);
	uint8_t ch2 = sys_read32(cfg->base + DEBAYER_CHAN_AVG_2);
	uint8_t ch3 = sys_read32(cfg->base + DEBAYER_CHAN_AVG_3);
	struct video_channel_stats *stats = (void *)stats_in;
	struct video_format fmt;
	int ret;

	/* Unconditionally set the frame counter */
	stats->base.frame_counter = sys_read32(cfg->base + DEBAYER_NUM_FRAMES);

	if (type_flags == 0) {
		return 0;
	}

	if ((type_flags & VIDEO_STATS_ASK_CHANNELS) == 0) {
		return -ENOTSUP;
	}

	ret = video_get_format(cfg->source_dev, VIDEO_EP_OUT, &fmt);
	if (ret < 0) {
		return ret;
	}

	/* Depending on the order of the bayer pattern, the channels will be positioned at a
	 * different location on the image sensor. i.e. for RGGB, CH0=R, CH1=G , CH2=G, CH3=B.
	 * [CH0, CH1, CH0, CH1, CH0, CH1, CH0, CH1, CH0, CH1...],
	 * [CH2, CH3, CH2, CH3, CH2, CH3, CH2, CH3, CH2, CH3...],
	 * [...]
	 */
	switch (fmt.pixelformat) {
	case VIDEO_PIX_FMT_BGGR8:
		stats->ch1 = ch3; /* red */
		stats->ch2 = (ch1 + ch2) / 2; /* green */
		stats->ch3 = ch0; /* blue */
		break;
	case VIDEO_PIX_FMT_GBRG8:
		stats->ch1 = ch2; /* red */
		stats->ch2 = (ch0 + ch3) / 2; /* green */
		stats->ch3 = ch1; /* blue */
		break;
	case VIDEO_PIX_FMT_GRBG8:
		stats->ch1 = ch1; /* red */
		stats->ch2 = (ch0 + ch3) / 2; /* green */
		stats->ch3 = ch2; /* blue */
		break;
	case VIDEO_PIX_FMT_RGGB8:
		stats->ch1 = ch0; /* red */
		stats->ch2 = (ch1 + ch2) / 2; /* green */
		stats->ch3 = ch3; /* blue */
		break;
	default:
		LOG_WRN("Unknown input pixel format, cannot decode the statistics");
	}

	/* Hardware bug */
	stats->ch1 = stats->ch1 == 255 ? 0 : stats->ch1;
	stats->ch2 = stats->ch2 == 255 ? 0 : stats->ch2;
	stats->ch3 = stats->ch3 == 255 ? 0 : stats->ch3;

	stats->base.type_flags = VIDEO_STATS_CHANNELS_RGB;

	return 0;
}
#endif

static int debayer_get_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct debayer_config *cfg = dev->config;

	return video_get_ctrl(cfg->source_dev, cid, value);
}

static int debayer_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct debayer_config *cfg = dev->config;

	return video_set_ctrl(cfg->source_dev, cid, value);
}

static int debayer_set_stream(const struct device *dev, bool on)
{
	const struct debayer_config *cfg = dev->config;

	return on ? video_stream_start(cfg->source_dev) : video_stream_stop(cfg->source_dev);
}

static const DEVICE_API(video, debayer_driver_api) = {
	.set_format = debayer_set_format,
	.get_format = debayer_get_format,
	.get_caps = debayer_get_caps,
	.set_frmival = debayer_set_frmival,
	.get_frmival = debayer_get_frmival,
	.enum_frmival = debayer_enum_frmival,
#if 0
	.get_stats = debayer_get_stats,
#endif
	.set_stream = debayer_set_stream,
	.set_ctrl = debayer_set_ctrl,
	.get_ctrl = debayer_get_ctrl,
};

#define DEBAYER_INIT(n)                                                                            \
	const static struct debayer_config debayer_cfg_##n = {                                     \
		.source_dev =                                                                      \
			DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(DT_INST_ENDPOINT_BY_ID(n, 0, 0))),     \
		.base = DT_INST_REG_ADDR(n),                                                       \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &debayer_cfg_##n, POST_KERNEL,                  \
			      CONFIG_VIDEO_INIT_PRIORITY, &debayer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DEBAYER_INIT)

static bool device_is_video_and_ready(const struct device *dev)
{
	return device_is_ready(dev) && DEVICE_API_IS(video, dev);
}

static int cmd_debayer_show(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	const struct debayer_config *cfg;

	__ASSERT_NO_MSG(argc == 2);

	dev = device_get_binding(argv[1]);
	if (dev == NULL || !device_is_video_and_ready(dev)) {
		shell_error(sh, "could not find a video device ready with that name");
		return -ENODEV;
	}

	cfg = dev->config;

	shell_print(sh, "num frames: %u", sys_read32(cfg->base + DEBAYER_NUM_FRAMES));
	shell_print(sh, "chan 0 average: 0x%02x", sys_read32(cfg->base + DEBAYER_CHAN_AVG_0));
	shell_print(sh, "chan 1 average: 0x%02x", sys_read32(cfg->base + DEBAYER_CHAN_AVG_1));
	shell_print(sh, "chan 2 average: 0x%02x", sys_read32(cfg->base + DEBAYER_CHAN_AVG_2));
	shell_print(sh, "chan 3 average: 0x%02x", sys_read32(cfg->base + DEBAYER_CHAN_AVG_3));

	return 0;
}

static void device_name_get(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_filter(idx, device_is_video_and_ready);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}
SHELL_DYNAMIC_CMD_CREATE(dsub_device_name, device_name_get);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_debayer,

	SHELL_CMD_ARG(show, &dsub_device_name, "Show a dump of the hardware registers",
		      cmd_debayer_show, 2, 0),

	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(debayer, &sub_debayer, "tinyVision.ai debayer and statistics", NULL);
