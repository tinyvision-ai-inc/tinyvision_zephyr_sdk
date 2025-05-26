/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_VIDEO_IMAGER_H_
#define ZEPHYR_DRIVERS_VIDEO_IMAGER_H_

#include <stddef.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/i2c.h>

/**
 * @defgroup video_imager Video Imager (image sensor) shared implementation
 *
 * This API is targeting image sensor driver developers.
 *
 * It provides a common implementation that only requires implementing a table of
 * @ref video_imager_mode that lists the various resolution, frame rates and associated I2C
 * configuration registers.
 *
 * The @c video_imager_... functions can be submitted directly to the @rev video_api.
 * If a driver also needs to do extra work before or after applying a mode, it is possible
 * to provide a custom wrapper or skip these default implementation altogether.
 *
 * @{
 */

/**
 * @brief Table entry for an imaging mode of the sensor device.
 *
 * AA mode can be applied to an imager to configure a particular framerate, resolution, and pixel
 * format. The index of the table of modes is meant to match the index of the table of formats.
 */
struct video_imager_mode {
	/* FPS for this mode */
	uint16_t fps;
	/* Multiple lists of registers to allow sharing common sets of registers across modes. */
	struct {
		const struct video_reg *values;
		/* Number of registers in this table */
		size_t nb;
	} regs[8];
};

/**
 * @brief A video imager device is expected to have dev->data point to this structure.
 *
 * In order to support custom data structure, it is possible to store an extra pointer
 * in the dev->config struct. See existing drivers for an example.
 */
struct video_imager_config {
	/** List of all formats supported by this sensor */
	const struct video_format_cap *fmts;
	/** Array of modes tables, one table per format cap lislted by "fmts" */
	const struct video_imager_mode **modes;
	/** I2C device to write the registers to */
	struct i2c_dt_spec i2c;
	/** Write a table of registers onto the device */
	int (*write_multi)(const struct i2c_dt_spec *i2c, const struct video_reg *regs, size_t sz);
};

/**
 * @brief A video imager device is expected to have dev->data point to this structure.
 *
 * In order to support custom data structure, it is possible to store an extra pointer
 * in the dev->config struct. See existing drivers for an example.
 */
struct video_imager_data {
	/** Index of the currently active format in both modes[] and fmts[] */
	int fmt_id;
	/** Currently active video format */
	struct video_format fmt;
	/** Currently active operating mode as defined above */
	const struct video_imager_mode *mode;
};

/**
 * @brief Initialize a register table part of @reg video_imager_config.
 *
 * @ref ARRAY_SIZE() will be called on @p arr to get the number of registers.
 *
 * @param arr Array of regsiters, must not be a pointer.
 */
#define VIDEO_IMAGER_REGS(arr) {.values = (arr), .nb = ARRAY_SIZE(arr)}

/**
 * @brief Initialize one row of a @struct video_format_cap with fixed width and height.
 *
 * The minimum and maximum are the same for both width and height fields.
 * @param
 */
#define VIDEO_IMAGER_FORMAT_CAP(pixfmt, width, height)                                             \
	{                                                                                          \
		.width_min = (width), .width_max = (width), .width_step = 0,                       \
		.height_min = (height), .height_max = (height), .height_step = 0,                  \
		.pixelformat = (pixfmt),                                                           \
	}

/**
 * @brief Set the operating mode of the imager as defined in @ref video_imager_mode.
 *
 * If the default immplementation for the video API are used, there is no need to explicitly call
 * this function in the image sensor driver.
 *
 * @param dev Device that has a struct video_imager in @c dev->data.
 * @param mode The mode to apply to the image sensor.
 * @return 0 if successful, or negative error number otherwise.
 */
int video_imager_set_mode(const struct device *dev, const struct video_imager_mode *mode);

/** @brief Default implementation for image drivers frame interval selection */
int video_imager_set_frmival(const struct device *dev, struct video_frmival *frmival);
/** @brief Default implementation for image drivers frame interval query */
int video_imager_get_frmival(const struct device *dev, struct video_frmival *frmival);
/** @brief Default implementation for image drivers frame interval enumeration */
int video_imager_enum_frmival(const struct device *dev, struct video_frmival_enum *fie);
/** @brief Default implementation for image drivers format selection */
int video_imager_set_fmt(const struct device *const dev, struct video_format *fmt);
/** @brief Default implementation for image drivers format query */
int video_imager_get_fmt(const struct device *dev, struct video_format *fmt);
/** @brief Default implementation for image drivers format capabilities */
int video_imager_get_caps(const struct device *dev, struct video_caps *caps);

/**
 * Initialize an imager by loading init_regs onto the device, and setting the default format.
 *
 * @param dev Device that has a struct video_imager in @c dev->data.
 * @param init_regs If non-NULL, table of registers to configure at init.
 * @param default_fmt_idx Default format index to apply at init.
 * @return 0 if successful, or negative error number otherwise.
 */
int video_imager_init(const struct device *dev, int default_fmt_idx);

/** @} */

#endif /* ZEPHYR_DRIVERS_VIDEO_IMAGER_H_ */
