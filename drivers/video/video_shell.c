/*
 * Copyright (c) 2024 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/shell/shell.h>

#define FPS(frmival) DIV_ROUND_CLOSEST((frmival)->denominator, (frmival)->numerator)
#define VIDEO_SHELL_CTRL_NAME_DEFINE(_name) {.name = #_name, .cid = VIDEO_CID_##_name,}

struct ctrl_name {
	uint32_t cid;
	char *name;
};

struct ctrl_name ctrl_names[] = {
	{VIDEO_CID_BRIGHTNESS,                "brightness"},
	{VIDEO_CID_CONTRAST,                  "contrast"},
	{VIDEO_CID_EXPOSURE,                  "exposure"},
	{VIDEO_CID_GAIN,                      "gain"},
	{VIDEO_CID_HFLIP,                     "hflip"},
	{VIDEO_CID_HUE,                       "hue"},
	{VIDEO_CID_JPEG_COMPRESSION_QUALITY,  "jpeg_compression_quality"},
	{VIDEO_CID_PIXEL_RATE,                "pixel_rate"},
	{VIDEO_CID_POWER_LINE_FREQUENCY,      "power_line_frequency"},
	{VIDEO_CID_SATURATION,                "saturation"},
	{VIDEO_CID_TEST_PATTERN,              "test_pattern"},
	{VIDEO_CID_VFLIP,                     "vflip"},
	{VIDEO_CID_WHITE_BALANCE_TEMPERATURE, "white_balance_temperature"},
	{VIDEO_CID_ZOOM_ABSOLUTE,             "zoom_absolute"},
};

static int video_shell_check_device(const struct shell *sh, const struct device *dev)
{
	if (dev == NULL) {
		shell_error(sh, "could not find a video device with that name");
		return -ENODEV;
	}

	if (!DEVICE_API_IS(video, dev)) {
		shell_error(sh, "%s is not a video device", dev->name);
		return -EINVAL;
	}

	if (!device_is_ready(dev)) {
		shell_error(sh, "device %s not ready", dev->name);
		return -ENODEV;
	}

	return 0;
}

static int cmd_video_start(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = device_get_binding(argv[1]);
	int ret;

	ret = video_shell_check_device(sh, dev);
	if (ret < 0) {
		return ret;
	}

	ret = video_stream_start(dev);
	if (ret < 0) {
		shell_error(sh, "failed to start %s", dev->name);
		return ret;
	}

	shell_print(sh, "started %s video stream", dev->name);
	return 0;
}

static int cmd_video_stop(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = device_get_binding(argv[1]);
	int ret;

	ret = video_shell_check_device(sh, dev);
	if (ret < 0) {
		return ret;
	}

	ret = video_stream_stop(dev);
	if (ret < 0) {
		shell_error(sh, "failed to stop %s", dev->name);
		return ret;
	}

	shell_print(sh, "stopped %s video stream", dev->name);
	return 0;
}

static void video_shell_show_discrete(const struct shell *sh, struct video_frmival *discrete)
{
	shell_print(sh, "- %u/%u sec (%u FPS)",
		    discrete->numerator, discrete->denominator, FPS(discrete));
}

static void video_shell_show_stepwise(const struct shell *sh,
				      struct video_frmival_stepwise *stepwise)
{
	shell_print(sh, "- min %u/%u sec (%u FPS), max %u/%u (%u FPS), step %u/%u sec (%u FPS)",
		    stepwise->min.numerator, stepwise->min.denominator, FPS(&stepwise->min),
		    stepwise->max.numerator, stepwise->max.denominator, FPS(&stepwise->max),
		    stepwise->step.numerator, stepwise->step.denominator, FPS(&stepwise->step));
}

static void video_shell_show_frmival(const struct shell *sh, const struct device *dev,
				     uint32_t pixelformat, uint32_t width, uint32_t height)
{
	struct video_format fmt = {.pixelformat = pixelformat, .width = width, .height = height};
	struct video_frmival_enum fie = {.format = &fmt};

	for (fie.index = 0; video_enum_frmival(dev, VIDEO_EP_ALL, &fie) == 0; fie.index++) {
		switch (fie.type) {
		case VIDEO_FRMIVAL_TYPE_DISCRETE:
			video_shell_show_discrete(sh, &fie.discrete);
			break;
		case VIDEO_FRMIVAL_TYPE_STEPWISE:
			video_shell_show_stepwise(sh, &fie.stepwise);
			break;
		default:
			shell_error(sh, "- invalid frame interval type reported: 0x%x", fie.type);
			break;
		}
	}
	if (fie.index == 0) {
		shell_error(sh, "error while listing frame intervals");
	}
}

static int cmd_video_show(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = device_get_binding(argv[1]);
	struct video_caps caps = {0};
	struct video_format fmt = {0};
#if 0
	struct video_channel_stats stats = {0};
#endif
	int ret;

	ret = video_shell_check_device(sh, dev);
	if (ret < 0) {
		return ret;
	}

	ret = video_get_caps(dev, VIDEO_EP_ALL, &caps);
	if (ret < 0) {
		shell_error(sh, "Failed to query %s capabilities", dev->name);
		return -ENODEV;
	}

	ret = video_get_format(dev, VIDEO_EP_ALL, &fmt);
	if (ret < 0) {
		shell_error(sh, "Failed to query %s capabilities", dev->name);
		return -ENODEV;
	}

	shell_print(sh, "min vbuf count: %u", caps.min_vbuf_count);
	shell_print(sh, "min line count: %u", caps.min_line_count);
	shell_print(sh, "max line count: %u", caps.max_line_count);
	shell_print(sh, "current format: %s %ux%u (%u bytes)",
		    VIDEO_FOURCC_TO_STR(fmt.pixelformat), fmt.width, fmt.height,
		    fmt.pitch * fmt.height);

	for (int i = 0; caps.format_caps[i].pixelformat != 0; i++) {
		const struct video_format_cap *cap = &caps.format_caps[i];
		size_t size = cap->width_max * cap->height_max *
			video_bits_per_pixel(cap->pixelformat) / BITS_PER_BYTE;

		if (cap->width_min != cap->width_max || cap->height_min != cap->height_max) {
			shell_print(sh, "fourcc %s, min %ux%u, max %ux%u (%u bytes), step %ux%u",
				    VIDEO_FOURCC_TO_STR(cap->pixelformat),
				    cap->width_min, cap->height_min,
				    cap->height_max, cap->width_max, size,
				    cap->width_step, cap->height_step);
			video_shell_show_frmival(sh, dev, cap->pixelformat, cap->width_min,
						 cap->height_min);
			video_shell_show_frmival(sh, dev, cap->pixelformat, cap->width_max,
						 cap->height_max);
		} else {
			shell_print(sh, "pixel format %s %ux%u (%u bytes)",
				    VIDEO_FOURCC_TO_STR(cap->pixelformat),
				    cap->width_max, cap->height_max, size);
			video_shell_show_frmival(sh, dev, cap->pixelformat, cap->width_min,
						 cap->height_min);
		}
	}

#if 0
	ret = video_get_stats(dev, VIDEO_EP_OUT, VIDEO_STATS_ASK_CHANNELS, &stats.base);
	if (ret == 0) {
		shell_print(sh, "total frames: %d", stats.base.frame_counter);
	}
	if (stats.base.type_flags & VIDEO_STATS_CHANNELS_Y) {
		shell_print(sh, "- Channel Y: %d", stats.ch0);
	}
	if (stats.base.type_flags & VIDEO_STATS_CHANNELS_YUV) {
		shell_print(sh, "- Channel U: %d", stats.ch1);
		shell_print(sh, "- Channel V: %d", stats.ch2);
	}
	if (stats.base.type_flags & VIDEO_STATS_CHANNELS_RGB) {
		shell_print(sh, "- Channel R: %d", stats.ch1);
		shell_print(sh, "- Channel G: %d", stats.ch2);
		shell_print(sh, "- Channel B: %d", stats.ch3);
	}
#endif

	return 0;
}

static int cmd_video_ctrl(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = device_get_binding(argv[1]);
	char *endptr = NULL;
	uint32_t cid = 0;
	int ret;
	int value;

	ret = video_shell_check_device(sh, dev);
	if (ret < 0) {
		return ret;
	}

	for (int i = 0; i < ARRAY_SIZE(ctrl_names); i++) {
		if (strcmp(argv[2], ctrl_names[i].name) == 0) {
			cid = ctrl_names[i].cid;
			break;
		}
	}
	if (cid == 0) {
		shell_error(sh, "invalid <cid> parameter %s", argv[2]);
		return -EINVAL;
	}

	value = strtoul(argv[3], &endptr, 10);
	if (*endptr != '\0') {
		shell_error(sh, "invalid <size> parameter %s", argv[1]);
		return -EINVAL;
	}

	ret = video_set_ctrl(dev, cid, (void *)value);
	if (ret < 0) {
		shell_error(sh, "error while sending control 0x%08x to %s: %s",
		            cid, dev->name, strerror(-ret));
		return ret;
	}

	shell_print(sh, "control 0x%04x = %u", cid, value);
	return 0;
}

static bool device_is_video_and_ready(const struct device *dev)
{
	return device_is_ready(dev) && DEVICE_API_IS(video, dev);
}

static void complete_video_ctrl(size_t idx, struct shell_static_entry *entry)
{
	entry->syntax = (idx < ARRAY_SIZE(ctrl_names)) ? ctrl_names[idx].name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}
SHELL_DYNAMIC_CMD_CREATE(dsub_video_ctrl, complete_video_ctrl);

static void complete_video_device_ctrl(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_filter(idx, device_is_video_and_ready);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = &dsub_video_ctrl;
}
SHELL_DYNAMIC_CMD_CREATE(dsub_video_device_ctrl, complete_video_device_ctrl);

static void complete_video_device(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_filter(idx, device_is_video_and_ready);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}
SHELL_DYNAMIC_CMD_CREATE(dsub_video_device, complete_video_device);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_video_cmds,
	SHELL_CMD_ARG(show, &dsub_video_device,
		"Show video device information\n"
		"Usage: video show <device>",
		cmd_video_show, 2, 0),
	SHELL_CMD_ARG(start, &dsub_video_device,
		"Start a video device and its sources\n"
		"Usage: video start <device>",
		cmd_video_start, 2, 0),
	SHELL_CMD_ARG(stop, &dsub_video_device,
		"Stop a video device and its sources\n"
		"Usage: video stop <device>",
		cmd_video_stop, 2, 0),
	SHELL_CMD_ARG(ctrl, &dsub_video_device_ctrl,
		"Send an integer-based control to a video device\n"
		"Usage: video ctrl <device> <cid> <value>",
		cmd_video_ctrl, 4, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(video, &sub_video_cmds, "Video driver commands", NULL);
