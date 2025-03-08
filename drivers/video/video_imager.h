/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_VIDEO_IMAGER_H
#define ZEPHYR_INCLUDE_DRIVERS_VIDEO_IMAGER_H

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>

/* Used by register tables */
#define U8(addr, reg)	{(addr), (reg)}
#define U16(addr, reg)	{(addr), (reg) >> 8}, {(addr) + 1, (reg)}

/*
 * Entry for an "imaging mode", as a combination of framereate, resolution, and pixel format.
 */
struct video_imager_mode {
	/* FPS for this mode */
	uint16_t fps;
	/* Multiple lists of registers to allow sharing common sets of registers across modes. */
	const void *regs[3];
};

/*
 * A video imager device is expected to have dev->data point to this structure.
 * In order to support custom data structure, it is possible to store an extra pointer
 * in the dev->config struct. See existing drivers for an example.
 */
struct video_imager_data {
	/* Index of the currently active format through both modes and formats */
	int fmt_id;
	/* Currently active video format */
	struct video_format fmt;
	/* List of all formats supported by this sensor */
	const struct video_format_cap *fmts;
	/* Currently active operating mode as defined above */
	const struct video_imager_mode *mode;
	/* Array of modes tables, one table per format cap lislted by "fmts" */
	const struct video_imager_mode **modes;
	/* I2C device to write the registers to */
	struct i2c_dt_spec *i2c;
	/* Function in charge of writing a table of register to the sensor */
	int (*write_multi_fn)(const struct device *dev, const void *register_table);
};

/*
 * Type used by register tables that have either the address or value 16-bit wide.
 */
struct video_imager_reg16 {
	uint16_t addr;
	uint16_t value;
};

/*
 * Type used by register tables that have either the address or value both 8-bit wide.
 */
struct video_imager_reg8 {
	uint8_t addr;
	uint8_t value;
};

/*
 * Video device capabilities where the supported resolutions and pixel formats are listed.
 * The format ID is used as index to fetch the matching mode from the list above.
 */
#define VIDEO_IMAGER_FORMAT_CAP(width, height, format)                                             \
	{                                                                                          \
		.pixelformat = (format), .width_min = (width), .width_max = (width),               \
		.height_min = (height), .height_max = (height), .width_step = 0, .height_step = 0, \
	}

/*
 * Functions to write a table of I2C values, with several variants to support the different sizes
 * of address and values.
 */
int video_imager_reg16_read8(const struct device *dev, uint16_t reg_addr, uint8_t *reg_value);
int video_imager_reg16_read16(const struct device *dev, uint16_t reg_addr, uint16_t *reg_value);
int video_imager_reg16_write8(const struct device *dev, uint16_t reg_addr, uint8_t reg_value);
int video_imager_reg16_write16(const struct device *dev, uint16_t reg_addr, uint16_t reg_value);
int video_imager_reg16_write8_multi(const struct device *dev, const void *regs);

/*
 * Function which will set the operating mode (as defined above) of the imager.
 */
int video_imager_set_mode(const struct device *dev, const struct video_imager_mode *mode);

/*
 * Default implementations for the video API that can be passed directly to the "struct video_api"
 * or called from a custom wrapper to insert extra actions, or ignored to provide a fully custom
 * implementation.
 */
int video_imager_set_frmival(const struct device *dev, enum video_endpoint_id ep,
			     struct video_frmival *frmival);
int video_imager_get_frmival(const struct device *dev, enum video_endpoint_id ep,
			     struct video_frmival *frmival);
int video_imager_enum_frmival(const struct device *dev, enum video_endpoint_id ep,
			      struct video_frmival_enum *fie);
int video_imager_set_fmt(const struct device *const dev, enum video_endpoint_id ep,
			 struct video_format *fmt);
int video_imager_get_fmt(const struct device *dev, enum video_endpoint_id ep,
			 struct video_format *fmt);
int video_imager_get_caps(const struct device *dev, enum video_endpoint_id ep,
			  struct video_caps *caps);

/*
 * Initialize an imager and its associated data structure, loading init_regs onto the device,
 * and then initializing the format at position 0.
 */
int video_imager_init(const struct device *dev, const void *init_regs);

#endif /* ZEPHYR_INCLUDE_DRIVERS_VIDEO_IMAGER_H */
