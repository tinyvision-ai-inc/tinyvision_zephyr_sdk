/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include "video_imager.h"

LOG_MODULE_REGISTER(video_imager, CONFIG_VIDEO_LOG_LEVEL);

int video_imager_reg16_write8(struct i2c_dt_spec *i2c, uint16_t reg_addr, uint8_t reg_value)
{
	uint8_t buf[] = {reg_addr >> 8, reg_addr & 0xff, reg_value};

	return i2c_write_dt(i2c, buf, sizeof(buf));
}

int video_imager_reg16_write16(struct i2c_dt_spec *i2c, uint16_t reg_addr, uint16_t reg_value)
{
	uint8_t buf[] = {reg_addr >> 8, reg_addr & 0xff, reg_value >> 8, reg_value & 0xff};

	return i2c_write_dt(i2c, buf, sizeof(buf));
}

int video_imager_reg16_read8(struct i2c_dt_spec *i2c, uint16_t reg_addr, uint8_t *reg_value)
{
	uint8_t buf[] = {reg_addr >> 8, reg_addr & 0xff};

	return i2c_write_read_dt(i2c, buf, sizeof(buf), reg_value, sizeof(*reg_value));
}

int video_imager_reg16_read16(struct i2c_dt_spec *i2c, uint16_t reg_addr, uint16_t *reg_value)
{
	uint8_t buf[] = {reg_addr >> 8, reg_addr & 0xff};
	int ret;

	ret = i2c_write_read_dt(i2c, buf, sizeof(buf), reg_value, sizeof(*reg_value));
	*reg_value = sys_be16_to_cpu(*reg_value);
	return ret;
}

int video_imager_reg16_write8_multi(struct i2c_dt_spec *i2c, const void *regs_in)
{
	const struct video_imager_reg16 *regs = regs_in;
	int ret;

	for (int i = 0; regs[i].addr != 0; i++) {
		ret = video_imager_reg16_write8(i2c, regs[i].addr, regs[i].value);
		if (ret != 0) {
			LOG_ERR("Failed to write 0x%04x to register 0x%02x",
				regs[i].value, regs[i].addr);
			return ret;
		}
	}

	return 0;
}

int video_imager_set_mode(const struct device *dev, const struct video_imager_mode *mode)
{
	struct video_imager_data *data = dev->data;
	struct i2c_dt_spec *i2c = data->i2c;
	int ret;

	if (data->mode == mode) {
		LOG_DBG("%s is arlready in the mode requested", dev->name);
		return 0;
	}

	__ASSERT_NO_MSG(data->write_multi_fn != NULL);

	LOG_DBG("Applying %s mode %p at %d FPS", dev->name, mode, mode->fps);

	for (int i = 0; mode->regs[i] != NULL && i < ARRAY_SIZE(mode->regs); i++) {
		ret = data->write_multi_fn(i2c, mode->regs[i]);
		if (ret != 0) {
			goto err;
		}
	}

	data->mode = mode;

	return 0;
err:
	LOG_ERR("Could not apply %s mode %p (%u FPS)", dev->name, mode, mode->fps);

	return ret;
}

int video_imager_set_frmival(const struct device *dev, enum video_endpoint_id ep,
			     struct video_frmival *frmival)
{
	struct video_imager_data *data = dev->data;
	struct video_frmival_enum fie = {.format = &data->fmt, .discrete = *frmival};

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	video_closest_frmival(dev, ep, &fie);

	return video_imager_set_mode(dev, &data->modes[data->fmt_id][fie.index]);
}

int video_imager_get_frmival(const struct device *dev, enum video_endpoint_id ep,
			     struct video_frmival *frmival)
{
	struct video_imager_data *data = dev->data;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	frmival->numerator = 1;
	frmival->denominator = data->mode->fps;

	return 0;
}

int video_imager_enum_frmival(const struct device *dev, enum video_endpoint_id ep,
			      struct video_frmival_enum *fie)
{
	struct video_imager_data *data = dev->data;
	const struct video_imager_mode *modes;
	size_t fmt_id = 0;
	int ret;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	ret = video_format_caps_index(data->fmts, fie->format, &fmt_id);
	if (ret != 0) {
		LOG_ERR("Format '%c%c%c%c' %ux%u not found for %s",
			(fie->format->pixelformat >> 24) & 0xff,
			(fie->format->pixelformat >> 16) & 0xff,
			(fie->format->pixelformat >> 8) & 0xff,
			(fie->format->pixelformat >> 0) & 0xff,
			fie->format->width, fie->format->height, dev->name);
		return ret;
	}

	modes = data->modes[fmt_id];

	for (int i = 0;; i++) {
		if (modes[i].fps == 0) {
			return -EINVAL;
		}

		if (i == fie->index) {
			fie->type = VIDEO_FRMIVAL_TYPE_DISCRETE;
			fie->discrete.numerator = 1;
			fie->discrete.denominator = modes[i].fps;
			break;
		}
	}

	return 0;
}

int video_imager_set_fmt(const struct device *const dev, enum video_endpoint_id ep,
			 struct video_format *fmt)
{
	struct video_imager_data *data = dev->data;
	size_t fmt_id;
	int ret;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		LOG_ERR("Only the output endpoint is supported for %s", dev->name);
		return -EINVAL;
	}

	ret = video_format_caps_index(data->fmts, fmt, &fmt_id);
	if (ret != 0) {
		LOG_ERR("Format %x %ux%u not found for %s", fmt->pixelformat, fmt->width,
			fmt->height, dev->name);
		return ret;
	}

	ret = video_imager_set_mode(dev, &data->modes[fmt_id][0]);
	if (ret != 0) {
		return ret;
	}

	data->fmt_id = fmt_id;
	data->fmt = *fmt;

	return 0;
}

int video_imager_get_fmt(const struct device *dev, enum video_endpoint_id ep,
			 struct video_format *fmt)
{
	struct video_imager_data *data = dev->data;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	*fmt = data->fmt;

	return 0;
}

int video_imager_get_caps(const struct device *dev, enum video_endpoint_id ep,
			  struct video_caps *caps)
{
	struct video_imager_data *data = dev->data;

	if (ep != VIDEO_EP_OUT && ep != VIDEO_EP_ALL) {
		return -EINVAL;
	}

	caps->format_caps = data->fmts;

	return 0;
}

int video_imager_init(const struct device *dev, const void *init_regs, int default_fmt)
{
	struct video_imager_data *data = dev->data;
	struct video_format fmt;
	int ret;

	__ASSERT_NO_MSG(data->write_multi_fn != NULL);
	__ASSERT_NO_MSG(data->i2c != NULL);
	__ASSERT_NO_MSG(data->i2c->bus != NULL);
	__ASSERT_NO_MSG(data->modes != NULL);
	__ASSERT_NO_MSG(data->fmts != NULL);

	if (!device_is_ready(data->i2c->bus)) {
		LOG_ERR("I2C bus device %s is not ready", data->i2c->bus->name);
		return -ENODEV;
	}

	if (init_regs != NULL) {
		ret = data->write_multi_fn(data->i2c, init_regs);
		if (ret != 0) {
			LOG_ERR("Could not set %s initial registers", dev->name);
			return ret;
		}
	}

	fmt.pixelformat = data->fmts[default_fmt].pixelformat;
	fmt.width = data->fmts[default_fmt].width_min;
	fmt.height = data->fmts[default_fmt].height_min;
	fmt.pitch = fmt.width * 2;

	ret = video_set_format(dev, VIDEO_EP_OUT, &fmt);
	if (ret != 0) {
		LOG_ERR("Failed to set %s to default format %x %ux%u", dev->name, fmt.pixelformat,
			fmt.width, fmt.height);
	}

	return 0;
}
