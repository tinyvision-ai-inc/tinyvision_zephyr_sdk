/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#error THIS DRIVER IS NOT FUNCTIONAL YET

#define DT_DRV_COMPAT sony_imx296

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

#include "video_imager.h"

LOG_MODULE_REGISTER(imx296, CONFIG_VIDEO_LOG_LEVEL);

#define IMX296_REG_STANDBY	0x3000
#define IMX296_REG_XMSTA	0x300a

#define U16(addr, reg) {(addr) + 0, (reg) >> 8}, {(addr) + 1, (reg) >> 0}

static const struct video_imager_reg16 init_regs[] = {
	/* Undocumented init sequence */
	{0x3005, 0xf0},
	{0x309e, 0x04},
	{0x30a0, 0x04},
	{0x30a1, 0x3c},
	{0x30a4, 0x5f},
	{0x30a8, 0x91},
	{0x30ac, 0x28},
	{0x30af, 0x09},
	{0x30df, 0x00},
	{0x3165, 0x00},
	{0x3169, 0x10},
	{0x316a, 0x02},
	{0x31c8, 0xf3},
	{0x31d0, 0xf4},
	{0x321a, 0x00},
	{0x3226, 0x02},
	{0x3256, 0x01},
	{0x3541, 0x72},
	{0x3516, 0x77},
	{0x350b, 0x7f},
	{0x3758, 0xa3},
	{0x3759, 0x00},
	{0x375a, 0x85},
	{0x375b, 0x00},
	{0x3832, 0xf5},
	{0x3833, 0x00},
	{0x38a2, 0xf6},
	{0x38a3, 0x00},
	{0x3a00, 0x80},
	{0x3d48, 0xa3},
	{0x3d49, 0x00},
	{0x3d4a, 0x85},
	{0x3d4b, 0x00},
	{0x400e, 0x58},
	{0x4014, 0x1c},
	{0x4041, 0x2a},
	{0x40a2, 0x06},
	{0x40c1, 0xf6},
	{0x40c7, 0x0f},
	{0x40c8, 0x00},
	{0x4174, 0x00},
	{0},
};

static const struct video_imager_reg16 clk_37_125_mhz[] __unused = {
	{0x3089, 0x80},		/* INCKSEL0 */
	{0x308a, 0x0b},		/* INCKSEL1 */
	{0x308b, 0x80},		/* INCKSEL2 */
	{0x308c, 0x08},		/* INCKSEL3 */
	{0x418c, 0x74},		/* INCK */
	{0},
};

/*
 * Like on the Raspberry Pi GS module.
 */
static const struct video_imager_reg16 clk_54_000_mhz[] __unused = {
	{0x3089, 0xb0},		/* INCKSEL0 */
	{0x308a, 0x0f},		/* INCKSEL1 */
	{0x308b, 0xb0},		/* INCKSEL2 */
	{0x308c, 0x0c},		/* INCKSEL3 */
	{0x418c, 0xa8},		/* INCK */
	{0},
};

static const struct video_imager_reg16 clk_74_250_mhz[] __unused = {
	{0x3089, 0x80},		/* INCKSEL0 */
	{0x308a, 0x0f},		/* INCKSEL1 */
	{0x308b, 0x80},		/* INCKSEL2 */
	{0x308c, 0x0c},		/* INCKSEL3 */
	{0x418c, 0xe8},		/* INCK */
	{0},
};

static const struct video_imager_reg16 gbrg8_1440x1080[] = {
	U16(0x3300, 0x00),	/* FID0_ROIH1ON=0, FID0_ROIV1ON=0 */
	U16(0x3014, 1100),	/* HMAX: horizontal blanking time in 74.25 MHz clock ticks */
	U16(0x3014, 1080 + 64),	/* VMAX: vertical blanking time in number of lines */
	{0},
};

static const struct video_imager_mode gbrg8_1440x1080_modes[] = {
	{.fps = 30, .regs = {gbrg8_1440x1080, clk_54_000_mhz}},
	{0},
};

enum {
	GBRG8_1440x1080,
};

static const struct video_format_cap fmts[] = {
	[GBRG8_1440x1080] = VIDEO_IMAGER_FORMAT_CAP(1440, 1080, VIDEO_PIX_FMT_GBRG8),
	{0},
};

static const struct video_imager_mode *modes[] = {
	[GBRG8_1440x1080] = gbrg8_1440x1080_modes,
	NULL,
};

#define imx296_write8 video_imager_reg16_write8

static int imx296_set_stream(const struct device *dev, bool on)
{
	struct video_imager_data *data = dev->data;
	int ret;

	if (on) {
		ret = imx296_write8(data->i2c, IMX296_REG_STANDBY, 0x00);
		if (ret != 0) {
			return ret;
		}

		k_sleep(K_MSEC(2));

		ret = imx296_write8(data->i2c, IMX296_REG_XMSTA, 0x00);
		if (ret != 0) {
			return ret;
		}
	} else {
		ret = imx296_write8(data->i2c, IMX296_REG_XMSTA, 0x01);
		if (ret != 0) {
			return ret;
		}

		return imx296_write8(data->i2c, IMX296_REG_STANDBY, 0x01);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

static const DEVICE_API(video, imx296_driver_api) = {
	/* Implementation common to all sensors */
	.set_format = video_imager_set_fmt,
	.get_format = video_imager_get_fmt,
	.get_caps = video_imager_get_caps,
	.set_frmival = video_imager_set_frmival,
	.get_frmival = video_imager_get_frmival,
	.enum_frmival = video_imager_enum_frmival,
	/* Implementation specific o this sensor */
	.set_stream = imx296_set_stream,
};

static int imx296_init(const struct device *dev)
{
	return video_imager_init(dev, init_regs, GBRG8_1440x1080);
}

#define IMX296_INIT(n)                                                                             \
	static struct i2c_dt_spec i2c_##n = I2C_DT_SPEC_INST_GET(n);                               \
	static struct video_imager_data data_##n = {                                               \
		.i2c = &i2c_##n,                                                                   \
		.fmts = fmts,                                                                      \
		.modes = modes,                                                                    \
		.write_multi_fn = video_imager_reg16_write8_multi,                                 \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &imx296_init, NULL, &data_##n, NULL, POST_KERNEL,                 \
			      CONFIG_VIDEO_INIT_PRIORITY, &imx296_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IMX296_INIT)
