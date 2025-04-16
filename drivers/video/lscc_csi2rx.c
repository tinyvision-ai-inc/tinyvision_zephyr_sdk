/*
 * Copyright (c) 2024-2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT lattice_csi2rx

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/video.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(csi2rx, CONFIG_VIDEO_LOG_LEVEL);

/* tinyVision.ai quirk: use 32-bit addresses: 2 lower address bits are always 0 */
#define LSCC_CSI2RX_REG(addr)			((addr) << 2)
#define LSCC_CSI2RX_WRITE(val, addr)		sys_write32((val), (addr))
#define LSCC_CSI2RX_READ(addr)			sys_read32((addr))

#define LSCC_CSI2RX_REG_LANE_SETTING		LSCC_CSI2RX_REG(0x0A)

#define LSCC_CSI2RX_REG_VC_DT			LSCC_CSI2RX_REG(0x1F)
#define LSCC_CSI2RX_REG_WCL			LSCC_CSI2RX_REG(0x20)
#define LSCC_CSI2RX_REG_WCH			LSCC_CSI2RX_REG(0x21)
#define LSCC_CSI2RX_REG_ECC			LSCC_CSI2RX_REG(0x22)

#define LSCC_CSI2RX_REG_VC_DT2			LSCC_CSI2RX_REG(0x23)
#define LSCC_CSI2RX_REG_WC2L			LSCC_CSI2RX_REG(0x24)
#define LSCC_CSI2RX_REG_WC2H			LSCC_CSI2RX_REG(0x25)
#define LSCC_CSI2RX_REG_ECC2			LSCC_CSI2RX_REG(0x26)

#define LSCC_CSI2RX_REG_REFDT			LSCC_CSI2RX_REG(0x27)
#define LSCC_CSI2RX_VAL_REFDT_RAW6		0x28
#define LSCC_CSI2RX_VAL_REFDT_RAW7		0x29
#define LSCC_CSI2RX_VAL_REFDT_RAW8		0x2A
#define LSCC_CSI2RX_VAL_REFDT_RAW10		0x2B
#define LSCC_CSI2RX_VAL_REFDT_RAW12		0x2C
#define LSCC_CSI2RX_VAL_REFDT_RAW14		0x2D

#define LSCC_CSI2RX_REG_ERROR_STATUS		LSCC_CSI2RX_REG(0x28)
#define LSCC_CSI2RX_REG_ERROR_STATUS_EN		LSCC_CSI2RX_REG(0x29)
#define LSCC_CSI2RX_REG_CRC_BYTE_LOW		LSCC_CSI2RX_REG(0x30)
#define LSCC_CSI2RX_REG_CRC_BYTE_HIGH		LSCC_CSI2RX_REG(0x31)
#define LSCC_CSI2RX_REG_ERROR_CTRL		LSCC_CSI2RX_REG(0x32)
#define LSCC_CSI2RX_REG_ERROR_HS_SOT		LSCC_CSI2RX_REG(0x33)
#define LSCC_CSI2RX_REG_ERROR_HS_SOT_SYNC	LSCC_CSI2RX_REG(0x34)
#define LSCC_CSI2RX_REG_CONTROL			LSCC_CSI2RX_REG(0x35)
#define LSCC_CSI2RX_REG_NOCIL_DSETTLE		LSCC_CSI2RX_REG(0x36)
#define LSCC_CSI2RX_REG_NOCIL_RXFIFODEL_LSB	LSCC_CSI2RX_REG(0x37)
#define LSCC_CSI2RX_REG_NOCIL_RXFIFODEL_MSB	LSCC_CSI2RX_REG(0x38)
#define LSCC_CSI2RX_REG_ERROR_SOT_SYNC_DET	LSCC_CSI2RX_REG(0x39)


struct lscc_csi2rx_config {
	const struct device *source_dev;
	uint32_t base;
};

struct lscc_csi2rx_reg {
	uint32_t addr;
	const char *name;
	const char *description;
};

static int lscc_csi2rx_init(const struct device *dev)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	/* Setup the default Reference Data Type to RAW8 to only allow RAW8 data in by default */
	LSCC_CSI2RX_WRITE(LSCC_CSI2RX_VAL_REFDT_RAW8, cfg->base + LSCC_CSI2RX_REG_REFDT);

	/* Set the default settle time */
	LSCC_CSI2RX_WRITE(0x06, cfg->base + LSCC_CSI2RX_REG_NOCIL_DSETTLE);

	return 0;
}

static int lscc_csi2rx_set_format(const struct device *dev, enum video_endpoint_id ep,
			      struct video_format *fmt)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return video_set_format(cfg->source_dev, VIDEO_EP_OUT, fmt);
}

static int lscc_csi2rx_get_format(const struct device *dev, enum video_endpoint_id ep,
			      struct video_format *fmt)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return video_get_format(cfg->source_dev, VIDEO_EP_OUT, fmt);
}

static int lscc_csi2rx_get_caps(const struct device *dev, enum video_endpoint_id ep,
			    struct video_caps *caps)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return video_get_caps(cfg->source_dev, VIDEO_EP_OUT, caps);
}

static int lscc_csi2rx_set_frmival(const struct device *dev, enum video_endpoint_id ep,
			       struct video_frmival *frmival)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return video_set_frmival(cfg->source_dev, VIDEO_EP_OUT, frmival);
}

static int lscc_csi2rx_get_frmival(const struct device *dev, enum video_endpoint_id ep,
			       struct video_frmival *frmival)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return video_get_frmival(cfg->source_dev, VIDEO_EP_OUT, frmival);
}

static int lscc_csi2rx_enum_frmival(const struct device *dev, enum video_endpoint_id ep,
				   struct video_frmival_enum *fie)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return video_enum_frmival(cfg->source_dev, VIDEO_EP_OUT, fie);
}

static int lscc_csi2rx_set_stream(const struct device *dev, bool on)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return on ? video_stream_start(cfg->source_dev) : video_stream_stop(cfg->source_dev);

}

static int lscc_csi2rx_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return video_set_ctrl(cfg->source_dev, cid, value);
}

static int lscc_csi2rx_get_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct lscc_csi2rx_config *cfg = dev->config;

	return video_get_ctrl(cfg->source_dev, cid, value);
}

static const DEVICE_API(video, lscc_csi2rx_driver_api) = {
	.set_format = lscc_csi2rx_set_format,
	.get_format = lscc_csi2rx_get_format,
	.get_caps = lscc_csi2rx_get_caps,
	.set_frmival = lscc_csi2rx_set_frmival,
	.get_frmival = lscc_csi2rx_get_frmival,
	.enum_frmival = lscc_csi2rx_enum_frmival,
	.set_stream = lscc_csi2rx_set_stream,
	.set_ctrl = lscc_csi2rx_set_ctrl,
	.get_ctrl = lscc_csi2rx_get_ctrl,
};

#define SRC_EP(inst) DT_INST_ENDPOINT_BY_ID(inst, 0, 0)

#define LSCC_CSI2RX_DEVICE_DEFINE(inst)                                                            \
	const struct lscc_csi2rx_config lscc_csi2rx_cfg_##inst = {                                 \
		.source_dev = DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(SRC_EP(inst))),                  \
		.base = DT_INST_REG_ADDR(inst),                                                    \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, lscc_csi2rx_init, NULL, NULL, &lscc_csi2rx_cfg_##inst,         \
			      POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY, &lscc_csi2rx_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LSCC_CSI2RX_DEVICE_DEFINE)

static bool device_is_video_and_ready(const struct device *dev)
{
	return device_is_ready(dev) && DEVICE_API_IS(video, dev);
}

static int cmd_tvai_csi2rx_show(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	const struct lscc_csi2rx_config *cfg;
	uint32_t reg;

	__ASSERT_NO_MSG(argc == 2);

	dev = device_get_binding(argv[1]);
	if (dev == NULL || !device_is_video_and_ready(dev)) {
		shell_error(sh, "could not find a video device ready with that name");
		return -ENODEV;
	}

	cfg = dev->config;

	shell_print(sh, "");

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_WCL);
	reg |= LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_WCH) << 8;
	shell_print(sh, "packet1.word_count           %u", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_ECC);
	shell_print(sh, "packet1.ecc_checksum         0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_VC_DT);
	shell_print(sh, "packet1.data_type            0x%02x", reg);
	shell_print(sh, "packet1.virtual_channel       //");

	shell_print(sh, "");

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_WC2L);
	reg |= LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_WC2H) << 8;
	shell_print(sh, "packet2.word_count           %u", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_ECC2);
	shell_print(sh, "packet2.ecc_checksum         0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_VC_DT2);
	shell_print(sh, "packet2.data_type            0x%02x", reg);
	shell_print(sh, "packet2.virtual_channel       //");

	shell_print(sh, "");

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_REFDT);
	shell_print(sh, "config.reference_data_type   0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_NOCIL_DSETTLE);
	shell_print(sh, "config.data_settle           0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_NOCIL_RXFIFODEL_LSB);
	reg |= LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_NOCIL_RXFIFODEL_MSB) << 8;
	shell_print(sh, "config.rx_fifo_read_delay    0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_CONTROL);
	shell_print(sh, "config.parser_controls       0x%02x", reg);

	shell_print(sh, "");

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_CRC_BYTE_LOW);
	reg |= LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_CRC_BYTE_HIGH) << 8;
	shell_print(sh, "status.crc_value             0x%04x", reg);

	shell_print(sh, "");

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_ERROR_STATUS);
	shell_print(sh, "error.ecc_and_crc            0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_ERROR_CTRL);
	shell_print(sh, "error.hard_d_phy_control     0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_ERROR_HS_SOT);
	shell_print(sh, "error.start_of_transmission  0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_ERROR_HS_SOT_SYNC);
	shell_print(sh, "error.sot_synchronization    0x%02x", reg);

	reg = LSCC_CSI2RX_READ(cfg->base + LSCC_CSI2RX_REG_ERROR_HS_SOT_SYNC);
	shell_print(sh, "error.sot_sync_detection     0x%02x", reg);

	shell_print(sh, "");

	return 0;
}

static int cmd_tvai_csi2rx_clear(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	const struct lscc_csi2rx_config *cfg;

	__ASSERT_NO_MSG(argc == 2);

	dev = device_get_binding(argv[1]);
	if (dev == NULL || !device_is_video_and_ready(dev)) {
		shell_error(sh, "could not find a video device ready with that name");
		return -ENODEV;
	}

	cfg = dev->config;

	/* Clear error registers by writing 1 to them */
	LSCC_CSI2RX_WRITE(0xFF, cfg->base + LSCC_CSI2RX_REG_ERROR_STATUS);
	LSCC_CSI2RX_WRITE(0xFF, cfg->base + LSCC_CSI2RX_REG_ERROR_CTRL);
	LSCC_CSI2RX_WRITE(0xFF, cfg->base + LSCC_CSI2RX_REG_ERROR_HS_SOT);
	LSCC_CSI2RX_WRITE(0xFF, cfg->base + LSCC_CSI2RX_REG_ERROR_HS_SOT_SYNC);
	LSCC_CSI2RX_WRITE(0xFF, cfg->base + LSCC_CSI2RX_REG_ERROR_SOT_SYNC_DET);

	shell_print(sh, "MIPI error registers cleared");

	return 0;
}

static int cmd_tvai_csi2rx_set(const struct shell *sh, size_t argc, char **argv, uint32_t reg)
{
	const struct device *dev;
	const struct lscc_csi2rx_config *cfg;
	int cur_value;
	int new_value;
	char *end = NULL;

	__ASSERT_NO_MSG(argc == 2 || argc == 3);

	dev = device_get_binding(argv[1]);
	if (dev == NULL || !device_is_video_and_ready(dev)) {
		shell_error(sh, "could not find a video device ready with that name");
		return -ENODEV;
	}

	cfg = dev->config;

	cur_value = LSCC_CSI2RX_READ(cfg->base + reg);

	if (argc == 2) {
		shell_print(sh, "Current value: %u", cur_value);
		return 0;
	}

	if (strchr("+-", argv[2][0]) != NULL) {
		new_value = CLAMP(cur_value + strtol(argv[2], &end, 10), 0x00, 0xff);
	} else {
		new_value = strtoll(argv[2], &end, 10);
	}

	if (*end != '\0') {
		shell_error(sh, "Invalid number %s", argv[2]);
		shell_error(sh, "+<n> to increment, -<n> to decrement, <n> to set absolute value");
		return -EINVAL;
	}

	LSCC_CSI2RX_WRITE(new_value, cfg->base + reg);

	shell_print(sh, "Register changed from %u to %u", cur_value, new_value);

	return 0;
}

static int cmd_tvai_csi2rx_settle(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_tvai_csi2rx_set(sh, argc, argv, LSCC_CSI2RX_REG_NOCIL_DSETTLE);
}

static int cmd_tvai_csi2rx_delay(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_tvai_csi2rx_set(sh, argc, argv, LSCC_CSI2RX_REG_NOCIL_RXFIFODEL_LSB);
}

static int cmd_tvai_csi2rx_datatype(const struct shell *sh, size_t argc, char **argv)
{
	unsigned long val;
	char *end;

	if (argc == 2) {
		shell_print(sh, "Reference MIPI data type allowed for reception: 0x%02x",
			    LSCC_CSI2RX_READ(LSCC_CSI2RX_REG_REFDT));
		return 0;
	}

	val = strtoul(argv[2], &end, 16);
	if (*end != '\0' || val > 0xff) {
		shell_error(sh, "Expected hex number without 0x, in range [0x00-0xff], got '%s'",
			    argv[2]);
		return -EINVAL;
	}

	LSCC_CSI2RX_WRITE(val, LSCC_CSI2RX_REG_REFDT);

	return 0;
}

static void device_name_get(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_filter(idx, device_is_video_and_ready);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}
SHELL_DYNAMIC_CMD_CREATE(dsub_device_name, device_name_get);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_tvai_csi2rx,

	SHELL_CMD_ARG(clear, &dsub_device_name, "Clear MIPI CSI-2 error registers",
		      cmd_tvai_csi2rx_clear, 2, 0),
	SHELL_CMD_ARG(show, &dsub_device_name, "Display MIPI CSI-2 registers",
		      cmd_tvai_csi2rx_show, 2, 0),
	SHELL_CMD_ARG(settle, &dsub_device_name, "Adjust the settle cycle register",
		      cmd_tvai_csi2rx_settle, 2, 1),
	SHELL_CMD_ARG(delay, &dsub_device_name, "Adjust the RX fifo delay register",
		      cmd_tvai_csi2rx_delay, 2, 1),
	SHELL_CMD_ARG(datatype, &dsub_device_name, "Allow MIPI packet with this data type",
		      cmd_tvai_csi2rx_datatype, 2, 1),

	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(tvai_csi2rx, &sub_tvai_csi2rx, "Lattice DPHY RX commands", NULL);
