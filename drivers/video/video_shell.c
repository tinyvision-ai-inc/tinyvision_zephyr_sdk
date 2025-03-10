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

static struct video_buffer *vbuf;
static struct video_format *vfmt;

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

static int video_shell_parse_fourcc(const struct shell *sh, char *fourcc)
{
	uint32_t pixfmt;

	if (strlen(fourcc) != 4) {
		shell_error(sh, "invalid <fourcc> parameter %s, expected 4 characters", fourcc);
		return 0;
	}

	pixfmt = VIDEO_FOURCC(fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
	if (video_bits_per_pixel(pixfmt) == 0) {
		shell_error(sh, "invalid <fourcc> parameter %s, format not supported", fourcc);
		shell_print(sh, "see <zephyr/drivers/video.h> for a list valid formats");
		shell_print(sh, "compressed formats like JPEG are not currently supported");
		return 0;
	}

	return pixfmt;
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
	shell_print(sh, "current format: %c%c%c%c %ux%u (%u bytes)",
		    fmt.pixelformat >> 0 & 0xff, fmt.pixelformat >> 8 & 0xff,
		    fmt.pixelformat >> 16 & 0xff, fmt.pixelformat >> 24 & 0xff,
		    fmt.width, fmt.height, fmt.pitch * fmt.height);

	for (int i = 0; caps.format_caps[i].pixelformat != 0; i++) {
		const struct video_format_cap *cap = &caps.format_caps[i];
		uint32_t px = cap->pixelformat;
		size_t size = cap->width_max * cap->height_max *
			video_bits_per_pixel(px) / BITS_PER_BYTE;

		if (cap->width_min != cap->width_max || cap->height_min != cap->height_max) {
			shell_print(sh, "pixel format %c%c%c%c, min %ux%u, max %ux%u, step %ux%u"
				    " (%u bytes max)",
				    px & 0xff, px >> 8 & 0xff, px >> 16 & 0xff, px >> 24 & 0xff,
				    cap->width_min, cap->height_min,
				    cap->height_max, cap->width_max,
				    cap->width_step, cap->height_step, size);
			video_shell_show_frmival(sh, dev, px, cap->width_min, cap->height_min);
			video_shell_show_frmival(sh, dev, px, cap->width_max, cap->height_max);
		} else {
			shell_print(sh, "pixel format %c%c%c%c %ux%u (%u bytes)",
				    px & 0xff, px >> 8 & 0xff, px >> 16 & 0xff, px >> 24 & 0xff,
				    cap->width_max, cap->height_max, size);
			video_shell_show_frmival(sh, dev, px, cap->width_min, cap->height_min);
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

static int cmd_video_format(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = device_get_binding(argv[1]);
	struct video_format fmt = {0};
	char *endptr = NULL;
	int ret;

	ret = video_shell_check_device(sh, dev);
	if (ret < 0) {
		return ret;
	}

	fmt.pixelformat = video_shell_parse_fourcc(sh, argv[2]);
	if (ret < 0) {
		return ret;
	}

	fmt.width = strtoul(argv[3], &endptr, 10);
	if (*endptr++ != 'x') {
		shell_error(sh, "invalid <width>x<height> parameter %s", argv[3]);
		return -EINVAL;
	}

	fmt.height = strtoul(endptr, &endptr, 10);
	if (*endptr != '\0') {
		shell_error(sh, "invalid <width>x<height> parameter %s", argv[3]);
		return -EINVAL;
	}

	fmt.pitch = fmt.width * video_bits_per_pixel(fmt.pixelformat) / BITS_PER_BYTE;

	shell_print(sh, "setting %s format to %c%c%c%c %ux%u (%u bytes)", dev->name,
		    fmt.pixelformat >> 0 & 0xff, fmt.pixelformat >> 8 & 0xff,
		    fmt.pixelformat >> 16 & 0xff, fmt.pixelformat >> 24 & 0xff,
		    fmt.width, fmt.height, fmt.pitch * fmt.height);

	ret = video_set_format(dev, VIDEO_EP_ALL, &fmt);
	if (ret) {
		shell_error(sh, "failed to set %s format (%u bytes)",
			    dev->name, fmt.pitch * fmt.width);
		return ret;
	}
	return 0;
}

static int cmd_video_framerate(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = device_get_binding(argv[1]);
	struct video_frmival frmival = {0};
	char *endptr = NULL;
	int ret;

	ret = video_shell_check_device(sh, dev);
	if (ret < 0) {
		return ret;
	}

	frmival.denominator = strtoul(argv[2], &endptr, 10);
	if (*endptr != '\0' || frmival.denominator == 0) {
		shell_error(sh, "invalid <fps> parameter %s", argv[2]);
		return -EINVAL;
	}

	shell_print(sh, "setting %s frame rate to %u FPS", dev->name, frmival.denominator);

	ret = video_set_frmival(dev, VIDEO_EP_ALL, &frmival);
	if (ret) {
		shell_error(sh, "failed to set %s frame rate", dev->name);
		return ret;
	}

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

static int cmd_video_alloc(const struct shell *sh, size_t argc, char **argv)
{
	char *endptr = NULL;
	size_t size;

	if (argc <= 1) {
		if (vbuf == NULL) {
			shell_print(sh, "no video buffer allocated");
		} else {
			shell_print(sh, "buffer %p filled with %u bytes out of %u bytes",
				    vbuf, vbuf->bytesused, vbuf->size);
		}
		return 0;
	}

	if (vbuf != NULL) {
		video_buffer_release(vbuf);
	}

	size = strtoul(argv[1], &endptr, 10);
	if (*endptr != '\0') {
		shell_error(sh, "invalid <size> parameter %s", argv[1]);
		return -EINVAL;
	}

	if (size == 0) {
		shell_print(sh, "size is zero, releasing the video buffer");
		vbuf = NULL;
		return 0;
	}

	vbuf = video_buffer_alloc(size, K_NO_WAIT);
	if (vbuf == NULL) {
		shell_error(sh, "failed to allocate a video buffer of %u bytes", size);
		return -ENOMEM;
	}

	shell_print(sh, "video buffer %p ready with %u bytes", vbuf, vbuf->size);
	return 0;
}

static int cmd_video_enqueue(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = device_get_binding(argv[1]);
	struct video_buffer *unused;
	int ret;

	ret = video_shell_check_device(sh, dev);
	if (ret < 0) {
		return ret;
	}

	if (vbuf == NULL) {
		shell_error(sh, "buffer not allocated yet, use the 'alloc' command first");
		return -ENOBUFS;
	}

	ret = video_enqueue(dev, VIDEO_EP_ALL, vbuf);
	if (ret < 0) {
		shell_error(sh, "enqueueing buffer to %s failed: %s", dev->name, strerror(-ret));
		return ret;
	}

	ret = video_dequeue(dev, VIDEO_EP_ALL, &unused, K_SECONDS(5));
	if (ret == -EAGAIN) {
		shell_error(sh, "dequeueing buffer from %s timed out after 5 seconds", dev->name);
		return ret;
	}
	if (ret < 0) {
		shell_error(sh, "dequeueing buffer from %s failed", dev->name);
		return ret;
	}
	if (unused != vbuf) {
		shell_warn(sh, "buffer enqueued (%p) and dequeued (%p) do not match",
			   vbuf, unused);
	}
	if (vbuf->bytesused > vbuf->size) {
		shell_warn(sh, "buffer filled (%u) beyond its size (%u)",
			   vbuf->bytesused, vbuf->size);
	}

	shell_print(sh, "sent video buffer of size %u to %s, came back with %u bytes",
		    vbuf->size, dev->name, vbuf->bytesused);

	return 0;
}

static int cmd_video_dump(const struct shell *sh, size_t argc, char **argv)
{
	size_t offset = 0;
	char *endptr;
	size_t size = 0;

	if (vbuf == NULL) {
		shell_error(sh, "buffer not allocated yet, use the 'alloc' command first");
		return -ENOBUFS;
	}

	for (size_t argidx = 1; argidx < argc; argidx++) {
		if (argidx + 1 >= argc) {
			shell_error(sh, "missing argument after command flag");
			return -EINVAL;
		} else if (strcmp(argv[argidx], "-s") == 0) {
			argidx++;
			size = strtoul(argv[argidx], &endptr, 10);
			if (*endptr != '\0' || size == 0) {
				shell_error(sh, "invalid <width> parameter %s", argv[argidx]);
				return -EINVAL;
			}
		} else if (strcmp(argv[argidx], "-o") == 0) {
			argidx++;
			offset = strtoul(argv[argidx], &endptr, 10);
			if (*endptr != '\0') {
				shell_error(sh, "invalid <offset> parameter %s", argv[argidx]);
				return -EINVAL;
			}
		} else {
			shell_error(sh, "unknown argument %s", argv[argidx]);
			return -EINVAL;
		}
	}

	if (offset > vbuf->bytesused) {
		shell_error(sh, "<offset> (%u) is beyond the end of the buffer filled region (%u)",
			    offset, vbuf->bytesused);
		return -E2BIG;
	}

	if (size == 0) {
		size = vbuf->bytesused - offset;
	}

	if (offset + size > vbuf->bytesused) {
		shell_error(sh, "<offset> + <size> (%u) is beyond the filled buffer region (%u)",
			    offset + size, vbuf->bytesused);
		return -E2BIG;
	}

	shell_hexdump(sh, &vbuf->buffer[offset], size);
	return 0;
}

static inline uint8_t video_rgb888_to_r8(uint32_t rgb888)
{
	return rgb888 >> 16;
}

static inline uint8_t video_rgb888_to_g8(uint32_t rgb888)
{
	return rgb888 >> 8;
}

static inline uint8_t video_rgb888_to_b8(uint32_t rgb888)
{
	return rgb888 >> 0;
}

static inline uint8_t video_rgb565_to_r8(uint16_t rgb565)
{
	return rgb565 >> (0 + 6 + 5) << (8 - 5) & 0xff;
}

static inline uint8_t video_rgb565_to_g8(uint16_t rgb565)
{
	return rgb565 >> (0 + 0 + 5) << (8 - 6) & 0xff;
}

static inline uint8_t video_rgb565_to_b8(uint16_t rgb565)
{
	return rgb565 >> (0 + 0 + 0) << (8 - 5) & 0xff;
}

/* Conversion using BT.709 profile as defined in: https://en.wikipedia.org/wiki/YCbCr */

static inline uint8_t video_yuv422_to_r8_bt709(uint8_t y, uint8_t u, uint8_t v)
{
	int32_t y21 = (int32_t)(+1.0000 * (1 << 21)) * y;
	int32_t u21 = (int32_t)(+0.0000 * (1 << 21)) * ((int32_t)u - 128);
	int32_t v21 = (int32_t)(+1.5748 * (1 << 21)) * ((int32_t)v - 128);
	int16_t r16 = (y21 + u21 + v21) >> 21;

	return CLAMP(r16, 0, 0xff);
}

static inline uint8_t video_yuv422_to_g8_bt709(uint8_t y, uint8_t u, uint8_t v)
{
	int32_t y21 = (int32_t)(+1.0000 * (1 << 21)) * y;
	int32_t u21 = (int32_t)(-0.1873 * (1 << 21)) * ((int32_t)u - 128);
	int32_t v21 = (int32_t)(-0.4681 * (1 << 21)) * ((int32_t)v - 128);
	int16_t g16 = (y21 + u21 + v21) >> 21;

	return CLAMP(g16, 0, 0xff);
}

static inline uint8_t video_yuv422_to_b8_bt709(uint8_t y, uint8_t u, uint8_t v)
{
	int32_t y21 = (int32_t)(+1.0000 * (1 << 21)) * y;
	int32_t u21 = (int32_t)(+1.8556 * (1 << 21)) * ((int32_t)u - 128);
	int32_t v21 = (int32_t)(+0.0000 * (1 << 21)) * ((int32_t)v - 128);
	int16_t b16 = (y21 + u21 + v21) >> 21;

	return CLAMP(b16, 0, 0xff);
}

static inline uint32_t video_rgb565le_to_rgb888(uint16_t rgb565le)
{
	uint16_t rgb565 = sys_le16_to_cpu(rgb565le);
	uint32_t rgb888 = 0;

	rgb888 |= video_rgb565_to_r8(rgb565) << 16;
	rgb888 |= video_rgb565_to_g8(rgb565) << 8;
	rgb888 |= video_rgb565_to_b8(rgb565) << 0;
	return rgb888;
}

static inline uint64_t video_yuyv422_to_rgb888x2(uint32_t yuyv422)
{
	uint8_t y0 = yuyv422 >> 24;
	int8_t uu = yuyv422 >> 16;
	int8_t y1 = yuyv422 >> 8;
	uint8_t vv = yuyv422 >> 0;
	uint32_t rgb888n0 = 0;
	uint32_t rgb888n1 = 0;

	rgb888n0 |= video_yuv422_to_r8_bt709(y0, uu, vv) << 16;
	rgb888n0 |= video_yuv422_to_g8_bt709(y0, uu, vv) << 8;
	rgb888n0 |= video_yuv422_to_b8_bt709(y0, uu, vv) << 0;
	rgb888n1 |= video_yuv422_to_r8_bt709(y1, uu, vv) << 16;
	rgb888n1 |= video_yuv422_to_g8_bt709(y1, uu, vv) << 8;
	rgb888n1 |= video_yuv422_to_b8_bt709(y1, uu, vv) << 0;
	return (uint64_t)rgb888n0 << 32 | rgb888n1 << 0;
}

/**
 * @brief Convert from base 256 (8-bit) to base 6 (VT100)
 *
 * @param rgb888 8-bit per channel red, green, blue in native (CPU) endianness.
 * @return Colors in ANSI escape code color format: values from 0 to 5 inclusive per channel.
 */
static inline uint8_t video_rgb888_to_vt100(uint32_t rgb888)
{
	uint8_t vt100 = 16;

	vt100 += video_rgb888_to_r8(rgb888) * 6 / 256 * 6 * 6;
	vt100 += video_rgb888_to_g8(rgb888) * 6 / 256 * 6;
	vt100 += video_rgb888_to_b8(rgb888) * 6 / 256;
	return vt100;
}

/**
 * @brief Step-by-step increment two indexes preserving their proportion
 *
 * This increments two indexes trying to preserve proportions between them:
 * @p i0 is always incremented, breaking the proportions between the two.
 * The other index, @p i1 is therefore incremented as many time as needed
 * to restore the proportion between the two indexes, and stops once reached.
 * The proportions are given as a ratio of @p len0 and @p len1.
 *
 * @param i0   First index always incremented.
 * @param len0 Length associated with i0, proprtion in i0 direction.
 * @param i1   Second index incremented zero or more times to preserve the proportions with @p i0.
 * @param len1 Length associated with i1, proprtion in i1 direction.
 * @param debt Accumulator telling how much overshooting was done in either direction.
 *             It must be initialized to zero on first call.
 */
static inline void video_skip_to_next(size_t *i0, size_t len0, size_t *i1, size_t len1,
				      size_t *debt)
{
	/* Move toward one direction, creating a small debt */
	*i0 += 1;
	*debt += len1;

	__ASSERT_NO_MSG(len0 > 0);

	/* Catch-up any debt in the other direction */
	while (*debt >= len0) {
		*i1 += 1;
		*debt -= len0;
	}
}

static void video_line_yuyv422be_to_vt100(uint16_t *yuyv422be_buf, size_t yuyv422be_len,
					  uint8_t *vt100_buf, size_t vt100_len)
{
	size_t yuyv422be_i = 0, vt100_i = 0, debt = 0;
	uint64_t rgb888x2;

	while (yuyv422be_i < yuyv422be_len && vt100_i < vt100_len) {
		/* YUYV is having 16-bit per pixel, but odd pixels and even pixels encode different
		 * color information: We align on a 2-pixels block and grab 2 values at once.
		 */
		uint32_t yuyv422x2 = sys_be32_to_cpu(((uint32_t *)yuyv422be_buf)[yuyv422be_i / 2]);

		rgb888x2 = video_yuyv422_to_rgb888x2(yuyv422x2);
		vt100_buf[vt100_i] = video_rgb888_to_vt100(rgb888x2);
		video_skip_to_next(&vt100_i, vt100_len, &yuyv422be_i, yuyv422be_len, &debt);
	}
}

static void video_line_rgb565le_to_vt100(uint16_t *rgb565le_buf, size_t rgb565le_len,
					 uint8_t *vt100_buf, size_t vt100_len)
{
	size_t rgb565le_i = 0, vt100_i = 0, debt = 0;
	uint32_t rgb888;

	while (rgb565le_i < rgb565le_len && vt100_i < vt100_len) {
		rgb888 = video_rgb565le_to_rgb888(rgb565le_buf[rgb565le_i]);
		vt100_buf[vt100_i] = video_rgb888_to_vt100(rgb888);
		video_skip_to_next(&vt100_i, vt100_len, &rgb565le_i, rgb565le_len, &debt);
	}
}

static void video_line_to_vt100(uint32_t pixfmt, void *pixp, uint16_t width,
				uint8_t *line_out, uint16_t term_cols)
{
	switch (pixfmt) {
	case VIDEO_PIX_FMT_YUYV:
		video_line_yuyv422be_to_vt100(pixp, width, line_out, term_cols);
		break;
	case VIDEO_PIX_FMT_RGB565:
		video_line_rgb565le_to_vt100(pixp, width, line_out, term_cols);
		break;
	default:
		CODE_UNREACHABLE;
	}
}

static int video_shell_view(const struct shell *sh, uint8_t *buf, size_t size, uint32_t pixfmt,
			     uint16_t width, uint16_t cols)
{
	uint8_t line[2][256];
	uint16_t pitch = width * video_bits_per_pixel(pixfmt) / BITS_PER_BYTE;
	size_t height = height = size / pitch;
	size_t rows = cols * height / width;
	size_t debt = 0;

	if (cols > sizeof(line[0])) {
		shell_error(sh, "parameter <columns> (%u) larger than line buffer %u",
			    cols, sizeof(line[0]));
		return -EINVAL;
	}

	if (pixfmt != VIDEO_PIX_FMT_RGB565 && pixfmt != VIDEO_PIX_FMT_YUYV) {
		shell_error(sh, "only <fourcc> 'RGBP' and 'YUYV' are supported for now");
		return -ENOTSUP;
	}

	for (size_t h = 0, r = 0, i = 0; i < size; i += pitch * h) {
		/* Convert this entire row */
		video_line_to_vt100(pixfmt, &vbuf->buffer[i], width, line[r % 2], cols);

		/* Skip the bytes instead of scaling: poor quality but faster */
		video_skip_to_next(&r, rows, &h, height, &debt);

		/* Once every two rows, display the result */
		if (r % 2 == 0) {
			for (size_t c = 0; c < cols; c++) {
				shell_fprintf_normal(sh, "\e[48;5;%u;38;5;%um▄\e[m", line[0][c], line[1][c]);
			}
			shell_fprintf_normal(sh, ":\n");
		}
	}

	return 0;
}

static int cmd_video_view(const struct shell *sh, size_t argc, char **argv)
{
	size_t pixfmt = 0;
	size_t width = 0;
	size_t offset = 0;
	size_t cols = 100;
	size_t size;
	char *endptr;

	if (vbuf == NULL) {
		shell_error(sh, "buffer not allocated yet, use the 'alloc' command first");
		return -ENOBUFS;
	}

	size = vbuf->bytesused;

	pixfmt = video_shell_parse_fourcc(sh, argv[1]);
	if (pixfmt == 0 || video_bits_per_pixel(pixfmt) == 0) {
		shell_error(sh, "unsupported <fourcc> %s", argv[1]);
		return -EINVAL;
	}

	width = strtoul(argv[2], &endptr, 10);
	if (*endptr != '\0' || width == 0) {
		shell_error(sh, "invalid <width> parameter %s", argv[2]);
		return -EINVAL;
	}

	for (size_t argidx = 3; argidx < argc; argidx++) {
		if (argidx + 1 >= argc) {
			shell_error(sh, "missing argument after command flag");
			return -EINVAL;
		} else if (strcmp(argv[argidx], "-s") == 0) {
			argidx++;
			size = strtoul(argv[argidx], &endptr, 10);
			if (*endptr != '\0' || size == 0) {
				shell_error(sh, "invalid <size> parameter %s", argv[argidx]);
				return -EINVAL;
			}
		} else if (strcmp(argv[argidx], "-o") == 0) {
			argidx++;
			offset = strtoul(argv[argidx], &endptr, 10);
			if (*endptr != '\0') {
				shell_error(sh, "invalid <offset> parameter %s", argv[argidx]);
				return -EINVAL;
			}
		} else if (strcmp(argv[argidx], "-c") == 0) {
			argidx++;
			cols = strtoul(argv[argidx], &endptr, 10);
			if (*endptr != '\0') {
				shell_error(sh, "invalid <width> parameter %s", argv[argidx]);
				return -EINVAL;
			}
		} else {
			shell_error(sh, "unknown argument %s", argv[argidx]);
			return -EINVAL;
		}
	}

	if (offset > vbuf->bytesused) {
		shell_error(sh, "offset (%u) larger than data (%u)", offset, vbuf->bytesused);
		return -EINVAL;
	}

	return video_shell_view(sh, &vbuf->buffer[offset], size - offset, pixfmt, width, cols);
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
	SHELL_CMD_ARG(start, &dsub_video_device,
		"Start a video device and its sources\n"
		"Usage: video start <device>",
		cmd_video_start, 2, 0),
	SHELL_CMD_ARG(stop, &dsub_video_device,
		"Stop a video device and its sources\n"
		"Usage: video stop <device>",
		cmd_video_stop, 2, 0),
	SHELL_CMD_ARG(show, &dsub_video_device,
		"Show video device information\n"
		"Usage: video show <device>",
		cmd_video_show, 2, 0),
	SHELL_CMD_ARG(format, &dsub_video_device,
		"Set the active format of a video device\n"
		"Usage: video format <device> <fourcc> <width>x<height>",
		cmd_video_format, 4, 0),
	SHELL_CMD_ARG(framerate, &dsub_video_device,
		"Set the frame rate of a video device\n"
		"Usage: video framerate <device> <fps>",
		cmd_video_framerate, 3, 0),
	SHELL_CMD_ARG(ctrl, &dsub_video_device_ctrl,
		"Send an integer-based control to a video device\n"
		"Usage: video ctrl <device> <cid> <value>",
		cmd_video_ctrl, 4, 0),
	SHELL_CMD_ARG(alloc, NULL,
		"Allocate a buffer to enqueue to video devices, or show the current buffer size\n"
		"Set to 0 to free the buffer completely, ommit argument to show the size\n"
		"Usage: video alloc [<size>]",
		cmd_video_alloc, 1, 1),
	SHELL_CMD_ARG(enqueue, &dsub_video_device,
		"Send a video buffer to a video device\n"
		"Usage: video enqueue <device>",
		cmd_video_enqueue, 2, 0),
	SHELL_CMD_ARG(dump, NULL,
		"Dump the given size (or everythiing) of the buffer at given offset (or zero)\n"
		"Usage: video dump [-o <offset>] [-s <size>]",
		cmd_video_dump, 1, 3),
	SHELL_CMD_ARG(view, NULL,
		"Display the video buffer content on the terminal\n"
		"Usage: video view <fourcc> <width> [-o <offset>] [-s <size>] [-c <columns>]",
		cmd_video_view, 3, 6),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(video, &sub_video_cmds, "Video driver commands", NULL);
