/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

static const struct video_imager_reg16 init_regs[] = {
	/* Undocumented registers */
	{0x3005, 8, 0xf0},
	{0x309e, 8, 0x04},
	{0x30a0, 8, 0x04},
	{0x30a1, 8, 0x3c},
	{0x30a4, 8, 0x5f},
	{0x30a8, 8, 0x91},
	{0x30ac, 8, 0x28},
	{0x30af, 8, 0x09},
	{0x30df, 8, 0x00},
	{0x3165, 8, 0x00},
	{0x3169, 8, 0x10},
	{0x316a, 8, 0x02},
	{0x31c8, 8, 0xf3},
	{0x31d0, 8, 0xf4},
	{0x321a, 8, 0x00},
	{0x3226, 8, 0x02},
	{0x3256, 8, 0x01},
	{0x3541, 8, 0x72},
	{0x3516, 8, 0x77},
	{0x350b, 8, 0x7f},
	{0x3758, 8, 0xa3},
	{0x3759, 8, 0x00},
	{0x375a, 8, 0x85},
	{0x375b, 8, 0x00},
	{0x3832, 8, 0xf5},
	{0x3833, 8, 0x00},
	{0x38a2, 8, 0xf6},
	{0x38a3, 8, 0x00},
	{0x3a00, 8, 0x80},
	{0x3d48, 8, 0xa3},
	{0x3d49, 8, 0x00},
	{0x3d4a, 8, 0x85},
	{0x3d4b, 8, 0x00},
	{0x400e, 8, 0x58},
	{0x4014, 8, 0x1c},
	{0x4041, 8, 0x2a},
	{0x40a2, 8, 0x06},
	{0x40c1, 8, 0xf6},
	{0x40c7, 8, 0x0f},
	{0x40c8, 8, 0x00},
	{0x4174, 8, 0x00},
	{0},
};

static const struct video_imager_reg16 clk_37_125_mhz[] __unused = {
	{0x3089, 8, 0x80},		/* INCKSEL0 */
	{0x308a, 8, 0x0b},		/* INCKSEL1 */
	{0x308b, 8, 0x80},		/* INCKSEL2 */
	{0x308c, 8, 0x08},		/* INCKSEL3 */
	{0x418c, 8, 0x74},		/* INCK */
	{0},
};

/* This is the clock frequency present on the Raspberry Pi GS module. */
static const struct video_imager_reg16 clk_54_000_mhz[] __unused = {
	{0x3089, 8, 0xb0},		/* INCKSEL0 */
	{0x308a, 8, 0x0f},		/* INCKSEL1 */
	{0x308b, 8, 0xb0},		/* INCKSEL2 */
	{0x308c, 8, 0x0c},		/* INCKSEL3 */
	{0x418c, 8, 0xa8},		/* INCK */
	{0},
};

static const struct video_imager_reg16 clk_74_250_mhz[] __unused = {
	{0x3089, 8, 0x80},		/* INCKSEL0 */
	{0x308a, 8, 0x0f},		/* INCKSEL1 */
	{0x308b, 8, 0x80},		/* INCKSEL2 */
	{0x308c, 8, 0x0c},		/* INCKSEL3 */
	{0x418c, 8, 0xe8},		/* INCK */
	{0},
};

#if 0
 = {
	{0x3239, 16, 8},		/* PGHPOS */
	{0x323c, 16, 8},		/* PGVPOS */
	{0x323e, 8, 8},			/* PGHPSTEP */
	{0x323f, 8, 8},			/* PGVPSTEP */
	{0x3240, 8, 100},		/* PGHPNUM */
	{0x3241, 8, 100},		/* PGVPNUM */
	{0x3244, 16, 0x300},		/* PGDATA1 */
	{0x3246, 16, 0x100},		/* PGDATA2 */
	{0x3249, 8, 0},			/* PGHGSTEP */
	{0x3254, 16, 0},		/* BLKLEVEL */
	{0x3022, 16, 0xf0},		/* BLKLEVELAUTO: off */
	{PGCTRL, 16, IMX296_PGCTRL_REGEN | IMX296_PGCTRL_CLKEN | IMX296_PGCTRL_MODE(ctrl->val - 1}),
};
#endif

static const struct video_imager_reg16 size_1440x1080[] = {
	{0x3300, 16, 0x00},		/* FID0_ROIH1ON=0, FID0_ROIV1ON=0 */
	{0x3014, 16, 1080},		/* HMAX: horizontal blanking time in 74.25 MHz clock ticks */
	{0x3014, 16, 1080 + 64},	/* VMAX: vertical blanking time in number of lines */
	{0},
};

static const struct video_imager_mode modes_1440x1080[] = {
	{.fps = 30, .regs = {size_1440x1080, clk_54_000_mhz}},
	{0},
};

static const struct video_imager_mode *modes[] = {
	modes_1440x1080,
	NULL,
};

static const struct video_format_cap fmts[] = {
	VIDEO_IMAGER_FORMAT_CAP(1440, 1080, VIDEO_PIX_FMT_GBRG8),
	{0},
};

#define imx296_write8 video_imager_reg16_write8

static int imx296_set_stream(const struct device *dev, bool on)
{
	struct video_imager_data *data = dev->data;
	int ret;

	if (on) {
		ret = imx296_write8(dev, IMX296_REG_STANDBY, 0x00);
		if (ret != 0) {
			return ret;
		}

		k_sleep(K_MSEC(2));

		ret = imx296_write8(dev, IMX296_REG_XMSTA, 0x00);
		if (ret != 0) {
			return ret;
		}
	} else {
		ret = imx296_write8(dev, IMX296_REG_XMSTA, 0x01);
		if (ret != 0) {
			return ret;
		}

		return imx296_write8(dev, IMX296_REG_STANDBY, 0x01);
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
	return video_imager_init(dev, init_regs);
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
