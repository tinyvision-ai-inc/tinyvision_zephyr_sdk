/*
 * Copyright (c) 2024 tinyVision.ai Inc.
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
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(debayer, CONFIG_VIDEO_LOG_LEVEL);

struct debayer_data {
	struct video_format fmt;
};

struct debayer_config {
	const struct device *source_dev;
};

/* Used to tune the video format caps from the source at runtime */
static struct video_format_cap fmts[10];

static int debayer_get_caps(const struct device *dev, enum video_endpoint_id ep, struct video_caps *caps)
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

static int debayer_set_format(const struct device *dev, enum video_endpoint_id ep, struct video_format *fmt)
{
	const struct debayer_config *cfg = dev->config;
	const struct device *sdev = cfg->source_dev;
	struct debayer_data *data = dev->data;
	struct video_format sfmt = *fmt;
	int ret;

	if (fmt->pixelformat != VIDEO_PIX_FMT_YUYV) {
		LOG_ERR("Only YUYV is supported as output format");
		return -ENOTSUP;
	}

	/* Apply the conversion done by hardware to the format */
	sfmt.width += 2;
	sfmt.height += 2;
	sfmt.pixelformat = VIDEO_PIX_FMT_BGGR8;

	LOG_DBG("%s: setting %s to %ux%u", dev->name, sdev->name, sfmt.width, sfmt.height);
	ret = video_set_format(sdev, ep, &sfmt);
	if (ret < 0) {
		LOG_ERR("%s: failed to set %s format", dev->name, sdev->name);
		return ret;
	}

	data->fmt = *fmt;
	return 0;
}

static int debayer_get_format(const struct device *dev, enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	struct debayer_data *data = dev->data;
	*fmt = data->fmt;
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
	return video_enum_frmival(cfg->source_dev, ep, fie);
}

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

static int debayer_stream_start(const struct device *dev)
{
	const struct debayer_config *cfg = dev->config;
	return video_stream_start(cfg->source_dev);
}

static int debayer_stream_stop(const struct device *dev)
{
	const struct debayer_config *cfg = dev->config;
	return video_stream_stop(cfg->source_dev);
}

static const struct video_driver_api debayer_driver_api = {
	.set_format = debayer_set_format,
	.get_format = debayer_get_format,
	.get_caps = debayer_get_caps,
	.set_frmival = debayer_set_frmival,
	.get_frmival = debayer_get_frmival,
	.enum_frmival = debayer_enum_frmival,
	.stream_start = debayer_stream_start,
	.stream_stop = debayer_stream_stop,
	.set_ctrl = debayer_set_ctrl,
	.get_ctrl = debayer_get_ctrl,
};

static int debayer_init(const struct device *dev)
{
	struct video_caps caps;
	struct video_format fmt;
	int ret;

	debayer_get_caps(dev, VIDEO_EP_OUT, &caps);

	fmt.pixelformat = caps.format_caps[0].pixelformat;
	fmt.width = caps.format_caps[0].width_max;
	fmt.height = caps.format_caps[0].height_max;
	fmt.pitch = fmt.width * video_pix_fmt_bpp(fmt.pixelformat);

	ret = debayer_set_format(dev, VIDEO_EP_OUT, &fmt);
	if (ret) {
		LOG_ERR("Unable to configure default format");
		return -EIO;
	}

	return 0;
}

/* See #80649 */

/* Handle the variability of "ports{port@0{}};" vs "port{};" while going down */
#define DT_INST_PORT_BY_ID(inst, pid)                                                              \
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, ports)),                                    \
		    (DT_CHILD(DT_INST_CHILD(inst, ports), port_##pid)),                            \
		    (DT_INST_CHILD(inst, port)))

/* Handle the variability of "endpoint@0{};" vs "endpoint{};" while going down */
#define DT_INST_ENDPOINT_BY_ID(inst, pid, eid)                                                     \
	COND_CODE_1(DT_NODE_EXISTS(DT_CHILD(DT_INST_PORT_BY_ID(inst, pid), endpoint)),             \
		    (DT_CHILD(DT_INST_PORT_BY_ID(inst, pid), endpoint)),                           \
		    (DT_CHILD(DT_INST_PORT_BY_ID(inst, pid), endpoint_##eid)))

/* Handle the variability of "ports{port@0{}};" vs "port{};" while going up */
#define DT_ENDPOINT_PARENT_DEVICE(node)                                                            \
	COND_CODE_1(DT_NODE_EXISTS(DT_CHILD(DT_GPARENT(node), port)),                              \
		    (DT_GPARENT(node)), (DT_PARENT(DT_GPARENT(node))))

/* Handle the "remote-endpoint-label" */
#define DT_GET_REMOTE_DEVICE(node)                                                                 \
	DT_ENDPOINT_PARENT_DEVICE(DT_NODELABEL(DT_STRING_TOKEN(node, remote_endpoint_label)))

/* Handle the "remote-endpoint-label" */
#define DEVICE_DT_GET_REMOTE_DEVICE(node)                                                          \
	DEVICE_DT_GET(DT_GET_REMOTE_DEVICE(node))

#define DEBAYER_INIT(n)                                                                            \
	static struct debayer_data debayer_data_##n;                                               \
	const static struct debayer_config debayer_cfg_##n = {                                     \
		.source_dev = DEVICE_DT_GET_REMOTE_DEVICE(DT_INST_ENDPOINT_BY_ID(n, 0, 0)),        \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, &debayer_init, NULL, &debayer_data_##n, &debayer_cfg_##n,         \
			      POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY, &debayer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DEBAYER_INIT)
