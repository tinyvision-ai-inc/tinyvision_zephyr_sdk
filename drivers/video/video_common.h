/*
 * Copyright (c) 2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_VIDEO_COMMON_H_
#define ZEPHYR_DRIVERS_VIDEO_COMMON_H_

#include <stddef.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/i2c.h>

/**
 * Type used by register tables that have either the address or value 16-bit wide.
 */
struct video_reg {
	/** Address of the register to write to as well as  */
	uint32_t addr;
	/** Value to write in this address */
	uint32_t data;
};

/*
 * Table entry for an "imaging mode" that can be applied to an imager to configure a particular
 * framerate, resolution, and pixel format. The index of the table of modes is meant to match
 * the index of the table of formats.
 */
struct video_imager_mode {
	/* FPS for this mode */
	uint16_t fps;
	/* Multiple lists of registers to allow sharing common sets of registers across modes. */
	const struct video_reg *regs[3];
};

/*
 * A video imager device is expected to have dev->data point to this structure.
 * In order to support custom data structure, it is possible to store an extra pointer
 * in the dev->config struct. See existing drivers for an example.
 */
struct video_imager_data {
	/* Index of the currently active format in both modes[] and fmts[] */
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
	struct i2c_dt_spec i2c;
	/* Write a table of registers onto the device */
	int (*write_multi)(struct i2c_dt_spec *i2c, const struct video_reg *regs);
};

/*
 * Initialize one row of a @struct video_format_cap with fixed width and height
 * The minimum and maximum are the same for both width and height fields.
 */
#define VIDEO_IMAGER_FORMAT_CAP(pixfmt, width, height)                                             \
	{                                                                                          \
		.width_min = (width), .width_max = (width), .width_step = 0,                       \
		.height_min = (height), .height_max = (height), .height_step = 0,                  \
		.pixelformat = (pixfmt),                                                           \
	}

/**
 * @defgroup video_cci Video Register Control Interface (CCI)
 *
 * The Camera Control Interface (CCI) is an I2C communication scheme part of MIPI-CSI.
 * It allows register addresses 8-bit or 16-bit wide, and register data 8-bit wide only.
 * Larger registers data size can be split across multiple write operations.
 *
 * Together, macros to define metadata in register addresses, and functions that can use this
 * information are provided to facilitate drivers development.
 *
 * @{
 */

#define VIDEO_REG_ENDIANNESS_MASK		GENMASK(24, 24)
#define VIDEO_REG_ADDR_SIZE_MASK		GENMASK(23, 20)
#define VIDEO_REG_DATA_SIZE_MASK		GENMASK(19, 16)
#define VIDEO_REG_ADDR_MASK			GENMASK(15, 0)

/**
 * @brief Encode the size of the register into the unused bits of the address.
 *
 * This permits to encode the register size information as used by @ref video_read_cci_reg(),
 * @ref video_write_cci_reg(), @ref video_write_cci_multi().
 *
 * The size of the address and the size of the register value are encoded in the macro name to
 * make the driver source more intuitive to read.
 *
 * Linux's @c REG8(addr) is equivalent to Zephyr's @ref (addr | VIDEO_ADDR16_REG8).
 *
 * Example usage:
 *
 * @code{.c}
 * #define IMX219_REG16(addr) ((addr) | VIDEO_ADDR16_DATA16_LE)
 * @endcode
 *
 * @param addr The address to which add the size information.
 * @param addr_size Number of address bytes. Possible values: 1, 2.
 * @param data_size Number of data bytes following the address. Possible values: 1, 2, 3, 4
 * @param endianness Endianness of the data sent over I2C after the address.
 */
#define VIDEO_REG(addr_size, data_size, endianness)                                                \
	(FIELD_PREP(VIDEO_REG_ADDR_SIZE_MASK, (addr_size)) |                                       \
	 FIELD_PREP(VIDEO_REG_DATA_SIZE_MASK, (data_size)) |                                       \
	 FIELD_PREP(VIDEO_REG_ENDIANNESS_MASK, (endianness)))

#define VIDEO_REG_ADDR8_DATA8		VIDEO_REG(1, 1, false)
#define VIDEO_REG_ADDR8_DATA16_LE	VIDEO_REG(1, 2, false)
#define VIDEO_REG_ADDR8_DATA16_BE	VIDEO_REG(1, 2, true)
#define VIDEO_REG_ADDR8_DATA24_LE	VIDEO_REG(1, 3, false)
#define VIDEO_REG_ADDR8_DATA24_BE	VIDEO_REG(1, 3, true)
#define VIDEO_REG_ADDR8_DATA32_LE	VIDEO_REG(1, 4, false)
#define VIDEO_REG_ADDR8_DATA32_BE	VIDEO_REG(1, 4, true)
#define VIDEO_REG_ADDR16_DATA8		VIDEO_REG(2, 1, false)
#define VIDEO_REG_ADDR16_DATA16_LE	VIDEO_REG(2, 2, false)
#define VIDEO_REG_ADDR16_DATA16_BE	VIDEO_REG(2, 2, true)
#define VIDEO_REG_ADDR16_DATA24_LE	VIDEO_REG(2, 3, false)
#define VIDEO_REG_ADDR16_DATA24_BE	VIDEO_REG(2, 3, true)
#define VIDEO_REG_ADDR16_DATA32_LE	VIDEO_REG(2, 4, false)
#define VIDEO_REG_ADDR16_DATA32_BE	VIDEO_REG(2, 4, true)

/**
 * @brief Write a register value to the specified register address and size.
 *
 * The size of the register address and register data passed as flags in the high bits of
 * @p reg_addrin the unused bits of the
 * address.
 *
 * @brief i2c Reference to the video device on an I2C bus.
 * @brief reg_addr Address of the register to fill with @reg_value along with size information.
 * @brief reg_value Value to write at this address, the size to write is encoded in the address.
 *
 * @{
 */
/** Camera Control Interface (CCI): the data is sent 8-bit at a time in multiple transactions */
int video_write_cci_reg(struct i2c_dt_spec *i2c, uint32_t reg_addr, uint32_t reg_value);
/** Variant where the registers is sent in a single I2C transaction */
int video_write_i2c_reg(struct i2c_dt_spec *i2c, uint32_t reg_addr, uint32_t reg_value);
/** @} */

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
 *
 * @{
 */
/** Camera Control Interface (CCI): the data is sent 8-bit at a time in multiple transactions */
int video_read_cci_reg(struct i2c_dt_spec *i2c, uint32_t reg_addr, uint32_t *reg_value);
/** Variant where the registers is sent in a single I2C transaction */
int video_read_i2c_reg(struct i2c_dt_spec *i2c, uint32_t reg_addr, uint32_t *reg_value);
/** @} */

/**
 * @brief Write a complete table of registers to a device one by one.
 *
 * The address present in the registers need to be encoding the size information using the macros
 * such as @ref VIDEO_ADDR16_REG8(). The last element must be empty (@c {0}) to mark the end of the
 * table.
 *
 * @brief i2c Reference to the video device on an I2C bus.
 * @brief regs Array of address/value pairs to write to the device sequentially.
 *
 * @{
 */
/** Camera Control Interface (CCI): the data is sent 8-bit at a time in multiple transactions */
int video_write_cci_multi(struct i2c_dt_spec *i2c, const struct video_reg *regs);
/** Variant where the registers is sent in a single I2C transaction */
int video_write_i2c_multi(struct i2c_dt_spec *i2c, const struct video_reg *regs);
/** @} */

/*
 * Function which will set the operating mode (as defined above) of the imager.
 */
int video_imager_set_mode(const struct device *dev, const struct video_imager_mode *mode);

/*
 * Default implementations that can be passed directly to the struct video_api, or called by the
 * driver implementation.
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
 * Initialize an imager by loading init_regs onto the device, and setting the default format.
 */
int video_imager_init(const struct device *dev, const struct video_reg *init_regs,
		      int default_fmt_idx);

#endif /* ZEPHYR_DRIVERS_VIDEO_COMMON_H_ */
