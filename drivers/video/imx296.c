/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#warning The IMX296 driver not functional yet

#define DT_DRV_COMPAT sony_imx296

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

#include "video_common.h"

LOG_MODULE_REGISTER(imx296, CONFIG_VIDEO_LOG_LEVEL);

#define IMX296_REG8(addr)	((addr) | VIDEO_REG_ADDR16_DATA8)
#define IMX296_REG16(addr)	((addr) | VIDEO_REG_ADDR16_DATA16_BE)

#define IMX296_REG_STANDBY	IMX296_REG8(0x3000)
#define IMX296_REG_XMSTA	IMX296_REG8(0x300a)

static const struct video_reg init_regs[] = {
	/* Undocumented registers */
	{IMX296_REG8(0x3005), 0xf0},
	{IMX296_REG8(0x309e), 0x04},
	{IMX296_REG8(0x30a0), 0x04},
	{IMX296_REG8(0x30a1), 0x3c},
	{IMX296_REG8(0x30a4), 0x5f},
	{IMX296_REG8(0x30a8), 0x91},
	{IMX296_REG8(0x30ac), 0x28},
	{IMX296_REG8(0x30af), 0x09},
	{IMX296_REG8(0x30df), 0x00},
	{IMX296_REG8(0x3165), 0x00},
	{IMX296_REG8(0x3169), 0x10},
	{IMX296_REG8(0x316a), 0x02},
	{IMX296_REG8(0x31c8), 0xf3},
	{IMX296_REG8(0x31d0), 0xf4},
	{IMX296_REG8(0x321a), 0x00},
	{IMX296_REG8(0x3226), 0x02},
	{IMX296_REG8(0x3256), 0x01},
	{IMX296_REG8(0x3541), 0x72},
	{IMX296_REG8(0x3516), 0x77},
	{IMX296_REG8(0x350b), 0x7f},
	{IMX296_REG8(0x3758), 0xa3},
	{IMX296_REG8(0x3759), 0x00},
	{IMX296_REG8(0x375a), 0x85},
	{IMX296_REG8(0x375b), 0x00},
	{IMX296_REG8(0x3832), 0xf5},
	{IMX296_REG8(0x3833), 0x00},
	{IMX296_REG8(0x38a2), 0xf6},
	{IMX296_REG8(0x38a3), 0x00},
	{IMX296_REG8(0x3a00), 0x80},
	{IMX296_REG8(0x3d48), 0xa3},
	{IMX296_REG8(0x3d49), 0x00},
	{IMX296_REG8(0x3d4a), 0x85},
	{IMX296_REG8(0x3d4b), 0x00},
	{IMX296_REG8(0x400e), 0x58},
	{IMX296_REG8(0x4014), 0x1c},
	{IMX296_REG8(0x4041), 0x2a},
	{IMX296_REG8(0x40a2), 0x06},
	{IMX296_REG8(0x40c1), 0xf6},
	{IMX296_REG8(0x40c7), 0x0f},
	{IMX296_REG8(0x40c8), 0x00},
	{IMX296_REG8(0x4174), 0x00},
	{0},
};

static const struct video_reg clk_37_125_mhz[] __unused = {
	{IMX296_REG8(0x3089), 0x80},		/* INCKSEL0 */
	{IMX296_REG8(0x308a), 0x0b},		/* INCKSEL1 */
	{IMX296_REG8(0x308b), 0x80},		/* INCKSEL2 */
	{IMX296_REG8(0x308c), 0x08},		/* INCKSEL3 */
	{IMX296_REG8(0x418c), 0x74},		/* INCK */
	{0},
};

/* This is the clock frequency present on the Raspberry Pi GS module. */
static const struct video_reg clk_54_000_mhz[] __unused = {
	{IMX296_REG8(0x3089), 0xb0},		/* INCKSEL0 */
	{IMX296_REG8(0x308a), 0x0f},		/* INCKSEL1 */
	{IMX296_REG8(0x308b), 0xb0},		/* INCKSEL2 */
	{IMX296_REG8(0x308c), 0x0c},		/* INCKSEL3 */
	{IMX296_REG8(0x418c), 0xa8},		/* INCK */
	{0},
};

static const struct video_reg clk_74_250_mhz[] __unused = {
	{IMX296_REG8(0x3089), 0x80},		/* INCKSEL0 */
	{IMX296_REG8(0x308a), 0x0f},		/* INCKSEL1 */
	{IMX296_REG8(0x308b), 0x80},		/* INCKSEL2 */
	{IMX296_REG8(0x308c), 0x0c},		/* INCKSEL3 */
	{IMX296_REG8(0x418c), 0xe8},		/* INCK */
	{0},
};

#if 0
 = {
	{IMX296_REG16(0x3239), 8},		/* PGHPOS */
	{IMX296_REG16(0x323c), 8},		/* PGVPOS */
	{IMX296_REG8(0x323e), 8},		/* PGHPSTEP */
	{IMX296_REG8(0x323f), 8},		/* PGVPSTEP */
	{IMX296_REG8(0x3240), 100},		/* PGHPNUM */
	{IMX296_REG8(0x3241), 100},		/* PGVPNUM */
	{IMX296_REG16(0x3244), 0x300},		/* PGDATA1 */
	{IMX296_REG16(0x3246), 0x100},		/* PGDATA2 */
	{IMX296_REG8(0x3249), 0},		/* PGHGSTEP */
	{IMX296_REG16(0x3254), 0},		/* BLKLEVEL */
	{IMX296_REG16(0x3022), 0xf0},		/* BLKLEVELAUTO: off */
	{IMX296_REG_PGCTRL, 16, IMX296_PGCTRL_REGEN | IMX296_PGCTRL_CLKEN | IMX296_PGCTRL_MODE(ctrl->val - 1}),
};
#endif

static const struct video_reg size_1440x1080[] = {
	{IMX296_REG16(0x3300), 0x00},		/* FID0_ROIH1ON=0, FID0_ROIV1ON=0 */
	{IMX296_REG16(0x3014), 1080},		/* HMAX: horizontal blanking time in 74.25 MHz clock ticks */
	{IMX296_REG16(0x3014), 1080 + 64},	/* VMAX: vertical blanking time in number of lines */
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

static int imx296_set_stream(const struct device *dev, bool on)
{
	struct video_imager_data *data = dev->data;
	int ret;

	if (on) {
		ret = video_write_cci_reg(&data->i2c, IMX296_REG_STANDBY, 0x00);
		if (ret != 0) {
			return ret;
		}

		k_sleep(K_MSEC(2));

		return video_write_cci_reg(&data->i2c, IMX296_REG_XMSTA, 0x00);
	} else {
		ret = video_write_cci_reg(&data->i2c, IMX296_REG_XMSTA, 0x01);
		if (ret != 0) {
			return ret;
		}

		return video_write_cci_reg(&data->i2c, IMX296_REG_STANDBY, 0x01);
	}

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
	return video_imager_init(dev, init_regs, 0);
}

#define IMX296_INIT(n)                                                                             \
	static struct video_imager_data data_##n = {                                               \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.fmts = fmts,                                                                      \
		.modes = modes,                                                                    \
		.write_multi = &video_write_cci_multi,                                             \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &imx296_init, NULL, &data_##n, NULL, POST_KERNEL,                 \
			      CONFIG_VIDEO_INIT_PRIORITY, &imx296_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IMX296_INIT)
