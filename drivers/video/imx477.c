/*
 * Copyright (c) 2024-2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#warning The IMX477 driver not functional yet

#define DT_DRV_COMPAT sony_imx477

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

#include "video_common.h"

LOG_MODULE_REGISTER(imx477, CONFIG_VIDEO_LOG_LEVEL);

#define IMX477_FULL_WIDTH			4072
#define IMX477_FULL_HEIGHT			3176

#define IMX477_REG8(addr)			((addr) | VIDEO_REG_ADDR16_DATA8)
#define IMX477_REG16(addr)			((addr) | VIDEO_REG_ADDR16_DATA16_BE)
#define IMX477_REG32(addr)			((addr) | VIDEO_REG_ADDR16_DATA32_BE)

#define IMX477_REG_CHIP_ID			IMX477_REG16(0x0016)
#define IMX477_REG_LONG_EXP_SHIFT		IMX477_REG8(0x3100)

#define IMX477_REG_MS_SEL			IMX477_REG8(0x3041)
#define IMX477_REG_XVS_IO_CTRL			IMX477_REG8(0x3040)
#define IMX477_REG_EXTOUT_EN			IMX477_REG8(0x4b81)

#define IMX477_REG_SW_RESET			IMX477_REG8(0x0103)
#define IMX477_REG_MODE_SEL			IMX477_REG8(0x0100)
#define IMX477_REG_CSI_LANE			IMX477_REG8(0x0114)
#define IMX477_REG_DPHY_CTRL			IMX477_REG8(0x0808)
#define IMX477_REG_EXCK_FREQ			IMX477_REG16(0x0136)
#define IMX477_REG_FRAME_LEN			IMX477_REG16(0x0340)
#define IMX477_REG_LINE_LEN			IMX477_REG16(0x0342)
#define IMX477_REG_LINE_LEN_INCLK		IMX477_REG16(0x3f56)
#define IMX477_REG_X_ADD_STA			IMX477_REG16(0x0344)
#define IMX477_REG_X_ADD_END			IMX477_REG16(0x0348)
#define IMX477_REG_Y_ADD_STA			IMX477_REG16(0x0346)
#define IMX477_REG_Y_ADD_END			IMX477_REG16(0x034a)

#define IMX477_REG_X_OUT_SIZE			IMX477_REG16(0x034c)
#define IMX477_REG_Y_OUT_SIZE			IMX477_REG16(0x034e)

#define IMX477_REG_IMG_ORIENT			IMX477_REG8(0x0101)
#define IMX477_REG_BINNING_MODE			IMX477_REG8(0x0900)
#define IMX477_REG_BINNING_HV			IMX477_REG8(0x0901)

#define IMX477_REG_BINNING_WEIGHTING		IMX477_REG8(0x0902)
#define IMX477_REG_BIN_CALC_MOD_H		IMX477_REG8(0x0176)
#define IMX477_REG_BIN_CALC_MOD_V		IMX477_REG8(0x0177)

#define IMX477_REG_CSI_FORMAT_C			IMX477_REG8(0x0112)
#define IMX477_REG_CSI_FORMAT_D			IMX477_REG8(0x0113)

#define IMX477_REG_ANA_GAIN_GLOBAL		IMX477_REG16(0x0204)

#define IMX477_REG_ANA_GAIN_GLOBAL1		IMX477_REG16(0x00f0)
#define IMX477_REG_ANA_GAIN_GLOBAL2		IMX477_REG16(0x00f2)
#define IMX477_REG_ANA_GAIN_GLOBAL3		IMX477_REG16(0x00f4)

#define IMX477_REG_DIG_GAIN_GLOBAL1		IMX477_REG16(0x00f6)
#define IMX477_REG_DIG_GAIN_GLOBAL2		IMX477_REG16(0x00f8)
#define IMX477_REG_DIG_GAIN_GLOBAL3		IMX477_REG16(0x00fa)

#define IMX477_REG_FINE_INTEGRATION_TIME	IMX477_REG16(0x0200)
#define IMX477_REG_COARSE_INTEGRATION_TIME	IMX477_REG16(0x0202)

/* Mux to select a source for ADCK, CPCK, HCLK, IVTPXCK: 0 for IOPCK, 1 for IVTCK */
#define IMX477_REG_PLL_MULTI_DRIVE		IMX477_REG8(0x0310)

/* PLL multiplier, valid range: [150, 350] */
#define IMX477_REG_IVT_PLL_MPY			IMX477_REG16(0x0306)
#define IMX477_REG_IVTPXCK_DIV			IMX477_REG8(0x0301)
#define IMX477_REG_IVTSYCK_DIV			IMX477_REG8(0x0303)
#define IMX477_REG_IVT_PREPLLCK_DIV		IMX477_REG8(0x0305)

/* PLL multiplier, valid range: [100, 350] */
#define IMX477_REG_IOP_PREPLLCK_DIV		IMX477_REG8(0x030d)
#define IMX477_REG_IOP_PLL_MPY			IMX477_REG16(0x030e)
#define IMX477_REG_IOPPXCK_DIV			IMX477_REG8(0x0309)
#define IMX477_REG_IOPSYCK_DIV			IMX477_REG8(0x030b)

#define IMX477_REG_TEST_PATTERN			IMX477_REG16(0x0600)
#define IMX477_REG_TP_RED			IMX477_REG16(0x0602)
#define IMX477_REG_TP_GREENR			IMX477_REG16(0x0604)
#define IMX477_REG_TP_BLUE			IMX477_REG16(0x0606)
#define IMX477_REG_TP_GREENB			IMX477_REG16(0x0608)
#define IMX477_REG_TP_X_OFFSET			IMX477_REG16(0x0620)
#define IMX477_REG_TP_Y_OFFSET			IMX477_REG16(0x0622)
#define IMX477_REG_TP_WIDTH			IMX477_REG16(0x0624)
#define IMX477_REG_TP_HEIGHT			IMX477_REG16(0x0626)
/* Whether to go out of HS and into LP mode while frame blank happen */
#define IMX477_REG_FRAME_BLANKSTOP_CTRL		IMX477_REG8(0xe000)

#define IMX477_REG_PD_AREA_WIDTH		IMX477_REG16(0x38a8)
#define IMX477_REG_PD_AREA_HEIGHT		IMX477_REG16(0x38aa)

#define IMX477_REG_FRAME_LENGTH_CTRL		IMX477_REG8(0x0350)
#define IMX477_REG_EBD_SIZE_V			IMX477_REG8(0xbcf1)
#define IMX477_REG_DPGA_GLOBEL_GAIN		IMX477_REG8(0x3ff9)

#define IMX477_REG_X_ENV_INC_CONST		IMX477_REG8(0x0381)
#define IMX477_REG_X_ODD_INC_CONST		IMX477_REG8(0x0383)
#define IMX477_REG_Y_ENV_INC_CONST		IMX477_REG8(0x0385)
#define IMX477_REG_Y_ODD_INC			IMX477_REG8(0x0387)

#define IMX477_REG_MULTI_CAM_MODE		IMX477_REG8(0x3f0b)
#define IMX477_REG_ADC_BIT_SETTING		IMX477_REG8(0x3f0d)

#define IMX477_REG_SCALE_MODE			IMX477_REG8(0x0401)
#define IMX477_REG_SCALE_M			IMX477_REG16(0x0404)
#define IMX477_REG_SCALE_N			IMX477_REG16(0x0406)

#define IMX477_REG_DIG_CROP_X_OFFSET		IMX477_REG16(0x0408)
#define IMX477_REG_DIG_CROP_Y_OFFSET		IMX477_REG16(0x040a)
#define IMX477_REG_DIG_CROP_WIDTH		IMX477_REG16(0x040c)
#define IMX477_REG_DIG_CROP_HEIGHT		IMX477_REG16(0x040e)

#define IMX477_REG_REQ_LINK_BIT_RATE		IMX477_REG32(0x0820)

#define IMX477_REG_TCLK_POST_EX			IMX477_REG16(0x080a)
#define IMX477_REG_THS_PRE_EX			IMX477_REG16(0x080c)
#define IMX477_REG_THS_ZERO_MIN			IMX477_REG16(0x080e)
#define IMX477_REG_THS_TRAIL_EX			IMX477_REG16(0x0810)
#define IMX477_REG_TCLK_TRAIL_MIN		IMX477_REG16(0x0812)
#define IMX477_REG_TCLK_PREP_EX			IMX477_REG16(0x0814)
#define IMX477_REG_TCLK_ZERO_EX			IMX477_REG16(0x0816)
#define IMX477_REG_TLPX_EX			IMX477_REG16(0x0818)

#define IMX477_REG_PDAF_CTRL1_0			IMX477_REG8(0x3e37)
#define IMX477_REG_POWER_SAVE_ENABLE		IMX477_REG8(0x3f50)

#define IMX477_REG_MAP_COUPLET_CORR		IMX477_REG8(0x0b05)
#define IMX477_REG_SING_DYNAMIC_CORR		IMX477_REG8(0x0b06)
#define IMX477_REG_CIT_LSHIFT_LONG_EXP		IMX477_REG8(0x3100)

#define IMX477_REG_TEMP_SENS_CTL		IMX477_REG8(0x0138)

#define IMX477_REG_DOL_HDR_EN			IMX477_REG8(0x00e3)
#define IMX477_REG_DOL_HDR_NUM			IMX477_REG8(0x00e4)
#define IMX477_REG_DOL_CSI_DT_FMT_H_2ND		IMX477_REG8(0x00fc)
#define IMX477_REG_DOL_CSI_DT_FMT_L_2ND		IMX477_REG8(0x00fd)
#define IMX477_REG_DOL_CSI_DT_FMT_H_3ND		IMX477_REG8(0x00fe)
#define IMX477_REG_DOL_CSI_DT_FMT_L_3ND		IMX477_REG8(0x00ff)
#define IMX477_REG_DOL_CONST			IMX477_REG8(0xe013)

static const struct video_reg init_regs[] = {
	/* whether to go out of high-speed and into low-power mode while frame blank happen */

	/* External clock input on board set at 24 MHz */
	{IMX477_REG_EXCK_FREQ, 0x1800},

	{IMX477_REG_TEMP_SENS_CTL, 0x01},		//temprature sensor control
	{IMX477_REG_FRAME_BLANKSTOP_CTRL, 0x01}, // Whether to go out of HS and into LP mode while frame blank happen
	{IMX477_REG8(0xe07a), 0x01},
	{IMX477_REG_DPHY_CTRL, 0x02},
	{IMX477_REG8(0x4ae9), 0x18},
	{IMX477_REG8(0x4aea), 0x08},
	{IMX477_REG8(0xf61c), 0x04},
	{IMX477_REG8(0xf61e), 0x04},
	{IMX477_REG8(0x4ae9), 0x21},
	{IMX477_REG8(0x4aea), 0x80},
	{IMX477_REG_PD_AREA_WIDTH, 0x1fff},
	{IMX477_REG_PD_AREA_HEIGHT, 0x1fff},
	{IMX477_REG8(0x55d4), 0x00},
	{IMX477_REG8(0x55d5), 0x00},
	{IMX477_REG8(0x55d6), 0x07},
	{IMX477_REG8(0x55d7), 0xff},
	{IMX477_REG8(0x55e8), 0x07},
	{IMX477_REG8(0x55e9), 0xff},
	{IMX477_REG8(0x55ea), 0x00},
	{IMX477_REG8(0x55eb), 0x00},
	{IMX477_REG8(0x574c), 0x07},
	{IMX477_REG8(0x574d), 0xff},
	{IMX477_REG8(0x574e), 0x00},
	{IMX477_REG8(0x574f), 0x00},
	{IMX477_REG8(0x5754), 0x00},
	{IMX477_REG8(0x5755), 0x00},
	{IMX477_REG8(0x5756), 0x07},
	{IMX477_REG8(0x5757), 0xff},
	{IMX477_REG8(0x5973), 0x04},
	{IMX477_REG8(0x5974), 0x01},
	{IMX477_REG8(0x5d13), 0xc3},
	{IMX477_REG8(0x5d14), 0x58},
	{IMX477_REG8(0x5d15), 0xa3},
	{IMX477_REG8(0x5d16), 0x1d},
	{IMX477_REG8(0x5d17), 0x65},
	{IMX477_REG8(0x5d18), 0x8c},
	{IMX477_REG8(0x5d1a), 0x06},
	{IMX477_REG8(0x5d1b), 0xa9},
	{IMX477_REG8(0x5d1c), 0x45},
	{IMX477_REG8(0x5d1d), 0x3a},
	{IMX477_REG8(0x5d1e), 0xab},
	{IMX477_REG8(0x5d1f), 0x15},
	{IMX477_REG8(0x5d21), 0x0e},
	{IMX477_REG8(0x5d22), 0x52},
	{IMX477_REG8(0x5d23), 0xaa},
	{IMX477_REG8(0x5d24), 0x7d},
	{IMX477_REG8(0x5d25), 0x57},
	{IMX477_REG8(0x5d26), 0xa8},
	{IMX477_REG8(0x5d37), 0x5a},
	{IMX477_REG8(0x5d38), 0x5a},
	{IMX477_REG8(0x5d77), 0x7f},
	{IMX477_REG8(0x7b75), 0x0e},
	{IMX477_REG8(0x7b76), 0x0b},
	{IMX477_REG8(0x7b77), 0x08},
	{IMX477_REG8(0x7b78), 0x0a},
	{IMX477_REG8(0x7b79), 0x47},
	{IMX477_REG8(0x7b7c), 0x00},
	{IMX477_REG8(0x7b7d), 0x00},
	{IMX477_REG8(0x8d1f), 0x00},
	{IMX477_REG8(0x8d27), 0x00},
	{IMX477_REG8(0x9004), 0x03},
	{IMX477_REG8(0x9200), 0x50},
	{IMX477_REG8(0x9201), 0x6c},
	{IMX477_REG8(0x9202), 0x71},
	{IMX477_REG8(0x9203), 0x00},
	{IMX477_REG8(0x9204), 0x71},
	{IMX477_REG8(0x9205), 0x01},
	{IMX477_REG8(0x9371), 0x6a},
	{IMX477_REG8(0x9373), 0x6a},
	{IMX477_REG8(0x9375), 0x64},
	{IMX477_REG8(0x991a), 0x00},
	{IMX477_REG8(0x996b), 0x8c},
	{IMX477_REG8(0x996c), 0x64},
	{IMX477_REG8(0x996d), 0x50},
	{IMX477_REG8(0x9a4c), 0x0d},
	{IMX477_REG8(0x9a4d), 0x0d},
	{IMX477_REG8(0xa001), 0x0a},
	{IMX477_REG8(0xa003), 0x0a},
	{IMX477_REG8(0xa005), 0x0a},
	{IMX477_REG8(0xa006), 0x01},
	{IMX477_REG8(0xa007), 0xc0},
	{IMX477_REG8(0xa009), 0xc0},
	{IMX477_REG8(0x3d8a), 0x01},
	{IMX477_REG8(0x4421), 0x04},
	{IMX477_REG8(0x7b3b), 0x01},
	{IMX477_REG8(0x7b4c), 0x00},
	{IMX477_REG8(0x9905), 0x00},
	{IMX477_REG8(0x9907), 0x00},
	{IMX477_REG8(0x9909), 0x00},
	{IMX477_REG8(0x990b), 0x00},
	{IMX477_REG8(0x9944), 0x3c},
	{IMX477_REG8(0x9947), 0x3c},
	{IMX477_REG8(0x994a), 0x8c},
	{IMX477_REG8(0x994b), 0x50},
	{IMX477_REG8(0x994c), 0x1b},
	{IMX477_REG8(0x994d), 0x8c},
	{IMX477_REG8(0x994e), 0x50},
	{IMX477_REG8(0x994f), 0x1b},
	{IMX477_REG8(0x9950), 0x8c},
	{IMX477_REG8(0x9951), 0x1b},
	{IMX477_REG8(0x9952), 0x0a},
	{IMX477_REG8(0x9953), 0x8c},
	{IMX477_REG8(0x9954), 0x1b},
	{IMX477_REG8(0x9955), 0x0a},
	{IMX477_REG8(0x9a13), 0x04},
	{IMX477_REG8(0x9a14), 0x04},
	{IMX477_REG8(0x9a19), 0x00},
	{IMX477_REG8(0x9a1c), 0x04},
	{IMX477_REG8(0x9a1d), 0x04},
	{IMX477_REG8(0x9a26), 0x05},
	{IMX477_REG8(0x9a27), 0x05},
	{IMX477_REG8(0x9a2c), 0x01},
	{IMX477_REG8(0x9a2d), 0x03},
	{IMX477_REG8(0x9a2f), 0x05},
	{IMX477_REG8(0x9a30), 0x05},
	{IMX477_REG8(0x9a41), 0x00},
	{IMX477_REG8(0x9a46), 0x00},
	{IMX477_REG8(0x9a47), 0x00},
	{IMX477_REG8(0x9c17), 0x35},
	{IMX477_REG8(0x9c1d), 0x31},
	{IMX477_REG8(0x9c29), 0x50},
	{IMX477_REG8(0x9c3b), 0x2f},
	{IMX477_REG8(0x9c41), 0x6b},
	{IMX477_REG8(0x9c47), 0x2d},
	{IMX477_REG8(0x9c4d), 0x40},
	{IMX477_REG8(0x9c6b), 0x00},
	{IMX477_REG8(0x9c71), 0xc8},
	{IMX477_REG8(0x9c73), 0x32},
	{IMX477_REG8(0x9c75), 0x04},
	{IMX477_REG8(0x9c7d), 0x2d},
	{IMX477_REG8(0x9c83), 0x40},
	{IMX477_REG8(0x9c94), 0x3f},
	{IMX477_REG8(0x9c95), 0x3f},
	{IMX477_REG8(0x9c96), 0x3f},
	{IMX477_REG8(0x9c97), 0x00},
	{IMX477_REG8(0x9c98), 0x00},
	{IMX477_REG8(0x9c99), 0x00},
	{IMX477_REG8(0x9c9a), 0x3f},
	{IMX477_REG8(0x9c9b), 0x3f},
	{IMX477_REG8(0x9c9c), 0x3f},
	{IMX477_REG8(0x9ca0), 0x0f},
	{IMX477_REG8(0x9ca1), 0x0f},
	{IMX477_REG8(0x9ca2), 0x0f},
	{IMX477_REG8(0x9ca3), 0x00},
	{IMX477_REG8(0x9ca4), 0x00},
	{IMX477_REG8(0x9ca5), 0x00},
	{IMX477_REG8(0x9ca6), 0x1e},
	{IMX477_REG8(0x9ca7), 0x1e},
	{IMX477_REG8(0x9ca8), 0x1e},
	{IMX477_REG8(0x9ca9), 0x00},
	{IMX477_REG8(0x9caa), 0x00},
	{IMX477_REG8(0x9cab), 0x00},
	{IMX477_REG8(0x9cac), 0x09},
	{IMX477_REG8(0x9cad), 0x09},
	{IMX477_REG8(0x9cae), 0x09},
	{IMX477_REG8(0x9cbd), 0x50},
	{IMX477_REG8(0x9cbf), 0x50},
	{IMX477_REG8(0x9cc1), 0x50},
	{IMX477_REG8(0x9cc3), 0x40},
	{IMX477_REG8(0x9cc5), 0x40},
	{IMX477_REG8(0x9cc7), 0x40},
	{IMX477_REG8(0x9cc9), 0x0a},
	{IMX477_REG8(0x9ccb), 0x0a},
	{IMX477_REG8(0x9ccd), 0x0a},
	{IMX477_REG8(0x9d17), 0x35},
	{IMX477_REG8(0x9d1d), 0x31},
	{IMX477_REG8(0x9d29), 0x50},
	{IMX477_REG8(0x9d3b), 0x2f},
	{IMX477_REG8(0x9d41), 0x6b},
	{IMX477_REG8(0x9d47), 0x42},
	{IMX477_REG8(0x9d4d), 0x5a},
	{IMX477_REG8(0x9d6b), 0x00},
	{IMX477_REG8(0x9d71), 0xc8},
	{IMX477_REG8(0x9d73), 0x32},
	{IMX477_REG8(0x9d75), 0x04},
	{IMX477_REG8(0x9d7d), 0x42},
	{IMX477_REG8(0x9d83), 0x5a},
	{IMX477_REG8(0x9d94), 0x3f},
	{IMX477_REG8(0x9d95), 0x3f},
	{IMX477_REG8(0x9d96), 0x3f},
	{IMX477_REG8(0x9d97), 0x00},
	{IMX477_REG8(0x9d98), 0x00},
	{IMX477_REG8(0x9d99), 0x00},
	{IMX477_REG8(0x9d9a), 0x3f},
	{IMX477_REG8(0x9d9b), 0x3f},
	{IMX477_REG8(0x9d9c), 0x3f},
	{IMX477_REG8(0x9d9d), 0x1f},
	{IMX477_REG8(0x9d9e), 0x1f},
	{IMX477_REG8(0x9d9f), 0x1f},
	{IMX477_REG8(0x9da0), 0x0f},
	{IMX477_REG8(0x9da1), 0x0f},
	{IMX477_REG8(0x9da2), 0x0f},
	{IMX477_REG8(0x9da3), 0x00},
	{IMX477_REG8(0x9da4), 0x00},
	{IMX477_REG8(0x9da5), 0x00},
	{IMX477_REG8(0x9da6), 0x1e},
	{IMX477_REG8(0x9da7), 0x1e},
	{IMX477_REG8(0x9da8), 0x1e},
	{IMX477_REG8(0x9da9), 0x00},
	{IMX477_REG8(0x9daa), 0x00},
	{IMX477_REG8(0x9dab), 0x00},
	{IMX477_REG8(0x9dac), 0x09},
	{IMX477_REG8(0x9dad), 0x09},
	{IMX477_REG8(0x9dae), 0x09},
	{IMX477_REG8(0x9dc9), 0x0a},
	{IMX477_REG8(0x9dcb), 0x0a},
	{IMX477_REG8(0x9dcd), 0x0a},
	{IMX477_REG8(0x9e17), 0x35},
	{IMX477_REG8(0x9e1d), 0x31},
	{IMX477_REG8(0x9e29), 0x50},
	{IMX477_REG8(0x9e3b), 0x2f},
	{IMX477_REG8(0x9e41), 0x6b},
	{IMX477_REG8(0x9e47), 0x2d},
	{IMX477_REG8(0x9e4d), 0x40},
	{IMX477_REG8(0x9e6b), 0x00},
	{IMX477_REG8(0x9e71), 0xc8},
	{IMX477_REG8(0x9e73), 0x32},
	{IMX477_REG8(0x9e75), 0x04},
	{IMX477_REG8(0x9e94), 0x0f},
	{IMX477_REG8(0x9e95), 0x0f},
	{IMX477_REG8(0x9e96), 0x0f},
	{IMX477_REG8(0x9e97), 0x00},
	{IMX477_REG8(0x9e98), 0x00},
	{IMX477_REG8(0x9e99), 0x00},
	{IMX477_REG8(0x9ea0), 0x0f},
	{IMX477_REG8(0x9ea1), 0x0f},
	{IMX477_REG8(0x9ea2), 0x0f},
	{IMX477_REG8(0x9ea3), 0x00},
	{IMX477_REG8(0x9ea4), 0x00},
	{IMX477_REG8(0x9ea5), 0x00},
	{IMX477_REG8(0x9ea6), 0x3f},
	{IMX477_REG8(0x9ea7), 0x3f},
	{IMX477_REG8(0x9ea8), 0x3f},
	{IMX477_REG8(0x9ea9), 0x00},
	{IMX477_REG8(0x9eaa), 0x00},
	{IMX477_REG8(0x9eab), 0x00},
	{IMX477_REG8(0x9eac), 0x09},
	{IMX477_REG8(0x9ead), 0x09},
	{IMX477_REG8(0x9eae), 0x09},
	{IMX477_REG8(0x9ec9), 0x0a},
	{IMX477_REG8(0x9ecb), 0x0a},
	{IMX477_REG8(0x9ecd), 0x0a},
	{IMX477_REG8(0x9f17), 0x35},
	{IMX477_REG8(0x9f1d), 0x31},
	{IMX477_REG8(0x9f29), 0x50},
	{IMX477_REG8(0x9f3b), 0x2f},
	{IMX477_REG8(0x9f41), 0x6b},
	{IMX477_REG8(0x9f47), 0x42},
	{IMX477_REG8(0x9f4d), 0x5a},
	{IMX477_REG8(0x9f6b), 0x00},
	{IMX477_REG8(0x9f71), 0xc8},
	{IMX477_REG8(0x9f73), 0x32},
	{IMX477_REG8(0x9f75), 0x04},
	{IMX477_REG8(0x9f94), 0x0f},
	{IMX477_REG8(0x9f95), 0x0f},
	{IMX477_REG8(0x9f96), 0x0f},
	{IMX477_REG8(0x9f97), 0x00},
	{IMX477_REG8(0x9f98), 0x00},
	{IMX477_REG8(0x9f99), 0x00},
	{IMX477_REG8(0x9f9a), 0x2f},
	{IMX477_REG8(0x9f9b), 0x2f},
	{IMX477_REG8(0x9f9c), 0x2f},
	{IMX477_REG8(0x9f9d), 0x00},
	{IMX477_REG8(0x9f9e), 0x00},
	{IMX477_REG8(0x9f9f), 0x00},
	{IMX477_REG8(0x9fa0), 0x0f},
	{IMX477_REG8(0x9fa1), 0x0f},
	{IMX477_REG8(0x9fa2), 0x0f},
	{IMX477_REG8(0x9fa3), 0x00},
	{IMX477_REG8(0x9fa4), 0x00},
	{IMX477_REG8(0x9fa5), 0x00},
	{IMX477_REG8(0x9fa6), 0x1e},
	{IMX477_REG8(0x9fa7), 0x1e},
	{IMX477_REG8(0x9fa8), 0x1e},
	{IMX477_REG8(0x9fa9), 0x00},
	{IMX477_REG8(0x9faa), 0x00},
	{IMX477_REG8(0x9fab), 0x00},
	{IMX477_REG8(0x9fac), 0x09},
	{IMX477_REG8(0x9fad), 0x09},
	{IMX477_REG8(0x9fae), 0x09},
	{IMX477_REG8(0x9fc9), 0x0a},
	{IMX477_REG8(0x9fcb), 0x0a},
	{IMX477_REG8(0x9fcd), 0x0a},
	{IMX477_REG8(0xa14b), 0xff},
	{IMX477_REG8(0xa151), 0x0c},
	{IMX477_REG8(0xa153), 0x50},
	{IMX477_REG8(0xa155), 0x02},
	{IMX477_REG8(0xa157), 0x00},
	{IMX477_REG8(0xa1ad), 0xff},
	{IMX477_REG8(0xa1b3), 0x0c},
	{IMX477_REG8(0xa1b5), 0x50},
	{IMX477_REG8(0xa1b9), 0x00},
	{IMX477_REG8(0xa24b), 0xff},
	{IMX477_REG8(0xa257), 0x00},
	{IMX477_REG8(0xa2ad), 0xff},
	{IMX477_REG8(0xa2b9), 0x00},
	{IMX477_REG8(0xb21f), 0x04},
	{IMX477_REG8(0xb35c), 0x00},
	{IMX477_REG8(0xb35e), 0x08},
	{IMX477_REG_CSI_FORMAT_C, 0x0c},
	{IMX477_REG_CSI_FORMAT_D, 0x0c},
	{IMX477_REG_CSI_LANE, 0x01},
	{IMX477_REG_FRAME_LENGTH_CTRL, 0x00},
	{IMX477_REG_EBD_SIZE_V, 0x02},
	{IMX477_REG_DPGA_GLOBEL_GAIN, 0x01},
	{0},
};

/* cropped mode */
static struct video_reg mode_1920x1080_30[] = {
	{IMX477_REG_LINE_LEN, 0x31c4},
	{IMX477_REG_X_ADD_STA, 0x0000},
	{IMX477_REG_Y_ADD_STA, 0x01b8},
	{IMX477_REG_X_ADD_END, 0x0fd7},
	{IMX477_REG_Y_ADD_END, 0x0a27},
	{IMX477_REG8(0x0220), 0x00},
	{IMX477_REG8(0x0221), 0x11},
	{IMX477_REG_X_ENV_INC_CONST, 0x01},
	{IMX477_REG_X_ODD_INC_CONST, 0x01},
	{IMX477_REG_Y_ENV_INC_CONST, 0x01},
	{IMX477_REG_Y_ODD_INC, 0x01},
	{IMX477_REG_BINNING_MODE, 0x01},
	{IMX477_REG_BINNING_HV, 0x12},
	{IMX477_REG_BINNING_WEIGHTING, 0x02},
	{IMX477_REG8(0x3140), 0x02},
	{IMX477_REG8(0x3c00), 0x00},
	{IMX477_REG8(0x3c01), 0x03},
	{IMX477_REG8(0x3c02), 0xa2},
	{IMX477_REG_ADC_BIT_SETTING, 0x01},
	{IMX477_REG8(0x5748), 0x07},
	{IMX477_REG8(0x5749), 0xff},
	{IMX477_REG8(0x574a), 0x00},
	{IMX477_REG8(0x574b), 0x00},
	{IMX477_REG8(0x7b53), 0x01},
	{IMX477_REG8(0x9369), 0x73},
	{IMX477_REG8(0x936b), 0x64},
	{IMX477_REG8(0x936d), 0x5f},
	{IMX477_REG8(0x9304), 0x00},
	{IMX477_REG8(0x9305), 0x00},
	{IMX477_REG8(0x9e9a), 0x2f},
	{IMX477_REG8(0x9e9b), 0x2f},
	{IMX477_REG8(0x9e9c), 0x2f},
	{IMX477_REG8(0x9e9d), 0x00},
	{IMX477_REG8(0x9e9e), 0x00},
	{IMX477_REG8(0x9e9f), 0x00},
	{IMX477_REG8(0xa2a9), 0x60},
	{IMX477_REG8(0xa2b7), 0x00},
	{IMX477_REG_SCALE_MODE, 0x00},
	{IMX477_REG_SCALE_M, 0x0020},
	{IMX477_REG_DIG_CROP_X_OFFSET, 0x0000},
	{IMX477_REG_DIG_CROP_Y_OFFSET, 0x0000},
	{IMX477_REG_DIG_CROP_WIDTH, 0x0fd8},
	{IMX477_REG_DIG_CROP_HEIGHT, 0x0438},
	{IMX477_REG_X_OUT_SIZE, 1920}, // change to 1920
	{IMX477_REG_Y_OUT_SIZE, 1080},
	{IMX477_REG_IVTPXCK_DIV, 0x05},
	{IMX477_REG_IVTSYCK_DIV, 0x02},
	{IMX477_REG_IVT_PREPLLCK_DIV, 0x02},
	{IMX477_REG_IVT_PLL_MPY, 0x009B},
	{IMX477_REG_IOPPXCK_DIV, 0x0a}, //decided by output bit width
	{IMX477_REG_IOPSYCK_DIV, 0x02},
	{IMX477_REG_IOP_PREPLLCK_DIV, 0x01},
	{IMX477_REG_IOP_PLL_MPY, 100},
	{IMX477_REG_PLL_MULTI_DRIVE, 0x00},
	{IMX477_REG_REQ_LINK_BIT_RATE, 0x07080000},
	{IMX477_REG_TCLK_POST_EX, 0x007f},
	{IMX477_REG_THS_PRE_EX, 0x004f},
	{IMX477_REG_THS_ZERO_MIN, 0x0077},
	{IMX477_REG_THS_TRAIL_EX, 0x005f},
	{IMX477_REG_TCLK_TRAIL_MIN, 0x0057},
	{IMX477_REG_TCLK_PREP_EX, 0x004f},
	{IMX477_REG_TCLK_ZERO_EX, 0x0127},
	{IMX477_REG_TLPX_EX, 0x003f},
	{IMX477_REG8(0xe04c), 0x00},
	{IMX477_REG8(0xe04d), 0x7f},
	{IMX477_REG8(0xe04e), 0x00},
	{IMX477_REG8(0xe04f), 0x1f},
	{IMX477_REG8(0x3e20), 0x01},
	{IMX477_REG_PDAF_CTRL1_0, 0x00},
	{IMX477_REG_POWER_SAVE_ENABLE, 0x00},
	{IMX477_REG_LINE_LEN_INCLK, 0x016C},

	{IMX477_REG_MAP_COUPLET_CORR, 0x01},
	{IMX477_REG_SING_DYNAMIC_CORR, 0x01},
	{IMX477_REG_CIT_LSHIFT_LONG_EXP, 0x00},
	{IMX477_REG_FRAME_LEN, 1167},

	//{IMX477_REG_COARSE_INTEGRATION_TIME, mode->integration_def},

	{0},
};

static const struct video_imager_mode modes_1920x1080[] = {
	{.fps = 30, .regs = {mode_1920x1080_30}},
	{0},
};

#if 0
static const struct video_reg clk_450_mhz[] = {
	{IMX477_REG8(0x030e), 0x00},
	{IMX477_REG8(0x030f), 0x96},
	{0},
};

static const struct video_reg clk_453_mhz[] = {
	{IMX477_REG8(0x030e), 0x00},
	{IMX477_REG8(0x030f), 0x97},
	{0},
};

static const struct video_reg clk_456_mhz[] = {
	{IMX477_REG8(0x030e), 0x00},
	{IMX477_REG8(0x030f), 0x98},
	{0},
};

static const struct video_reg clk_459_mhz[] = {
	{IMX477_REG8(0x030e), 0x00},
	{IMX477_REG8(0x030f), 0x99},
	{0},
};

static const struct video_reg clk_462_mhz[] = {
	{IMX477_REG8(0x030e), 0x00},
	{IMX477_REG8(0x030f), 0x9a},
	{0},
};

static const struct video_reg clk_498_mhz[] = {
	{IMX477_REG8(0x030e), 0x00},
	{IMX477_REG8(0x030f), 0xa6},
	{0},
};
#endif

enum {
	SIZE_1920x1080,
};

static const struct video_imager_mode *modes[] = {
	[SIZE_1920x1080] = modes_1920x1080,
	NULL,
};

static const struct video_format_cap fmts[] = {
	[SIZE_1920x1080] = VIDEO_IMAGER_FORMAT_CAP(VIDEO_PIX_FMT_BGGR8, 1920, 1080),
	{0},
};

static int imx477_set_ctrl(const struct device *dev, unsigned int cid, void *val)
{
	const struct video_imager_config *cfg = dev->config;

	switch (cid) {
	case VIDEO_CID_TEST_PATTERN:
		return video_write_cci_reg(&cfg->i2c, IMX477_REG_TEST_PATTERN, (int)val);
	default:
		return -ENOTSUP;
	}
}

static int imx477_set_stream(const struct device *dev, bool on)
{
	const struct video_imager_config *cfg = dev->config;

	return video_write_cci_reg(&cfg->i2c, IMX477_REG_MODE_SEL, on ? 0x01 : 0x00);
}

static const DEVICE_API(video, imx477_driver_api) = {
	/* Implementation common to all sensors */
	.set_format = video_imager_set_fmt,
	.get_format = video_imager_get_fmt,
	.get_caps = video_imager_get_caps,
	.set_frmival = video_imager_set_frmival,
	.get_frmival = video_imager_get_frmival,
	.enum_frmival = video_imager_enum_frmival,
	/* Implementation specific to this sensor */
	.set_stream = imx477_set_stream,
	.set_ctrl = imx477_set_ctrl,
};

static int imx477_init(const struct device *dev)
{
	const struct video_imager_config *cfg = dev->config;
	uint32_t reg;
	int ret;

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("I2C device %s is not ready", cfg->i2c.bus->name);
		return -ENODEV;
	}

	k_sleep(K_MSEC(10));

	ret = video_read_cci_reg(&cfg->i2c, IMX477_REG_CHIP_ID, &reg);
	if (ret != 0) {
		LOG_ERR("Error during %s initialization: %s", dev->name, strerror(-ret));
		return ret;
	}

	if (reg != 0x0477) {
		LOG_ERR("Wrong chip ID %04x", reg);
		return -ENODEV;
	}

	LOG_INF("Detected IMX477 on %s", cfg->i2c.bus->name);

	video_stream_stop(dev);

	ret = video_imager_init(dev, init_regs, SIZE_1920x1080);
	if (ret != 0) {
		return ret;
	}

	ret = video_write_cci_reg(&cfg->i2c, IMX477_REG_SW_RESET, 0x01);
	if (ret != 0) {
		return ret;
	}

	k_sleep(K_MSEC(10));

	ret = video_write_cci_reg(&cfg->i2c, IMX477_REG_SW_RESET, 0x00);
	if (ret != 0) {
		return ret;
	}

	k_sleep(K_MSEC(10));

/*
	ret = video_write_cci_multi(&cfg->i2c, clk_450_mhz);
	if (ret != 0) {
		return ret;
	}
*/

	return 0;
}

#define IMX477_INIT(n)                                                                             \
	static struct video_imager_data imx477_data_##n;                                           \
                                                                                                   \
	static struct video_imager_config imx477_cfg_##n = {                                       \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.fmts = fmts,                                                                      \
		.modes = modes,                                                                    \
		.write_multi = &video_write_cci_multi,                                             \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &imx477_init, NULL, &imx477_data_##n, &imx477_cfg_##n,            \
			      POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY, &imx477_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IMX477_INIT)
