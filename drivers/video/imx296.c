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

#include "video_common.h"

LOG_MODULE_REGISTER(imx296, CONFIG_VIDEO_LOG_LEVEL);

#define IMX296_FULL_WIDTH		1440
#define IMX296_FULL_HEIGHT		1080

/* TODO for debug only, remove me */
#define WIDTH 640
#define HEIGHT 480

/* Register flags definition */
#define IMX296_REG8(addr)		((addr) | VIDEO_REG_ADDR16_DATA8)
#define IMX296_REG16(addr)		((addr) | VIDEO_REG_ADDR16_DATA16_LE)
#define IMX296_REG24(addr)		((addr) | VIDEO_REG_ADDR16_DATA24_LE)

/* Register address definition */
#define IMX296_REG_STANDBY		IMX296_REG8(0x3000)
#define IMX296_REG_XMSTA		IMX296_REG8(0x300a)
#define IMX296_REG_INCKSEL0		IMX296_REG8(0x3089)
#define IMX296_REG_INCKSEL1		IMX296_REG8(0x308a)
#define IMX296_REG_INCKSEL2		IMX296_REG8(0x308b)
#define IMX296_REG_INCKSEL3		IMX296_REG8(0x308c)
#define IMX296_REG_INCK			IMX296_REG8(0x418c)
#define IMX296_REG_PGHPOS		IMX296_REG16(0x3239)
#define IMX296_REG_PGVPOS		IMX296_REG16(0x323c)
#define IMX296_REG_PGHPSTEP		IMX296_REG8(0x323e)
#define IMX296_REG_PGVPSTEP		IMX296_REG8(0x323f)
#define IMX296_REG_PGHPNUM		IMX296_REG8(0x3240)
#define IMX296_REG_PGVPNUM		IMX296_REG8(0x3241)
#define IMX296_REG_PGDATA1		IMX296_REG16(0x3244)
#define IMX296_REG_PGDATA2		IMX296_REG16(0x3246)
#define IMX296_REG_PGHGSTEP		IMX296_REG8(0x3249)
#define IMX296_REG_BLKLEVEL		IMX296_REG16(0x3254)
#define IMX296_REG_BLKLEVELAUTO		IMX296_REG16(0x3022)
#define IMX296_REG_FID0_ROI_ON		IMX296_REG16(0x3300)
#define IMX296_REG_FID0_ROIPH1		IMX296_REG16(0x3310)
#define IMX296_REG_FID0_ROIPV1		IMX296_REG16(0x3312)
#define IMX296_REG_FID0_ROIWV1		IMX296_REG16(0x3314)
#define IMX296_REG_FID0_ROIWH1		IMX296_REG16(0x3316)
#define IMX296_REG_VMAX			IMX296_REG24(0x3010)
#define IMX296_REG_HMAX			IMX296_REG16(0x3014)

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
	{IMX296_REG_INCKSEL0, 0x80},
	{IMX296_REG_INCKSEL1, 0x0b},
	{IMX296_REG_INCKSEL2, 0x80},
	{IMX296_REG_INCKSEL3, 0x08},
	{IMX296_REG_INCK, 0x74},
	{0},
};

/* This is the clock frequency present on the Raspberry Pi GS module. */
static const struct video_reg clk_54_000_mhz[] __unused = {
	{IMX296_REG_INCKSEL0, 0xb0},
	{IMX296_REG_INCKSEL1, 0x0f},
	{IMX296_REG_INCKSEL2, 0xb0},
	{IMX296_REG_INCKSEL3, 0x0c},
	{IMX296_REG_INCK, 0xa8},
	{0},
};

static const struct video_reg clk_74_250_mhz[] __unused = {
	{IMX296_REG_INCKSEL0, 0x80},
	{IMX296_REG_INCKSEL1, 0x0f},
	{IMX296_REG_INCKSEL2, 0x80},
	{IMX296_REG_INCKSEL3, 0x0c},
	{IMX296_REG_INCK, 0xe8},
	{0},
};

static const struct video_reg size_WIDTHxHEIGHT[] = {
	/* Enable vertical and horizontal ROI selection */
	{IMX296_REG_FID0_ROI_ON, BIT(0) | BIT(1)},
	/* Set crop start to (0, 0) */
	{IMX296_REG_FID0_ROIPH1, 0x00},
	{IMX296_REG_FID0_ROIPV1, 0x00},
	/* Set crop end to (W, H) */
	{IMX296_REG_FID0_ROIWH1, WIDTH},
	{IMX296_REG_FID0_ROIWV1, HEIGHT},
	/* horizontal blanking time (74.25 MHz clock ticks) */
	{IMX296_REG_HMAX, WIDTH + 32},
	/* vertical blanking time (number of lines) */
	{IMX296_REG_VMAX, HEIGHT + 64},
	{0},
};

static const struct video_imager_mode modes_WIDTHxHEIGHT[] = {
	{.fps = 30, .regs = {size_WIDTHxHEIGHT, clk_54_000_mhz}},
	{0},
};

static const struct video_imager_mode *modes[] = {
	modes_WIDTHxHEIGHT,
	NULL,
};

static const struct video_format_cap fmts[] = {
	VIDEO_IMAGER_FORMAT_CAP(VIDEO_PIX_FMT_GBRG8, WIDTH, HEIGHT),
	{0},
};

static int imx296_set_stream(const struct device *dev, bool on)
{
	const struct video_imager_config *cfg = dev->config;
	int ret;

	if (on) {
		ret = video_write_cci_reg(&cfg->i2c, IMX296_REG_STANDBY, 0x00);
		if (ret != 0) {
			return ret;
		}

		k_sleep(K_MSEC(2));

		ret = video_write_cci_reg(&cfg->i2c, IMX296_REG_XMSTA, 0x00);
		if (ret != 0) {
			return ret;
		}
	} else {
		ret = video_write_cci_reg(&cfg->i2c, IMX296_REG_XMSTA, 0x01);
		if (ret != 0) {
			return ret;
		}

		return video_write_cci_reg(&cfg->i2c, IMX296_REG_STANDBY, 0x01);
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
	/* Implementation specific to this sensor */
	.set_stream = imx296_set_stream,
};

static int imx296_init(const struct device *dev)
{
	return video_imager_init(dev, init_regs, 0);
}

#define IMX296_INIT(n)                                                                             \
	static struct video_imager_config imx296_cfg_##n = {                                       \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.fmts = fmts,                                                                      \
		.modes = modes,                                                                    \
		.write_multi = &video_write_cci_multi,                                             \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &imx296_init, NULL, NULL, &imx296_cfg_##n, POST_KERNEL,           \
			      CONFIG_VIDEO_INIT_PRIORITY, &imx296_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IMX296_INIT)
