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

#define IMX477_FULL_WIDTH		4072
#define IMX477_FULL_HEIGHT		3176

#define IMX477_REG8(addr)		((addr) | VIDEO_REG_ADDR16_DATA8)
#define IMX477_REG16(addr)		((addr) | VIDEO_REG_ADDR16_DATA16_BE)
#define IMX477_REG24(addr)		((addr) | VIDEO_REG_ADDR16_DATA24_BE)

#define IMX477_REG_CHIP_ID		IMX477_REG16(0x0016)
#define IMX477_REG_MODE_SELECT		IMX477_REG8(0x0100)
#define IMX477_REG_SOFT_RESET		IMX477_REG8(0x0103)
#define IMX477_REG_FRAME_LENGTH		IMX477_REG16(0x0340)
#define IMX477_REG_LONG_EXP_SHIFT	IMX477_REG8(0x3100)
#define IMX477_REG_LINE_LENGTH		IMX477_REG16(0x0342)
#define IMX477_REG_TEST_PATTERN		IMX477_REG16(0x0600)
#define IMX477_REG_MC_MODE		IMX477_REG8(0x3f0b)
#define IMX477_REG_MS_SEL		IMX477_REG8(0x3041)
#define IMX477_REG_XVS_IO_CTRL		IMX477_REG8(0x3040)
#define IMX477_REG_EXTOUT_EN		IMX477_REG8(0x4b81)

static const struct video_reg init_regs[] = {
	{IMX477_REG8(0x0136), 0x18},
	{IMX477_REG8(0x0137), 0x00},
	{IMX477_REG8(0x0808), 0x02},
	{IMX477_REG8(0xe07a), 0x01},
	{IMX477_REG8(0xe000), 0x00},
	{IMX477_REG8(0x4ae9), 0x18},
	{IMX477_REG8(0x4aea), 0x08},
	{IMX477_REG8(0xf61c), 0x04},
	{IMX477_REG8(0xf61e), 0x04},
	{IMX477_REG8(0x4ae9), 0x21},
	{IMX477_REG8(0x4aea), 0x80},
	{IMX477_REG8(0x38a8), 0x1f},
	{IMX477_REG8(0x38a9), 0xff},
	{IMX477_REG8(0x38aa), 0x1f},
	{IMX477_REG8(0x38ab), 0xff},
	{IMX477_REG8(0x420b), 0x01},
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
	{IMX477_REG8(0x990c), 0x00},
	{IMX477_REG8(0x990d), 0x08},
	{IMX477_REG8(0x9956), 0x8c},
	{IMX477_REG8(0x9957), 0x64},
	{IMX477_REG8(0x9958), 0x50},
	{IMX477_REG8(0x9a48), 0x06},
	{IMX477_REG8(0x9a49), 0x06},
	{IMX477_REG8(0x9a4a), 0x06},
	{IMX477_REG8(0x9a4b), 0x06},
	{IMX477_REG8(0x9a4c), 0x06},
	{IMX477_REG8(0x9a4d), 0x06},
	{IMX477_REG8(0xa001), 0x0a},
	{IMX477_REG8(0xa003), 0x0a},
	{IMX477_REG8(0xa005), 0x0a},
	{IMX477_REG8(0xa006), 0x01},
	{IMX477_REG8(0xa007), 0xc0},
	{IMX477_REG8(0xa009), 0xc0},
	{IMX477_REG8(0x4bd5), 0x16},
	{IMX477_REG8(0x3d8a), 0x01},
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

	/* VSYNC trigger mode: stand-alone */
	{IMX477_REG_MC_MODE, false},
	{IMX477_REG_MS_SEL, true},
	{IMX477_REG_XVS_IO_CTRL, false},
	{IMX477_REG_EXTOUT_EN, false},

	{0}
};

static const struct video_reg clk_450_mhz[] = {
	{IMX477_REG8(0x030e), 0x00},
	{IMX477_REG8(0x030f), 0x96},
	{0},
};

#if 0
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

static const struct video_reg mode_1920x1080_60fps[] = {
	{IMX477_REG8(0x0112), 0x0a},
	{IMX477_REG8(0x0113), 0x0a},
	{IMX477_REG8(0x0114), 0x01},
	{IMX477_REG8(0x0342), 0x1b},
	{IMX477_REG8(0x0343), 0x58},
	{IMX477_REG8(0x0340), 0x07},
	{IMX477_REG8(0x0341), 0xd0},
	{IMX477_REG8(0x0344), 0x00},
	{IMX477_REG8(0x0345), 0x00},
	{IMX477_REG8(0x0346), 0x01},
	{IMX477_REG8(0x0347), 0xb8},
	{IMX477_REG8(0x0348), 0x0f},
	{IMX477_REG8(0x0349), 0xd7},
	{IMX477_REG8(0x034a), 0x0a},
	{IMX477_REG8(0x034b), 0x27},
	{IMX477_REG8(0x00e3), 0x00},
	{IMX477_REG8(0x00e4), 0x00},
	{IMX477_REG8(0x00fc), 0x0a},
	{IMX477_REG8(0x00fd), 0x0a},
	{IMX477_REG8(0x00fe), 0x0a},
	{IMX477_REG8(0x00ff), 0x0a},
	{IMX477_REG8(0x0220), 0x00},
	{IMX477_REG8(0x0221), 0x11},
	{IMX477_REG8(0x0381), 0x01},
	{IMX477_REG8(0x0383), 0x01},
	{IMX477_REG8(0x0385), 0x01},
	{IMX477_REG8(0x0387), 0x01},
	{IMX477_REG8(0x0900), 0x01},
	{IMX477_REG8(0x0901), 0x22},
	{IMX477_REG8(0x0902), 0x02},
	{IMX477_REG8(0x3140), 0x02},
	{IMX477_REG8(0x3c00), 0x00},
	{IMX477_REG8(0x3c01), 0x01},
	{IMX477_REG8(0x3c02), 0x9c},
	{IMX477_REG8(0x3f0d), 0x00},
	{IMX477_REG8(0x5748), 0x00},
	{IMX477_REG8(0x5749), 0x00},
	{IMX477_REG8(0x574a), 0x00},
	{IMX477_REG8(0x574b), 0xa4},
	{IMX477_REG8(0x7b75), 0x0e},
	{IMX477_REG8(0x7b76), 0x09},
	{IMX477_REG8(0x7b77), 0x08},
	{IMX477_REG8(0x7b78), 0x06},
	{IMX477_REG8(0x7b79), 0x34},
	{IMX477_REG8(0x7b53), 0x00},
	{IMX477_REG8(0x9369), 0x73},
	{IMX477_REG8(0x936b), 0x64},
	{IMX477_REG8(0x936d), 0x5f},
	{IMX477_REG8(0x9304), 0x03},
	{IMX477_REG8(0x9305), 0x80},
	{IMX477_REG8(0x9e9a), 0x2f},
	{IMX477_REG8(0x9e9b), 0x2f},
	{IMX477_REG8(0x9e9c), 0x2f},
	{IMX477_REG8(0x9e9d), 0x00},
	{IMX477_REG8(0x9e9e), 0x00},
	{IMX477_REG8(0x9e9f), 0x00},
	{IMX477_REG8(0xa2a9), 0x27},
	{IMX477_REG8(0xa2b7), 0x03},
	{IMX477_REG8(0x0401), 0x00},
	{IMX477_REG8(0x0404), 0x00},
	{IMX477_REG8(0x0405), 0x10},
	{IMX477_REG8(0x0408), 0x00},
	{IMX477_REG8(0x0409), 0x36},
	{IMX477_REG8(0x040a), 0x00},
	{IMX477_REG8(0x040b), 0x00},
	{IMX477_REG8(0x040c), 0x07},
	{IMX477_REG8(0x040d), 0x80},
	{IMX477_REG8(0x040e), 0x04},
	{IMX477_REG8(0x040f), 0x38},
	{IMX477_REG8(0x034c), 0x07},
	{IMX477_REG8(0x034d), 0x80},
	{IMX477_REG8(0x034e), 0x04},
	{IMX477_REG8(0x034f), 0x38},
	{IMX477_REG8(0x0301), 0x05},
	{IMX477_REG8(0x0303), 0x02},
	{IMX477_REG8(0x0305), 0x02},
	{IMX477_REG8(0x0306), 0x00},
	{IMX477_REG8(0x0307), 0xaf},
	{IMX477_REG8(0x0309), 0x0a},
	{IMX477_REG8(0x030b), 0x01},
	{IMX477_REG8(0x030d), 0x02},
	{IMX477_REG8(0x030e), 0x00},
	{IMX477_REG8(0x030f), 0x7d},
	{IMX477_REG8(0x0310), 0x01},
	{IMX477_REG8(0x0820), 0x0b},
	{IMX477_REG8(0x0821), 0xb8},
	{IMX477_REG8(0x0822), 0x00},
	{IMX477_REG8(0x0823), 0x00},
	{IMX477_REG8(0x080a), 0x00},
	{IMX477_REG8(0x080b), 0x97},
	{IMX477_REG8(0x080c), 0x00},
	{IMX477_REG8(0x080d), 0x5f},
	{IMX477_REG8(0x080e), 0x00},
	{IMX477_REG8(0x080f), 0x9f},
	{IMX477_REG8(0x0810), 0x00},
	{IMX477_REG8(0x0811), 0x6f},
	{IMX477_REG8(0x0812), 0x00},
	{IMX477_REG8(0x0813), 0x6f},
	{IMX477_REG8(0x0814), 0x00},
	{IMX477_REG8(0x0815), 0x57},
	{IMX477_REG8(0x0816), 0x01},
	{IMX477_REG8(0x0817), 0x87},
	{IMX477_REG8(0x0818), 0x00},
	{IMX477_REG8(0x0819), 0x4f},
	{IMX477_REG8(0xe04c), 0x00},
	{IMX477_REG8(0xe04d), 0x9f},
	{IMX477_REG8(0xe04e), 0x00},
	{IMX477_REG8(0xe04f), 0x1f},
	{IMX477_REG8(0x3e20), 0x01},
	{IMX477_REG8(0x3e37), 0x00},
	{IMX477_REG8(0x3f50), 0x00},
	{IMX477_REG8(0x3f56), 0x00},
	{IMX477_REG8(0x3f57), 0xc8},
	{IMX477_REG8(0x3ff9), 0x01},
	//{VIDEO_REG_WAIT_MS, 1},
	{0}
};

static const struct video_imager_mode modes_1920x1080[] = {
	{.fps = 60, .regs = {mode_1920x1080_60fps}},
	{0},
};

enum {
	SIZE_1332x990,
};

static const struct video_imager_mode *modes[] = {
<<<<<<< HEAD
	modes_1920x1080,
=======
	[SIZE_1332x990] = modes_1332x990,
>>>>>>> 628a27c98dc (drivers: video: imx*: avoid a config pitfall)
	NULL,
};

static const struct video_format_cap fmts[] = {
<<<<<<< HEAD
	VIDEO_IMAGER_FORMAT_CAP(VIDEO_PIX_FMT_BGGR8, 1920, 1080),
=======
	[SIZE_1332x990] = VIDEO_IMAGER_FORMAT_CAP(VIDEO_PIX_FMT_BGGR8, 1332, 990),
>>>>>>> 628a27c98dc (drivers: video: imx*: avoid a config pitfall)
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

	return video_write_cci_reg(&cfg->i2c, IMX477_REG_MODE_SELECT, on ? 0x01 : 0x00);
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

	ret = video_imager_init(dev, init_regs, 0);
	if (ret != 0) {
		return ret;
	}

	ret = video_write_cci_reg(&cfg->i2c, IMX477_REG_SOFT_RESET, 0x01);
	if (ret != 0) {
		return ret;
	}

	k_sleep(K_MSEC(10));

	ret = video_write_cci_multi(&cfg->i2c, clk_450_mhz);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

#define IMX477_INIT(n)                                                                             \
	static struct video_imager_data imx477_data_##n;                                           \
                                                                                                   \
	static struct video_imager_config imx477_cfg_##n = {                                       \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.fmts = fmts,                                                                      \
		.modes = modes,                                                                    \
		.data = &imx477_data_##n,                                                          \
		.write_multi = &video_write_cci_multi,                                             \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &imx477_init, NULL, NULL, &imx477_cfg_##n, POST_KERNEL,           \
			      CONFIG_VIDEO_INIT_PRIORITY, &imx477_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IMX477_INIT)
