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

#define IMX219_REG_CHIP_ID 0x0000
#define IMX219_REG_SOFTWARE_RESET 0x0103
#define IMX219_REG_MODE_SELECT 0x0100
#define IMX219_MODE_STANDBY    0x00
#define IMX219_MODE_STREAMING  0x01
#define IMX219_REG_ANALOG_GAIN 0x0157
#define IMX219_REG_DIGITAL_GAIN 0x0158
#define IMX219_REG_INTEGRATION_TIME 0x015A
#define IMX219_REG_TESTPATTERN 0x0600

#define U16(addr, reg) {(addr + 0), (reg) >> 8}, {(addr) + 1, (reg) >> 0}

static const struct video_imager_reg16 init_regs[] = {
	{0x0100, 0x00}, /* MODE_SELECT: standby */
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},		/* CSI_LANE_MODE: 2 lanes */
	{0x0128, 0x00},		/* DPHY_CTRL: auto */
	U16(0x012a, 0x1800),	/* EXCK_FREQ: 24 MHz */
	U16(0x0160, 0x06e3),	/* FRAME_LEN */
	U16(0x0162, 0x0d78),	/* LINE_LENGTH_A  */
	U16(0x0164, 0x02A8),	/* X_ADD_STA_A */
	U16(0x0166, 0x0A27),	/* X_ADD_END_A */
	U16(0x0168, 0x02B4),	/* Y_ADD_STA_A */
	U16(0x016a, 0x06EB),	/* Y_ADD_END_A */
	U16(0x016c, 0x0780),	/* X_OUTPUT_SIZE */
	U16(0x016e, 0x0438),	/* Y_OUTPUT_SIZE */
	{0x0170, 0x01},		/* X_ODD_INC_A */
	{0x0171, 0x01},		/* Y_ODD_INC_A */
	{0x0174, 0x00},		/* BINNING_MODE_H */
	{0x0175, 0x00},		/* BINNING_MODE_V */
	U16(0x018c, 0x0A0A),	/* CSI_DATA_FORMAT_A */
	{0x0301, 0x05},		/* VTPXCK_DIV */
	{0x0303, 0x01},		/* VTSYCK_DIV */
	{0x0304, 0x03},		/* PREPLLCK_VT_DIV */
	{0x0305, 0x03},		/* PREPLLCK_OP_DIV */
	U16(0x0306, 0x0052),	/* PLL_VT_MPY */
	{0x0309, 0x0A},		/* OPPXCK_DIV */
	{0x030b, 0x01},		/* OPSYCK_DIV */
	U16(0x030c, 0x0032),	/* PLL_OP_MPY */
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	U16(0x0602, 0x0000),	/* TESTP_RED */
	U16(0x0604, 0x0000),	/* TESTP_GREENR */
	U16(0x0606, 0x0000),	/* TESTP_BLUE */
	U16(0x0600, 0x0000),	/* TESTPATTERN */
	U16(0x0620, 0x0000),	/* TP_X_OFFSET */
	U16(0x0622, 0x0000),	/* TP_Y_OFFSET */
	U16(0x0624, 0x0500),	/* TP_WINDOW_WIDTH */
	U16(0x0626, 0x02D0),	/* TP_WINDOW_HEIGHT */
	U16(0x0158, 0x0100),	/* DIGITAL_GAIN */
	{0x0157, 0x80},		/* ANALOG_GAIN */
	U16(0x015A, 0x0351),	/* INTEGRATION_TIME */
	{0x0172, 0x03},		/* ORIENTATION */
	{0},
};

static const struct video_imager_reg16 bggr8_1920x1080[] = {
	U16(0x0306, 0x0020),	/* PLL_VT_MPY */
	{0x0301, 0x04},		/* VTPXCK_DIV */
	U16(0x015A, 0x04ac),	/* INTEGRATION_TIME */
	{0x0157, 0x80},		/* ANALOG_GAIN */
	U16(0x0162, 0x0d78),	/* LINE_LENGTH_A */
	U16(0x0160, 0x04b0),	/* FRAME_LEN */
	U16(0x0164, 0x02a8),	/* X_ADD_STA_A */
	U16(0x0168, 0x02b4),	/* Y_ADD_STA_A */
	U16(0x0166, 0x0a27),	/* X_ADD_END_A */
	U16(0x016a, 0x06eb),	/* Y_ADD_END_A */
	U16(0x016c, 0x0780),	/* X_OUTPUT_SIZE */
	U16(0x016e, 0x0438),	/* Y_OUTPUT_SIZE */
	{0x0600, 0x00},		/* TESTPATTERN */
	{0x0174, 0x00},		/* BINNING_MODE_H */
	{0x0175, 0x00},		/* BINNING_MODE_V */
	{0},
};

static const struct video_imager_mode bggr8_1920x1080_modes[] = {
	{.fps = 30, .regs = {bggr8_1920x1080, NULL}},
	{0},
};

enum {
	BGGR8_1920x1080,
};

static const struct video_format_cap fmts[] = {
	[BGGR8_1920x1080] = VIDEO_IMAGER_FORMAT_CAP(1920, 1080, VIDEO_PIX_FMT_BGGR8),
	{0},
};

static const struct video_imager_mode *modes[] = {
	[BGGR8_1920x1080] = bggr8_1920x1080_modes,
	NULL,
};

#define imx219_read8   video_imager_reg16_read8
#define imx219_read16  video_imager_reg16_read16
#define imx219_write8  video_imager_reg16_write8
#define imx219_write16 video_imager_reg16_write16

static int imx219_set_stream(const struct device *dev, bool on)
{
	struct video_imager_data *data = dev->data;

	return on ? imx219_write8(data->i2c, IMX219_REG_MODE_SELECT, IMX219_MODE_STREAMING)
		  : imx219_write8(data->i2c, IMX219_REG_MODE_SELECT, IMX219_MODE_STANDBY);
}

static int imx219_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	struct video_imager_data *data = dev->data;

	switch (cid) {
	case VIDEO_CID_EXPOSURE:
		return imx219_write16(data->i2c, IMX219_REG_INTEGRATION_TIME, (int)value);
	case VIDEO_CID_GAIN:
		return imx219_write8(data->i2c, IMX219_REG_ANALOG_GAIN, (int)value);
	case VIDEO_CID_TEST_PATTERN:
		return imx219_write16(data->i2c, IMX219_REG_TESTPATTERN, (int)value);
	default:
		LOG_WRN("Control not supported");
		return -ENOTSUP;
	}
}

static int imx219_get_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	struct video_imager_data *data = dev->data;
	uint32_t *u32 = value;
	uint16_t u16;
	uint8_t u8;
	int ret;

	switch (cid) {
	case VIDEO_CID_EXPOSURE:
		/* Values for normal frame rate, different range for low frame rate mode */
		ret = imx219_read16(data->i2c, IMX219_REG_INTEGRATION_TIME, &u16);
		*u32 = u16;
		return ret;
	case VIDEO_CID_GAIN:
		ret = imx219_read8(data->i2c, IMX219_REG_ANALOG_GAIN, &u8);
		*u32 = u8;
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
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	k_sleep(K_MSEC(1));

	ret = imx219_write8(data->i2c, IMX219_REG_SOFTWARE_RESET, 1);
	if (ret != 0) {
		LOG_ERR("Unable to perform software reset");
		return -EIO;
	}

	/* Initializing time of silicon (t5): 32000 clock cycles, 5.3 msec for 6 MHz */
	k_sleep(K_MSEC(6));

	ret = imx219_read16(data->i2c, IMX219_REG_CHIP_ID, &chip_id);
	if (ret != 0) {
		LOG_ERR("Unable to read sensor chip ID, ret = %d", ret);
		return -ENODEV;
	}

	if (chip_id != 0x0219) {
		LOG_ERR("Wrong chip ID %04x", chip_id);
		return -ENODEV;
	}

	return video_imager_init(dev, init_regs, BGGR8_1920x1080);
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
