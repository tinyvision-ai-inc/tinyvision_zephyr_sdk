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

#include "video_common.h"

LOG_MODULE_REGISTER(video_common, CONFIG_VIDEO_LOG_LEVEL);

int video_read_cci_reg(struct i2c_dt_spec *i2c, uint32_t addr, uint32_t *data)
{
	size_t addr_size = FIELD_GET(VIDEO_REG_ADDR_SIZE_MASK, addr);
	size_t data_size = FIELD_GET(VIDEO_REG_DATA_SIZE_MASK, addr);
	uint8_t *data_ptr = (uint8_t *)data + sizeof(uint32_t) - data_size;
	uint8_t buf_w[sizeof(uint16_t)] = {0};
	int ret;

	*data = 0;

	for (int i = 0; i < data_size; i++, addr += 1) {
		if (addr_size == 1) {
			buf_w[0] = addr;
		} else {
			sys_put_be16(addr, &buf_w[0]);
		}

		ret = i2c_write_read_dt(i2c, buf_w, addr_size, &data_ptr[i], 1);
		if (ret != 0) {
			return ret;
		}
	}

	*data = sys_be32_to_cpu(*data);

	return 0;
}

int video_write_cci_reg(struct i2c_dt_spec *i2c, uint32_t addr, uint32_t data)
{
	size_t addr_size = FIELD_GET(VIDEO_REG_ADDR_SIZE_MASK, addr);
	size_t data_size = FIELD_GET(VIDEO_REG_DATA_SIZE_MASK, addr);
	uint8_t *data_ptr = (uint8_t *)&data + sizeof(uint32_t) - data_size;
	uint8_t buf_w[sizeof(uint16_t) + sizeof(uint32_t)] = {0};
	int ret;

	data = sys_cpu_to_be32(data);

	for (int i = 0; i < data_size; i++, addr += 1) {
		if (addr_size == 1) {
			buf_w[0] = addr;
		} else {
			sys_put_be16(addr, &buf_w[0]);
		}

		buf_w[addr_size] = data_ptr[i];

		ret = i2c_write_dt(i2c, buf_w, addr_size + 1);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

int video_write_cci_multi(struct i2c_dt_spec *i2c, const struct video_reg *regs)
{
	int ret;

	for (int i = 0; regs[i].addr != 0; i++) {
		ret = video_write_cci_reg(i2c, regs[i].addr, regs[i].data);
		if (ret != 0) {
			LOG_ERR("Failed to write 0x%04x to register 0x%02x",
				regs[i].data, regs[i].addr);
			return ret;
		}
	}

	return 0;
}

/* Common implementation for imagers (a.k.a. image sensor) drivers */

int video_imager_set_mode(const struct device *dev, const struct video_imager_mode *mode)
{
	struct video_imager_data *data = dev->data;
	int ret;

	if (data->mode == mode) {
		LOG_DBG("%s is arlready in the mode requested", dev->name);
		return 0;
	}

	/* Write each register table to the device */
	for (int i = 0; i < ARRAY_SIZE(mode->regs) && mode->regs[i] != NULL; i++) {
		ret = data->write_multi(&data->i2c, mode->regs[i]);
		if (ret != 0) {
			LOG_ERR("Could not set %s to mode %p, %u FPS", dev->name, mode, mode->fps);
			return ret;
		}
	}

	data->mode = mode;

	return 0;
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
		LOG_ERR("Format '%s' %ux%u not found for %s",
			VIDEO_FOURCC_TO_STR(fie->format->pixelformat),
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
		LOG_ERR("Format '%s' %ux%u not found for device %s",
			VIDEO_FOURCC_TO_STR(fmt->pixelformat), fmt->width, fmt->height, dev->name);
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

int video_imager_init(const struct device *dev, const struct video_reg *init_regs,
		      int default_fmt_idx)
{
	struct video_imager_data *data = dev->data;
	struct video_format fmt;
	int ret;

	__ASSERT_NO_MSG(data->modes != NULL);
	__ASSERT_NO_MSG(data->fmts != NULL);

	if (!device_is_ready(data->i2c.bus)) {
		LOG_ERR("I2C bus device %s is not ready", data->i2c.bus->name);
		return -ENODEV;
	}

	if (init_regs != NULL) {
		ret = data->write_multi(&data->i2c, init_regs);
		if (ret != 0) {
			LOG_ERR("Could not set %s initial registers", dev->name);
			return ret;
		}
	}

	fmt.pixelformat = data->fmts[default_fmt_idx].pixelformat;
	fmt.width = data->fmts[default_fmt_idx].width_max;
	fmt.height = data->fmts[default_fmt_idx].height_max;
	fmt.pitch = fmt.width * video_bits_per_pixel(fmt.pixelformat) / BITS_PER_BYTE;

	ret = video_set_format(dev, VIDEO_EP_OUT, &fmt);
	if (ret != 0) {
		LOG_ERR("Failed to set %s to default format '%s' %ux%u",
			dev->name, VIDEO_FOURCC_TO_STR(fmt.pixelformat), fmt.width, fmt.height);
		return ret;
	}

	return 0;
}
