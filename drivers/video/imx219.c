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

#include "video_common.h"
#include "video_imager.h"
#include "video_ctrls.h"

LOG_MODULE_REGISTER(imx219, CONFIG_VIDEO_LOG_LEVEL);

#define IMX219_FULL_WIDTH		3280
#define IMX219_FULL_HEIGHT		2464

#define IMX219_REG8(addr)		((addr) | VIDEO_REG_ADDR16_DATA8)
#define IMX219_REG16(addr)		((addr) | VIDEO_REG_ADDR16_DATA16_BE)

#define IMX219_REG_CHIP_ID		IMX219_REG16(0x0000)
#define IMX219_REG_SOFTWARE_RESET	IMX219_REG8(0x0103)
#define IMX219_REG_MODE_SELECT		IMX219_REG8(0x0100)
#define IMX219_REG_ANALOG_GAIN		IMX219_REG8(0x0157)
#define IMX219_REG_DIGITAL_GAIN		IMX219_REG16(0x0158)
#define IMX219_REG_INTEGRATION_TIME	IMX219_REG16(0x015A)
#define IMX219_REG_TESTPATTERN		IMX219_REG16(0x0600)
#define IMX219_REG_TP_WINDOW_WIDTH	IMX219_REG16(0x0624)
#define IMX219_REG_TP_WINDOW_HEIGHT	IMX219_REG16(0x0626)
#define IMX219_REG_MODE_SELECT		IMX219_REG8(0x0100)
#define IMX219_REG_CSI_LANE_MODE	IMX219_REG8(0x0114)
#define IMX219_REG_DPHY_CTRL		IMX219_REG8(0x0128)
#define IMX219_REG_EXCK_FREQ		IMX219_REG16(0x012a)
#define IMX219_REG_PREPLLCK_VT_DIV	IMX219_REG8(0x0304)
#define IMX219_REG_PREPLLCK_OP_DIV	IMX219_REG8(0x0305)
#define IMX219_REG_OPPXCK_DIV		IMX219_REG8(0x0309)
#define IMX219_REG_VTPXCK_DIV		IMX219_REG8(0x0301)
#define IMX219_REG_VTSYCK_DIV		IMX219_REG8(0x0303)
#define IMX219_REG_OPSYCK_DIV		IMX219_REG8(0x030b)
#define IMX219_REG_PLL_VT_MPY		IMX219_REG16(0x0306)
#define IMX219_REG_PLL_OP_MPY		IMX219_REG16(0x030c)
#define IMX219_REG_LINE_LENGTH_A	IMX219_REG16(0x0162)
#define IMX219_REG_CSI_DATA_FORMAT_A0	IMX219_REG8(0x018c)
#define IMX219_REG_CSI_DATA_FORMAT_A1	IMX219_REG8(0x018d)
#define IMX219_REG_BINNING_MODE_H	IMX219_REG8(0x0174)
#define IMX219_REG_BINNING_MODE_V	IMX219_REG8(0x0175)
#define IMX219_REG_ORIENTATION		IMX219_REG8(0x0172)
#define IMX219_REG_FRM_LENGTH_A		IMX219_REG16(0x0160)
#define IMX219_REG_X_ADD_STA_A		IMX219_REG16(0x0164)
#define IMX219_REG_X_ADD_END_A		IMX219_REG16(0x0166)
#define IMX219_REG_Y_ADD_STA_A		IMX219_REG16(0x0168)
#define IMX219_REG_Y_ADD_END_A		IMX219_REG16(0x016a)
#define IMX219_REG_X_OUTPUT_SIZE	IMX219_REG16(0x016c)
#define IMX219_REG_Y_OUTPUT_SIZE	IMX219_REG16(0x016e)
#define IMX219_REG_X_ODD_INC_A		IMX219_REG8(0x0170)
#define IMX219_REG_Y_ODD_INC_A		IMX219_REG8(0x0171)

struct imx219_data {
	struct video_imager_data imager;
	struct {
		struct video_ctrl exposure;
		struct video_ctrl gain;
		struct video_ctrl test_pattern;
	} ctrl;
};

/* Registers to crop down a resolution to a centered width and height */
static const struct video_reg init_regs[] = {
	{IMX219_REG_MODE_SELECT, 0x00},		/* Standby */

	/* Enable access to registers from 0x3000 to 0x5fff */
	{IMX219_REG8(0x30eb), 0x05},
	{IMX219_REG8(0x30eb), 0x0c},
	{IMX219_REG8(0x300a), 0xff},
	{IMX219_REG8(0x300b), 0xff},
	{IMX219_REG8(0x30eb), 0x05},
	{IMX219_REG8(0x30eb), 0x09},

	/* MIPI configuration registers */
	{IMX219_REG_CSI_LANE_MODE, 0x01},	/* 2 Lanes */
	{IMX219_REG_DPHY_CTRL, 0x00},		/* Timing auto */

	/* Clock configuration registers */
	{IMX219_REG_EXCK_FREQ, 24 << 8},	/* 24 MHz */

	/* Undocumented registers */
	{IMX219_REG8(0x455e), 0x00},
	{IMX219_REG8(0x471e), 0x4b},
	{IMX219_REG8(0x4767), 0x0f},
	{IMX219_REG8(0x4750), 0x14},
	{IMX219_REG8(0x4540), 0x00},
	{IMX219_REG8(0x47b4), 0x14},
	{IMX219_REG8(0x4713), 0x30},
	{IMX219_REG8(0x478b), 0x10},
	{IMX219_REG8(0x478f), 0x10},
	{IMX219_REG8(0x4793), 0x10},
	{IMX219_REG8(0x4797), 0x0e},
	{IMX219_REG8(0x479b), 0x0e},

	/* Timing and format registers */
	{IMX219_REG_LINE_LENGTH_A, 3448},
	{IMX219_REG_X_ODD_INC_A, 1},
	{IMX219_REG_Y_ODD_INC_A, 1},

	/* Custom defaults */
	{IMX219_REG_BINNING_MODE_H, 0x00},	/* No binning */
	{IMX219_REG_BINNING_MODE_V, 0x00}	/* No binning */,
	{IMX219_REG_DIGITAL_GAIN, 5000},
	{IMX219_REG_ANALOG_GAIN, 240},
	{IMX219_REG_INTEGRATION_TIME, 500},
	{IMX219_REG_ORIENTATION, 0x03},
};

#define IMX219_REGS_CROP(width, height)                                                            \
	{IMX219_REG_X_ADD_STA_A, (IMX219_FULL_WIDTH - (width)) / 2},                               \
	{IMX219_REG_X_ADD_END_A, (IMX219_FULL_WIDTH + (width)) / 2 - 1},                           \
	{IMX219_REG_Y_ADD_STA_A, (IMX219_FULL_HEIGHT - (height)) / 2},                             \
	{IMX219_REG_Y_ADD_END_A, (IMX219_FULL_HEIGHT + (height)) / 2 - 1}


static const struct video_reg fmt_raw10[] = {
	{IMX219_REG_CSI_DATA_FORMAT_A0, 10},
	{IMX219_REG_CSI_DATA_FORMAT_A1, 10},
	{IMX219_REG_OPPXCK_DIV, 10},
};

static const struct video_reg fps_30[] = {
	{IMX219_REG_PREPLLCK_VT_DIV, 0x03},	/* Auto */
	{IMX219_REG_PREPLLCK_OP_DIV, 0x03},	/* Auto */
	{IMX219_REG_VTPXCK_DIV, 4},		/* Video Timing clock multiplier */
	{IMX219_REG_VTSYCK_DIV, 1},
	{IMX219_REG_OPPXCK_DIV, 10},		/* Output pixel clock divider */
	{IMX219_REG_OPSYCK_DIV, 1},
	{IMX219_REG_PLL_VT_MPY, 30},		/* Video Timing clock multiplier */
	{IMX219_REG_PLL_OP_MPY, 50},		/* Output clock multiplier */
};

static const struct video_reg fps_15[] = {
	{IMX219_REG_PREPLLCK_VT_DIV, 0x03},	/* Auto */
	{IMX219_REG_PREPLLCK_OP_DIV, 0x03},	/* Auto */
	{IMX219_REG_VTPXCK_DIV, 4},		/* Video Timing clock multiplier */
	{IMX219_REG_VTSYCK_DIV, 1},
	{IMX219_REG_OPPXCK_DIV, 10},		/* Output pixel clock divider */
	{IMX219_REG_OPSYCK_DIV, 1},
	{IMX219_REG_PLL_VT_MPY, 15},		/* Video Timing clock multiplier */
	{IMX219_REG_PLL_OP_MPY, 50},		/* Output clock multiplier */
};

static const struct video_reg stop[] = {
	{IMX219_REG_MODE_SELECT, 0x00},
};

static const struct video_reg start[] = {
	{IMX219_REG_MODE_SELECT, 0x01},
};

static const struct video_reg size_1920x1080[] = {
	IMX219_REGS_CROP(1920, 1080),
	{IMX219_REG_X_OUTPUT_SIZE, 1920},
	{IMX219_REG_Y_OUTPUT_SIZE, 1080},
	{IMX219_REG_FRM_LENGTH_A, 1080 + 20},
	/* Test pattern size */
	{IMX219_REG_TP_WINDOW_WIDTH, 1920},
	{IMX219_REG_TP_WINDOW_HEIGHT, 1080},
};

static const struct video_imager_mode modes_1920x1080[] = {
	{.fps = 15, .regs = {
		VIDEO_IMAGER_REGS(stop),
		VIDEO_IMAGER_REGS(fmt_raw10),
		VIDEO_IMAGER_REGS(size_1920x1080),
		VIDEO_IMAGER_REGS(fps_15),
		VIDEO_IMAGER_REGS(start),
	}},
	{.fps = 30, .regs = {
		VIDEO_IMAGER_REGS(stop),
		VIDEO_IMAGER_REGS(fmt_raw10),
		VIDEO_IMAGER_REGS(size_1920x1080),
		VIDEO_IMAGER_REGS(fps_30),
		VIDEO_IMAGER_REGS(start),
	}},
	{0},
};

enum {
	SIZE_1920x1080,
};

static const struct video_imager_mode *modes[] = {
	[SIZE_1920x1080] = modes_1920x1080,
	NULL,
};

static const struct video_format_cap fmts[] = {
	[SIZE_1920x1080] = VIDEO_IMAGER_FORMAT_CAP(VIDEO_PIX_FMT_SBGGR8, 1920, 1080),
	{0},
};

static int imx219_set_stream(const struct device *dev, bool on, enum video_buf_type type)
{
	const struct video_imager_config *cfg = dev->config;

	return video_write_cci_reg(&cfg->i2c, IMX219_REG_MODE_SELECT, on ? 0x01 : 0x00);
}

static int imx219_set_ctrl(const struct device *dev, unsigned int cid)
{
	const struct video_imager_config *cfg = dev->config;
	struct imx219_data *data = dev->data;

	switch (cid) {
	case VIDEO_CID_EXPOSURE:
		/* Values for normal frame rate, different range for low frame rate mode */
		return video_write_cci_reg(&cfg->i2c, IMX219_REG_INTEGRATION_TIME,
					   data->ctrl.exposure.val);
	case VIDEO_CID_GAIN:
		return video_write_cci_reg(&cfg->i2c, IMX219_REG_ANALOG_GAIN,
					   data->ctrl.gain.val);
	case VIDEO_CID_TEST_PATTERN:
		return video_write_cci_reg(&cfg->i2c, IMX219_REG_TESTPATTERN,
					   data->ctrl.test_pattern.val);
	default:
		LOG_WRN("Control not supported");
		return -ENOTSUP;
	}
}

static const DEVICE_API(video, imx219_driver_api) = {
	/* Local implementation */
	.set_stream = imx219_set_stream,
	.set_ctrl = imx219_set_ctrl,
	/* Default implementation */
	.set_format = video_imager_set_fmt,
	.get_format = video_imager_get_fmt,
	.get_caps = video_imager_get_caps,
	.set_frmival = video_imager_set_frmival,
	.get_frmival = video_imager_get_frmival,
	.enum_frmival = video_imager_enum_frmival,
};

static int imx219_init(const struct device *dev)
{
	const struct video_imager_config *cfg = dev->config;
	uint32_t reg;
	int ret;

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("I2C device %s is not ready", cfg->i2c.bus->name);
		return -ENODEV;
	}

	k_sleep(K_MSEC(1));

	ret = video_write_cci_reg(&cfg->i2c, IMX219_REG_SOFTWARE_RESET, 1);
	if (ret < 0) {
		goto err;
	}

	/* Initializing time of silicon (t5): 32000 clock cycles, 5.3 msec for 6 MHz */
	k_sleep(K_MSEC(6));

	ret = video_read_cci_reg(&cfg->i2c, IMX219_REG_CHIP_ID, &reg);
	if (ret < 0) {
		LOG_ERR("Unable to read Chip ID");
		goto err;
	}

	if (reg != 0x0219) {
		LOG_ERR("Wrong chip ID %04x", reg);
		return -ENODEV;
	}

	ret = video_write_cci_multiregs(&cfg->i2c, init_regs, ARRAY_SIZE(init_regs));
	if (ret < 0) {
		return ret;
	}

	return video_imager_init(dev, SIZE_1920x1080);
err:
	LOG_ERR("Error during %s initialization: %s", dev->name, strerror(-ret));
	return ret;
}

#define IMX219_INIT(n)                                                                             \
	static struct imx219_data imx219_data_##n;                                                 \
                                                                                                   \
	static const struct video_imager_config imx219_cfg_##n = {                                 \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.fmts = fmts,                                                                      \
		.modes = modes,                                                                    \
		.write_multi = video_write_cci_multiregs,                                          \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &imx219_init, NULL, &imx219_data_##n, &imx219_cfg_##n,            \
			      POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY, &imx219_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IMX219_INIT)
