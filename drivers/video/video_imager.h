/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_VIDEO_IMAGER_H
#define ZEPHYR_INCLUDE_DRIVERS_VIDEO_IMAGER_H

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>

/**
 * Type used by register tables that have either the address or value 16-bit wide.
 */
struct video_reg {
	/** Address of the register to write to as well as  */
	uint32_t addr;
	/** Value to write in this address */
	uint32_t data;
};

/**
 * Entry for an "imaging mode", as a combination of framereate, resolution, and pixel format.
 */
struct video_imager_mode {
	/* FPS for this mode */
	uint16_t fps;
	/* Multiple lists of registers to allow sharing common sets of registers across modes. */
	const struct video_reg *regs[3];
};

/**
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
};

/**
 * @brief Provide one row of a @struct video_format_cap with fixed width and height
 *
 * This initializes the fields of one entry of a format capabilities array, where the minimum and
 * maximum are the same for both width and height fields. The step is set to 0 as it is not
 * possible to step through multiple values.
 *
 * @parma pixfmt The pixel format for that entire entry. @c VIDEO_PIX_FMT_...
 * @param width The width of the image frame in number of pixels.
 * @param height The height of the image frame in number of pixels.
 */
#define VIDEO_IMAGER_FORMAT_CAP(pixfmt, width, height)                                             \
	{                                                                                          \
		.width_min = (width), .width_max = (width), .width_step = 0,                       \
		.height_min = (height), .height_max = (height), .height_step = 0,                  \
		.pixelformat = (pixfmt),                                                           \
	}

/**
 * @brief Encode the size of the register into the unused bits of the address.
 *
 * This permits to encode the register size information as used by @ref video_read_reg(),
 * @ref videoo_write_reg(), @ref video_write_multi().
 *
 * The size of the address and the size of the register value are encoded in the macro name to
 * make the driver source more intuitive to read.
 *
 * The Linux @c CCI_REG8() macro is equivalent to the Zephyr @ref VIDEO_ADDR16_REG8() macro.
 *
 * Example usage:
 *
 * @code{.c}
 * #define IMX219_BINNING_MODE_H VIDEO_ADDR16_REG8(0x0174)
 * @endcode
 *
 * @param addr The address to which add the size information.
 * @param addr_size Number of address bytes. Possible values: 1, 2.
 * @param data_size Number of data bytes following the address. Possible values: 1, 2, 3, 4
 */
#define VIDEO_REG(addr, addr_size, data_size)                                                      \
	(FIELD_PREP(VIDEO_REG_ADDR_SIZE_MASK, (addr_size)) |                                       \
	 FIELD_PREP(VIDEO_REG_DATA_SIZE_MASK, (data_size)) | (addr))

#define VIDEO_REG_DATA_SIZE_MASK GENMASK(19, 16)
#define VIDEO_REG_ADDR_SIZE_MASK GENMASK(21, 20)

/**
 * @brief Write a register value to the specified register address and size.
 *
 * The size of the address and value are both encoded in the unused bits of the address by macros
 * such as @ref VIDEO_ADDR16_REG8().
 *
 * @brief i2c Reference to the video device on an I2C bus.
 * @brief reg_addr Address of the register to fill with @reg_value along with size information.
 * @brief reg_value Value to write at this address, the size to write is encoded in the address.
 */
int video_write_reg(struct i2c_dt_spec *i2c, uint32_t reg_addr, uint32_t reg_value);

/**
 * @brief Read a register value from the specified register address and size.
 *
 * The size of the address and value are both encoded in the unused bits of the address by macros
 * such as @ref VIDEO_ADDR16_REG8().
 *
 * @brief i2c Reference to the video device on an I2C bus.
 * @brief reg_addr Address of the register to fill with @reg_value along with size information.
 * @brief reg_value Value to write at this address, the size to write is encoded in the address.
 *                  This is a 32-bit integer pointer even when reading 8-bit or 16 bits value.
 */
int video_read_reg(struct i2c_dt_spec *i2c, uint32_t reg_addr, uint32_t *reg_value);

/**
 * @brief Write a complete table of registers to a device one by one.
 *
 * The address present in the registers need to be encoding the size information using the macros
 * such as @ref VIDEO_ADDR16_REG8(). The last element must be empty (@c {0}) to mark the end of the
 * table.
 *
 * @brief i2c Reference to the video device on an I2C bus.
 * @brief regs Array of address/value pairs to write to the device sequentially.
 */
int video_write_multi(struct i2c_dt_spec *i2c, const struct video_reg *regs);

/**
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
int video_imager_init(const struct device *dev, const struct video_reg *init_regs,
		      int default_fmt_idx);

#endif /* ZEPHYR_INCLUDE_DRIVERS_VIDEO_IMAGER_H */
