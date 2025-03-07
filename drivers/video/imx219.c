/*
 * Copyright (c) 2023 Gaurav Singh www.CircuitValley.com
 * Copyright (c) 2024 NXP
 * Copyright (c) 2024 tinyVision.ai Inc.
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

#define CHIP_ID_REG 0x0000
#define CHIP_ID_VAL 0x0219

#define IMX219_REG_SOFTWARE_RESET 0x0103
#define IMX219_SOFTWARE_RESET     1

#define IMX219_REG_MODE_SELECT 0x0100
#define IMX219_MODE_STANDBY    0x00
#define IMX219_MODE_STREAMING  0x01

#define IMX219_REG_CSI_LANE_MODE 0x0114
#define IMX219_CSI_2_LANE_MODE   0x01
#define IMX219_CSI_4_LANE_MODE   0x03

#define IMX219_REG_DPHY_CTRL           0x0128
#define IMX219_DPHY_CTRL_TIMING_AUTO   0
#define IMX219_DPHY_CTRL_TIMING_MANUAL 1

#define IMX219_REG_EXCK_FREQ     0x012a
#define IMX219_REG_EXCK_FREQ_LSB 0x012B

#define IMX219_REG_ANALOG_GAIN 0x0157

#define IMX219_REG_DIGITAL_GAIN_MSB 0x0158
#define IMX219_REG_DIGITAL_GAIN_LSB 0x0159
#define IMX219_DGTL_GAIN_DEF        0x0100

/* Exposure control */
#define IMX219_REG_INTEGRATION_TIME_MSB 0x015A
#define IMX219_REG_INTEGRATION_TIME_LSB 0x015B

/* V_TIMING internal */
#define IMX219_REG_FRAME_LEN_MSB 0x0160
#define IMX219_REG_FRAME_LEN_LSB 0x0161

#define IMX219_REG_LINE_LENGTH_A_MSB 0x0162
#define IMX219_REG_LINE_LENGTH_A_LSB 0x0163
#define IMX219_REG_X_ADD_STA_A_MSB   0x0164
#define IMX219_REG_X_ADD_STA_A_LSB   0x0165
#define IMX219_REG_X_ADD_END_A_MSB   0x0166
#define IMX219_REG_X_ADD_END_A_LSB   0x0167
#define IMX219_REG_Y_ADD_STA_A_MSB   0x0168
#define IMX219_REG_Y_ADD_STA_A_LSB   0x0169
#define IMX219_REG_Y_ADD_END_A_MSB   0x016a
#define IMX219_REG_Y_ADD_END_A_LSB   0x016b
#define IMX219_REG_X_OUTPUT_SIZE_MSB 0x016c
#define IMX219_REG_X_OUTPUT_SIZE_LSB 0x016d
#define IMX219_REG_Y_OUTPUT_SIZE_MSB 0x016e
#define IMX219_REG_Y_OUTPUT_SIZE_LSB 0x016f
#define IMX219_REG_X_ODD_INC_A       0x0170
#define IMX219_REG_Y_ODD_INC_A       0x0171
#define IMX219_REG_ORIENTATION       0x0172

/* Binning  Mode */
#define IMX219_REG_BINNING_MODE_H 0x0174
#define IMX219_REG_BINNING_MODE_V 0x0175
#define IMX219_BINNING_NONE       0x00

#define IMX219_REG_CSI_DATA_FORMAT_A_MSB 0x018c
#define IMX219_REG_CSI_DATA_FORMAT_A_LSB 0x018d

/* PLL Settings */
#define IMX219_REG_VTPXCK_DIV      0x0301
#define IMX219_REG_VTSYCK_DIV      0x0303
#define IMX219_REG_PREPLLCK_VT_DIV 0x0304
#define IMX219_REG_PREPLLCK_OP_DIV 0x0305
#define IMX219_REG_PLL_VT_MPY_MSB  0x0306
#define IMX219_REG_PLL_VT_MPY_LSB  0x0307
#define IMX219_REG_OPPXCK_DIV      0x0309
#define IMX219_REG_OPSYCK_DIV      0x030b
#define IMX219_REG_PLL_OP_MPY_MSB  0x030c
#define IMX219_REG_PLL_OP_MPY_LSB  0x030d

/* Test Pattern Control */
#define IMX219_REG_TESTPATTERN_MSB 0x0600
#define IMX219_REG_TESTPATTERN_LSB 0x0601
#define IMX219_TESTPATTERN_DISABLE 0

#define IMX219_REG_TP_X_OFFSET_MSB 0x0620
#define IMX219_REG_TP_X_OFFSET_LSB 0x0621
#define IMX219_REG_TP_Y_OFFSET_MSB 0x0622
#define IMX219_REG_TP_Y_OFFSET_LSB 0x0623

/* Test pattern colour components */
#define IMX219_REG_TESTP_RED_MSB    0x0602
#define IMX219_REG_TESTP_RED_LSB    0x0603
#define IMX219_REG_TESTP_GREENR_MSB 0x0604
#define IMX219_REG_TESTP_GREENR_LSB 0x0605
#define IMX219_REG_TESTP_BLUE_MSB   0x0606
#define IMX219_REG_TESTP_BLUE_LSB   0x0607

#define IMX219_REG_TP_WINDOW_WIDTH_MSB  0x0624
#define IMX219_REG_TP_WINDOW_WIDTH_LSB  0x0625
#define IMX219_REG_TP_WINDOW_HEIGHT_MSB 0x0626
#define IMX219_REG_TP_WINDOW_HEIGHT_LSB 0x0627

#define IMX219_RESOLUTION_PARAM_NUM 20

static const struct video_imager_reg16 init_regs[] = {
	{IMX219_REG_MODE_SELECT, IMX219_MODE_STANDBY},
	{0x30EB, 0x05}, // access sequence
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{IMX219_REG_CSI_LANE_MODE, IMX219_CSI_2_LANE_MODE},   // 3-> 4Lane 1-> 2Lane
	{IMX219_REG_DPHY_CTRL, IMX219_DPHY_CTRL_TIMING_AUTO}, // DPHY timing 0-> auot 1-> manual
	{IMX219_REG_EXCK_FREQ, 0x18}, // external oscillator frequncy 0x18 -> 24Mhz
	{IMX219_REG_EXCK_FREQ_LSB, 0x00},
	// frame length , Raspberry pi sends this commands continously when recording video
	// @60fps ,writes come at interval of 32ms , Data 355 for resolution 1280x720
	// command 162 also comes along with data 0DE7 also 15A with data 0200
	{IMX219_REG_FRAME_LEN_MSB, 0x06},
	{IMX219_REG_FRAME_LEN_LSB, 0xE3},
	// does not directly affect how many bits on wire in
	// one line does affect how many clock between lines
	{IMX219_REG_LINE_LENGTH_A_MSB, 0x0d},
	// appears to be having step in value, not every LSb change will reflect on fps
	{IMX219_REG_LINE_LENGTH_A_LSB, 0x78},
	{IMX219_REG_X_ADD_STA_A_MSB, 0x02}, // x start
	{IMX219_REG_X_ADD_STA_A_LSB, 0xA8},
	{IMX219_REG_X_ADD_END_A_MSB, 0x0A}, // x end
	{IMX219_REG_X_ADD_END_A_LSB, 0x27},
	{IMX219_REG_Y_ADD_STA_A_MSB, 0x02}, // y start
	{IMX219_REG_Y_ADD_STA_A_LSB, 0xB4},
	{IMX219_REG_Y_ADD_END_A_MSB, 0x06}, // y end
	{IMX219_REG_Y_ADD_END_A_LSB, 0xEB},
	// resolution 1280 -> 5 00 , 1920 -> 780 , 2048 -> 0x8 0x00
	{IMX219_REG_X_OUTPUT_SIZE_MSB, 0x07},
	{IMX219_REG_X_OUTPUT_SIZE_LSB, 0x80},
	// 720 -> 0x02D0 | 1080 -> 0x438  | this setting changes how many line over wire
	// does not affect frame rate
	{IMX219_REG_Y_OUTPUT_SIZE_MSB, 0x04},
	{IMX219_REG_Y_OUTPUT_SIZE_LSB, 0x38},
	{IMX219_REG_X_ODD_INC_A, 0x01},           // increment
	{IMX219_REG_Y_ODD_INC_A, 0x01},           // increment
	{IMX219_REG_BINNING_MODE_H, 0x00},        // binning H 0 off 1 x2 2 x4 3 x2 analog
	{IMX219_REG_BINNING_MODE_V, 0x00},        // binning H 0 off 1 x2 2 x4 3 x2 analog
	{IMX219_REG_CSI_DATA_FORMAT_A_MSB, 0x0A}, // CSI Data format A-> 10bit
	{IMX219_REG_CSI_DATA_FORMAT_A_LSB, 0x0A}, // CSI Data format
	{IMX219_REG_VTPXCK_DIV, 0x05},            // vtpxclkd_div	5 301
	{IMX219_REG_VTSYCK_DIV, 0x01},            // vtsclk _div  1	303
	{IMX219_REG_PREPLLCK_VT_DIV, 0x03},       // external oscillator /3
	{IMX219_REG_PREPLLCK_OP_DIV, 0x03},       // external oscillator /3
	{IMX219_REG_PLL_VT_MPY_MSB, 0x00},        // PLL_VT multiplizer
	{IMX219_REG_PLL_VT_MPY_LSB, 0x52}, // Changes Frame rate with , integration register 0x15a
	{IMX219_REG_OPPXCK_DIV, 0x0A},     // oppxck_div
	{IMX219_REG_OPSYCK_DIV, 0x01},     // opsysck_div
	// PLL_OP // 8Mhz x 0x57 ->696Mhz -> 348Mhz |  0x32 -> 200Mhz | 0x40 -> 256Mhz
	{IMX219_REG_PLL_OP_MPY_MSB, 0x00},
	{IMX219_REG_PLL_OP_MPY_LSB, 0x32},
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
	{IMX219_REG_TESTP_RED_MSB, 0x00},
	{IMX219_REG_TESTP_RED_LSB, 0x00},
	{IMX219_REG_TESTP_GREENR_MSB, 0x00},
	{IMX219_REG_TESTP_GREENR_LSB, 0x00},
	{IMX219_REG_TESTP_BLUE_MSB, 0x00},
	{IMX219_REG_TESTP_BLUE_LSB, 0x00},
	{IMX219_REG_TESTPATTERN_MSB, 0x00}, // test pattern
	{IMX219_REG_TESTPATTERN_LSB, 0x00},
	{IMX219_REG_TP_X_OFFSET_MSB, 0x00}, // tp offset x 0
	{IMX219_REG_TP_X_OFFSET_LSB, 0x00},
	{IMX219_REG_TP_Y_OFFSET_MSB, 0x00}, // tp offset y 0
	{IMX219_REG_TP_Y_OFFSET_LSB, 0x00},
	{IMX219_REG_TP_WINDOW_WIDTH_MSB, 0x05}, // TP width 1920 ->780 1280->500
	{IMX219_REG_TP_WINDOW_WIDTH_LSB, 0x00},
	{IMX219_REG_TP_WINDOW_HEIGHT_MSB, 0x02}, // TP height 1080 -> 438 720->2D0
	{IMX219_REG_TP_WINDOW_HEIGHT_LSB, 0xD0},
	{IMX219_REG_DIGITAL_GAIN_MSB, 0x01},
	{IMX219_REG_DIGITAL_GAIN_LSB, 0x00},
	// analog gain , raspberry pi constinouly changes this depending on scene
	{IMX219_REG_ANALOG_GAIN, 0x80},
	// integration time , really important for frame rate
	{IMX219_REG_INTEGRATION_TIME_MSB, 0x03},
	{IMX219_REG_INTEGRATION_TIME_LSB, 0x51},
	// image_orientation (for both direction) bit[0]: hor bit[1]: vert
	{IMX219_REG_ORIENTATION, 0x03},
	{0},
};

static const struct video_imager_reg16 bggr8_1920x1080[] = {
	{IMX219_REG_PLL_VT_MPY_MSB, 0x00},
	{IMX219_REG_PLL_VT_MPY_LSB, 0x20},
	{IMX219_REG_VTPXCK_DIV, 0x04},
	{IMX219_REG_INTEGRATION_TIME_MSB, 0x04},
	{IMX219_REG_INTEGRATION_TIME_LSB, 0xac},
	{IMX219_REG_ANALOG_GAIN, 0x80},
	{IMX219_REG_LINE_LENGTH_A_MSB, 0x0d},
	{IMX219_REG_LINE_LENGTH_A_LSB, 0x78},
	{IMX219_REG_FRAME_LEN_MSB, 0x04},
	{IMX219_REG_FRAME_LEN_LSB, 0xb0},
	{IMX219_REG_X_ADD_STA_A_MSB, 0x02},
	{IMX219_REG_X_ADD_STA_A_LSB, 0xa8},
	{IMX219_REG_Y_ADD_STA_A_MSB, 0x02},
	{IMX219_REG_Y_ADD_STA_A_LSB, 0xb4},
	{IMX219_REG_X_ADD_END_A_MSB, 0x0a},
	{IMX219_REG_X_ADD_END_A_LSB, 0x27},
	{IMX219_REG_Y_ADD_END_A_MSB, 0x06},
	{IMX219_REG_Y_ADD_END_A_LSB, 0xeb},
	{IMX219_REG_X_OUTPUT_SIZE_MSB, 0x07},
	{IMX219_REG_X_OUTPUT_SIZE_LSB, 0x80},
	{IMX219_REG_Y_OUTPUT_SIZE_MSB, 0x04},
	{IMX219_REG_Y_OUTPUT_SIZE_LSB, 0x38},
	{IMX219_REG_TESTPATTERN_LSB, 0x00},
	{IMX219_REG_BINNING_MODE_H, 0x00},
	{IMX219_REG_BINNING_MODE_V, 0x00},
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
		return imx219_write16(data->i2c, IMX219_REG_INTEGRATION_TIME_MSB, (int)value);
	case VIDEO_CID_GAIN:
		return imx219_write8(data->i2c, IMX219_REG_ANALOG_GAIN, (int)value);
	case VIDEO_CID_TEST_PATTERN:
		return imx219_write16(data->i2c, IMX219_REG_TESTPATTERN_MSB, (int)value);
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
		ret = imx219_read16(data->i2c, IMX219_REG_INTEGRATION_TIME_MSB, &u16);
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

	k_sleep(K_MSEC(10));

	/* Software reset */
	ret = imx219_write8(data->i2c, IMX219_REG_SOFTWARE_RESET, IMX219_SOFTWARE_RESET);
	if (ret != 0) {
		LOG_ERR("Unable to perform software reset");
		return -EIO;
	}

	k_sleep(K_MSEC(5));

	/* Check sensor chip id */
	ret = imx219_read16(data->i2c, CHIP_ID_REG, &chip_id);
	if (ret != 0) {
		LOG_ERR("Unable to read sensor chip ID, ret = %d", ret);
		return -ENODEV;
	}
	if (chip_id != CHIP_ID_VAL) {
		LOG_ERR("Wrong chip ID: %04x (expected %04x)", chip_id, CHIP_ID_VAL);
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
