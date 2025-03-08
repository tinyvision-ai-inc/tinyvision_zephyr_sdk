/*
 * Copyright (c) 2024-2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sony_imx219

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

#include "video_imager.h"

LOG_MODULE_REGISTER(imx219, CONFIG_VIDEO_LOG_LEVEL);

#define IMX219_WIDTH	3280
#define IMX219_HEIGHT	2464

#define IMX219_REG_CHIP_ID		0x0000
#define IMX219_REG_SOFTWARE_RESET	0x0103
#define IMX219_REG_MODE_SELECT		0x0100
#define IMX219_REG_ANALOG_GAIN		0x0157
#define IMX219_REG_DIGITAL_GAIN		0x0158
#define IMX219_REG_INTEGRATION_TIME	0x015A
#define IMX219_REG_TESTPATTERN		0x0600

/* Definition of cropping coordinates of the full frame */
#define X_ADD_STA_A(width)		U16(0x0164, (IMX219_WIDTH - (width)) / 2)
#define X_ADD_END_A(width)		U16(0x0166, (IMX219_WIDTH + (width)) / 2 - 1)
#define Y_ADD_STA_A(height)		U16(0x0168, (IMX219_HEIGHT - (height)) / 2)
#define Y_ADD_END_A(height)		U16(0x016a, (IMX219_HEIGHT + (height)) / 2 - 1)

static const struct video_imager_reg16 init_regs[] = {
	U8(0x0100, 0x00),		/* MODE_SELECT: standby */

	/* Enable access to registers from 0x3000 to 0x5fff */
	U8(0x30EB, 0x05),
	U8(0x30EB, 0x0C),
	U8(0x300A, 0xFF),
	U8(0x300B, 0xFF),
	U8(0x30EB, 0x05),
	U8(0x30EB, 0x09),

	/* MIPI configuration registers */
	U8(0x0114, 0x01),		/* CSI_LANE_MODE: 2 lanes */
	U8(0x0128, 0x00),		/* DPHY_CTRL: timing auto */

	/* Clock configuration registers */
	U16(0x012a, 24 << 8),		/* EXCK_FREQ: 24 MHz */
	U8(0x0304, 0x03),		/* PREPLLCK_VT_DIV: auto */
	U8(0x0305, 0x03),		/* PREPLLCK_OP_DIV: auto */
	U8(0x0309, 10),			/* OPPXCK_DIV: 10-bits per pixel */
	U8(0x0301, 5),			/* VTPXCK_DIV */
	U8(0x0303, 1),			/* VTSYCK_DIV */
	U8(0x030b, 1),			/* OPSYCK_DIV */
	U16(0x0306, 32),		/* PLL_VT_MPY: Pixel/Sys clock multiplier */
	U16(0x030c, 50),		/* PLL_OP_MPY: Output clock multiplier */

	/* Undocumented registers */
	U8(0x455E, 0x00),
	U8(0x471E, 0x4B),
	U8(0x4767, 0x0F),
	U8(0x4750, 0x14),
	U8(0x4540, 0x00),
	U8(0x47B4, 0x14),
	U8(0x4713, 0x30),
	U8(0x478B, 0x10),
	U8(0x478F, 0x10),
	U8(0x4793, 0x10),
	U8(0x4797, 0x0E),
	U8(0x479B, 0x0E),

	/* Timing and format registers */
	U16(0x0162, 3448),		/* LINE_LENGTH_A */
	U8(0x0170, 1),			/* X_ODD_INC_A */
	U8(0x0171, 1),			/* Y_ODD_INC_A */
	U16(0x018c, (10 << 8) | 10),	/* CSI_DATA_FORMAT_A: 10-bits per pixels */

	/* Custom defaults */
	U8(0x0174, 0x00),		/* BINNING_MODE_H */
	U8(0x0175, 0x00),		/* BINNING_MODE_V */
	U16(0x0158, 5000),		/* DIGITAL_GAIN */
	U8(0x0157, 240),		/* ANALOG_GAIN */
	U16(0x015A, 1600),		/* INTEGRATION_TIME */
	U8(0x0172, 0x03),		/* ORIENTATION */

	{0},
};

static const struct video_imager_reg16 size_640x480[] = {
	X_ADD_STA_A(640), Y_ADD_STA_A(480),
	X_ADD_END_A(640), Y_ADD_END_A(480),
	U16(0x016c, 640),		/* X_OUTPUT_SIZE */
	U16(0x016e, 480),		/* Y_OUTPUT_SIZE */
	U16(0x0160, 480 + 120),		/* FRM_LENGTH_A */
	{0},
};

static const struct video_imager_reg16 size_1920x1080[] = {
	X_ADD_STA_A(1920), Y_ADD_STA_A(1080),
	X_ADD_END_A(1920), Y_ADD_END_A(1080),
	U16(0x016c, 1920),		/* X_OUTPUT_SIZE */
	U16(0x016e, 1080),		/* Y_OUTPUT_SIZE */
	U16(0x0160, 1080 + 120),	/* FRM_LENGTH_A */
	{0},
};

static const struct video_imager_reg16 size_2560x1440[] = {
	X_ADD_STA_A(2560), Y_ADD_STA_A(1440),
	X_ADD_END_A(2560), Y_ADD_END_A(1440),
	U16(0x016c, 2560),		/* X_OUTPUT_SIZE */
	U16(0x016e, 1440),		/* Y_OUTPUT_SIZE */
	U16(0x0160, 1440 + 120),	/* FRM_LENGTH_A */
	{0},
};

static const struct video_imager_reg16 size_3280x2464[] = {
	X_ADD_STA_A(3280), Y_ADD_STA_A(2464),
	X_ADD_END_A(3280), Y_ADD_END_A(2464),
	U16(0x016c, 3280),		/* X_OUTPUT_SIZE */
	U16(0x016e, 2464),		/* Y_OUTPUT_SIZE */
	U16(0x0160, 2464 + 120),	/* FRM_LENGTH_A */
	{0},
};

static const struct video_imager_mode modes_640x480[] = {
	{.fps = 30, .regs = {size_640x480}},
	{0},
};

static const struct video_imager_mode modes_1920x1080[] = {
	{.fps = 30, .regs = {size_1920x1080}},
	{0},
};

static const struct video_imager_mode modes_2560x1440[] = {
	{.fps = 30, .regs = {size_2560x1440}},
	{0},
};

static const struct video_imager_mode modes_3280x2464[] = {
	{.fps = 30, .regs = {size_3280x2464}},
	{0},
};

static const struct video_imager_mode *modes[] = {
//	modes_640x480,
	modes_1920x1080,
//	modes_2560x1440,
//	modes_3280x2464,
	NULL,
};

static const struct video_format_cap fmts[] = {
//	VIDEO_IMAGER_FORMAT_CAP(640, 480, VIDEO_PIX_FMT_BGGR8),
	VIDEO_IMAGER_FORMAT_CAP(1920, 1080, VIDEO_PIX_FMT_BGGR8),
//	VIDEO_IMAGER_FORMAT_CAP(2560, 1440, VIDEO_PIX_FMT_BGGR8),
//	VIDEO_IMAGER_FORMAT_CAP(3280, 2464, VIDEO_PIX_FMT_BGGR8),
	{0},
};

#define imx219_read8   video_imager_reg16_read8
#define imx219_read16  video_imager_reg16_read16
#define imx219_write8  video_imager_reg16_write8
#define imx219_write16 video_imager_reg16_write16

static int imx219_set_stream(const struct device *dev, bool on)
{
	return imx219_write8(dev, IMX219_REG_MODE_SELECT, on ? 0x01 : 0x00);
}

static int imx219_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	switch (cid) {
	case VIDEO_CID_EXPOSURE:
		return imx219_write16(dev, IMX219_REG_INTEGRATION_TIME, (int)value);
	case VIDEO_CID_GAIN:
		return imx219_write8(dev, IMX219_REG_ANALOG_GAIN, (int)value);
	case VIDEO_CID_TEST_PATTERN:
		return imx219_write16(dev, IMX219_REG_TESTPATTERN, (int)value);
	default:
		LOG_WRN("Control not supported");
		return -ENOTSUP;
	}
}

static int imx219_get_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	uint16_t reg16;
	uint8_t reg8;
	int ret;

	switch (cid) {
	case VIDEO_CID_EXPOSURE:
		/* Values for normal frame rate, different range for low frame rate mode */
		ret = imx219_read16(dev, IMX219_REG_INTEGRATION_TIME, &reg16);
		*(uint32_t *)value = reg16;
		return ret;
	case VIDEO_CID_GAIN:
		ret = imx219_read8(dev, IMX219_REG_ANALOG_GAIN, &reg8);
		*(uint32_t *)value = reg8;
		return ret;
	default:
		LOG_WRN("Control not supported");
		return -ENOTSUP;
	}
}

static const DEVICE_API(video, imx219_driver_api) = {
	/* Implementation common to all sensors */
	.set_format = video_imager_set_fmt,
	.get_format = video_imager_get_fmt,
	.get_caps = video_imager_get_caps,
	.set_frmival = video_imager_set_frmival,
	.get_frmival = video_imager_get_frmival,
	.enum_frmival = video_imager_enum_frmival,
	/* Implementation specific o this sensor */
	.set_stream = imx219_set_stream,
	.set_ctrl = imx219_set_ctrl,
	.get_ctrl = imx219_get_ctrl,
};

static int imx219_init(const struct device *dev)
{
	struct video_imager_data *data = dev->data;
	uint16_t chip_id;
	int ret;

	if (!device_is_ready(data->i2c->bus)) {
		LOG_ERR("I2C device %s is not ready", data->i2c->bus->name);
		return -ENODEV;
	}

	k_sleep(K_MSEC(1));

	ret = imx219_write8(dev, IMX219_REG_SOFTWARE_RESET, 1);
	if (ret != 0) {
		goto err;
	}

	/* Initializing time of silicon (t5): 32000 clock cycles, 5.3 msec for 6 MHz */
	k_sleep(K_MSEC(6));

	ret = imx219_read16(dev, IMX219_REG_CHIP_ID, &chip_id);
	if (ret != 0) {
		goto err;
	}

	if (chip_id != 0x0219) {
		LOG_ERR("Wrong chip ID %04x", chip_id);
		return -ENODEV;
	}

	return video_imager_init(dev, init_regs);
err:
	LOG_ERR("Error during %s initialization: %s", dev->name, strerror(-ret));
	return ret;

}

#define IMX219_INIT(n)                                                                             \
	static struct i2c_dt_spec i2c_##n = I2C_DT_SPEC_INST_GET(n);                               \
	static struct video_imager_data data_##n = {                                               \
		.i2c = &i2c_##n,                                                                   \
		.fmts = fmts,                                                                      \
		.modes = modes,                                                                    \
		.write_multi_fn = video_imager_reg16_write8_multi,                                 \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &imx219_init, NULL, &data_##n, NULL, POST_KERNEL,                 \
			      CONFIG_VIDEO_INIT_PRIORITY, &imx219_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IMX219_INIT)
