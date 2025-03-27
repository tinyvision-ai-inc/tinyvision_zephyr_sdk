/*
 * Copyright (c) 2024-2025 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT lattice_dphy_rx

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/video.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(dphy_rx, CONFIG_VIDEO_LOG_LEVEL);

#define DPHY_RX_LANE_SETTING        0x0A
#define DPHY_RX_VC_DT               0x1F
#define DPHY_RX_WCL                 0x20
#define DPHY_RX_WCH                 0x21
#define DPHY_RX_ECC                 0x22
#define DPHY_RX_VC_DT2              0x23
#define DPHY_RX_WC2L                0x24
#define DPHY_RX_WC2H                0x25
#define DPHY_RX_REFDT               0x27
#define DPHY_RX_ERROR_STATUS        0x28
#define DPHY_RX_ERROR_STATUS_EN     0x29
#define DPHY_RX_CRC_BYTE_LOW        0x30
#define DPHY_RX_CRC_BYTE_HIGH       0x31
#define DPHY_RX_ERROR_CTRL          0x32
#define DPHY_RX_ERROR_HS_SOT        0x33
#define DPHY_RX_ERROR_HS_SOT_SYNC   0x34
#define DPHY_RX_CONTROL             0x35
#define DPHY_RX_NOCIL_DSETTLE       0x36
#define DPHY_RX_NOCIL_RXFIFODEL_LSB 0x37
#define DPHY_RX_NOCIL_RXFIFODEL_MSB 0x38
#define DPHY_RX_ERROR_SOT_SYNC_DET  0x39

struct dphy_rx_config {
	const struct device *source_dev;
	uint32_t base;
};

struct dphy_rx_reg {
	uint32_t addr;
	const char *name;
	const char *description;
};

static int dphy_rx_init(const struct device *dev)
{
	const struct dphy_rx_config *cfg = dev->config;

	/* Setup the REF_DT to RAW8 */
	sys_write8(0x2A, cfg->base + DPHY_RX_REFDT);

	/* Set the settle time */
	sys_write8(0x06, cfg->base + DPHY_RX_NOCIL_DSETTLE);

	return 0;
}

static int dphy_rx_set_format(const struct device *dev, enum video_endpoint_id ep,
			      struct video_format *fmt)
{
	const struct dphy_rx_config *cfg = dev->config;

	return video_set_format(cfg->source_dev, VIDEO_EP_OUT, fmt);
}

static int dphy_rx_get_format(const struct device *dev, enum video_endpoint_id ep,
			      struct video_format *fmt)
{
	const struct dphy_rx_config *cfg = dev->config;

	return video_get_format(cfg->source_dev, VIDEO_EP_OUT, fmt);
}

static int dphy_rx_get_caps(const struct device *dev, enum video_endpoint_id ep,
			    struct video_caps *caps)
{
	const struct dphy_rx_config *cfg = dev->config;

	return video_get_caps(cfg->source_dev, VIDEO_EP_OUT, caps);
}

static int dphy_rx_set_frmival(const struct device *dev, enum video_endpoint_id ep,
			       struct video_frmival *frmival)
{
	const struct dphy_rx_config *cfg = dev->config;

	return video_set_frmival(cfg->source_dev, VIDEO_EP_OUT, frmival);
}

static int dphy_rx_get_frmival(const struct device *dev, enum video_endpoint_id ep,
			       struct video_frmival *frmival)
{
	const struct dphy_rx_config *cfg = dev->config;

	return video_get_frmival(cfg->source_dev, VIDEO_EP_OUT, frmival);
}

static int dphy_rx_enum_frmival(const struct device *dev, enum video_endpoint_id ep,
				   struct video_frmival_enum *fie)
{
	const struct dphy_rx_config *cfg = dev->config;

	return video_enum_frmival(cfg->source_dev, VIDEO_EP_OUT, fie);
}

static int dphy_rx_set_stream(const struct device *dev, bool on)
{
	const struct dphy_rx_config *cfg = dev->config;

	return on ? video_stream_start(cfg->source_dev) : video_stream_stop(cfg->source_dev);

}

static int dphy_rx_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct dphy_rx_config *cfg = dev->config;

	return video_set_ctrl(cfg->source_dev, cid, value);
}

static int dphy_rx_get_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	const struct dphy_rx_config *cfg = dev->config;

	return video_get_ctrl(cfg->source_dev, cid, value);
}

static const DEVICE_API(video, dphy_rx_driver_api) = {
	.set_format = dphy_rx_set_format,
	.get_format = dphy_rx_get_format,
	.get_caps = dphy_rx_get_caps,
	.set_frmival = dphy_rx_set_frmival,
	.get_frmival = dphy_rx_get_frmival,
	.enum_frmival = dphy_rx_enum_frmival,
	.set_stream = dphy_rx_set_stream,
	.set_ctrl = dphy_rx_set_ctrl,
	.get_ctrl = dphy_rx_get_ctrl,
};

#define SRC_EP(inst) DT_INST_ENDPOINT_BY_ID(inst, 0, 0)

#define DPHY_RX_DEVICE_DEFINE(inst)                                                                \
	const struct dphy_rx_config dphy_rx_cfg_##inst = {                                         \
		.source_dev = DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(SRC_EP(inst))),                  \
		.base = DT_INST_REG_ADDR(inst),                                                    \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, dphy_rx_init, NULL, NULL, &dphy_rx_cfg_##inst,                 \
			      POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY, &dphy_rx_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DPHY_RX_DEVICE_DEFINE)

static bool device_is_video_and_ready(const struct device *dev)
{
	return device_is_ready(dev) && DEVICE_API_IS(video, dev);
}

static const struct dphy_rx_reg dphy_rx_regs[] = {

#define DPHY_RX_REG(reg, desc) \
	{.addr = DPHY_RX_##reg, .name = #reg, .description = (desc)}

	DPHY_RX_REG(LANE_SETTING, "Number of Lane Select"),
	DPHY_RX_REG(VC_DT, "Virtual channel and Data type register"),
	DPHY_RX_REG(WCL, "Word count low register"),
	DPHY_RX_REG(WCH, "Word count high register"),
	DPHY_RX_REG(ECC, "ECC register / Extended virtual channel ID"),
	DPHY_RX_REG(VC_DT2, "Virtual channel 2 and Data type 2 register"),
	DPHY_RX_REG(WC2L, "Word count 2 low register"),
	DPHY_RX_REG(WC2H, "Word count 2 high register"),
	DPHY_RX_REG(REFDT, "Reference data type"),
	DPHY_RX_REG(ERROR_STATUS, "ECC and CRC error status"),
	DPHY_RX_REG(ERROR_STATUS_EN, "ECC and CRC error status enable"),
	DPHY_RX_REG(CRC_BYTE_LOW, "Received payload CRC LSB"),
	DPHY_RX_REG(CRC_BYTE_HIGH, "Received payload CRC MSB"),
	DPHY_RX_REG(ERROR_CTRL, "Hard D-PHY control error"),
	DPHY_RX_REG(ERROR_HS_SOT, "Hard D-PHY Start-of-Transmit error"),
	DPHY_RX_REG(ERROR_HS_SOT_SYNC, "Hard D-PHY Start-of-Transmit sync error"),
	DPHY_RX_REG(CONTROL, "Parser Controls"),
	DPHY_RX_REG(NOCIL_DSETTLE, "Data Settle register"),
	DPHY_RX_REG(NOCIL_RXFIFODEL_LSB, "RX_FIFO read delay LSB register"),
	DPHY_RX_REG(NOCIL_RXFIFODEL_MSB, "RX_FIFO read delay MSB register"),
	DPHY_RX_REG(ERROR_SOT_SYNC_DET, "Soft D-PHY SOT sync detect error"),
};

static int cmd_dphy_rx_show(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	const struct dphy_rx_config *cfg;
	uint32_t value;

	__ASSERT_NO_MSG(argc == 2);

	dev = device_get_binding(argv[1]);
	if (dev == NULL || !device_is_video_and_ready(dev)) {
		shell_error(sh, "could not find a video device ready with that name");
		return -ENODEV;
	}

	cfg = dev->config;

	for (size_t i = 0; i < ARRAY_SIZE(dphy_rx_regs); i++) {
		value = sys_read8(cfg->base + dphy_rx_regs[i].addr);
		shell_print(sh, "%-20s = %02x - %s",
			    dphy_rx_regs[i].name, value, dphy_rx_regs[i].description);
	}

	return 0;
}

static int cmd_dphy_rx_clear(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	const struct dphy_rx_config *cfg;

	__ASSERT_NO_MSG(argc == 2);

	dev = device_get_binding(argv[1]);
	if (dev == NULL || !device_is_video_and_ready(dev)) {
		shell_error(sh, "could not find a video device ready with that name");
		return -ENODEV;
	}

	cfg = dev->config;

	/* Clear error registers by writing 1 to them */
	sys_write8(0xFF, cfg->base + DPHY_RX_ERROR_STATUS);
	sys_write8(0xFF, cfg->base + DPHY_RX_ERROR_CTRL);
	sys_write8(0xFF, cfg->base + DPHY_RX_ERROR_HS_SOT);
	sys_write8(0xFF, cfg->base + DPHY_RX_ERROR_HS_SOT_SYNC);
	sys_write8(0xFF, cfg->base + DPHY_RX_ERROR_SOT_SYNC_DET);

	shell_print(sh, "MIPI error registers cleared");

	return 0;
}

static int cmd_dphy_rx_set(const struct shell *sh, size_t argc, char **argv, uint32_t reg)
{
	const struct device *dev;
	const struct dphy_rx_config *cfg;
	uint32_t cur_value;
	uint32_t new_value;
	char *end = NULL;

	__ASSERT_NO_MSG(argc == 3);

	dev = device_get_binding(argv[1]);
	if (dev == NULL || !device_is_video_and_ready(dev)) {
		shell_error(sh, "could not find a video device ready with that name");
		return -ENODEV;
	}

	cfg = dev->config;

	cur_value = sys_read8(cfg->base + reg);

	if (strchr("+-", argv[2][0]) == 0) {
		new_value = cur_value + strtoll(argv[2], &end, 10);
	} else {
		new_value = strtoll(argv[2], &end, 10);
	}

	if (*end != '\0') {
		shell_error(sh, "Invalid number %s", argv[2]);
		shell_error(sh, "+<n> to increment, -<n> to decrement, <n> to set absolute value");
		return -EINVAL;
	}

	sys_write8(new_value, cfg->base + reg);

	shell_print(sh, "Register changed from %u to %u", cur_value, new_value);

	return 0;
}

static int cmd_dphy_rx_settle(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_dphy_rx_set(sh, argc, argv, DPHY_RX_NOCIL_DSETTLE);
}

static int cmd_dphy_rx_delay(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_dphy_rx_set(sh, argc, argv, DPHY_RX_NOCIL_RXFIFODEL_LSB);
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
	sub_dphy_rx,

	SHELL_CMD_ARG(clear, &dsub_device_name, "Clear MIPI CSI-2 error registers",
		      cmd_dphy_rx_clear, 2, 0),
	SHELL_CMD_ARG(show, &dsub_device_name, "Display MIPI CSI-2 registers",
		      cmd_dphy_rx_show, 2, 0),
	SHELL_CMD_ARG(settle, &dsub_device_name, "Adjust the settle cycle register",
		      cmd_dphy_rx_settle, 3, 0),
	SHELL_CMD_ARG(delay, &dsub_device_name, "Adjust the RX fifo delay register",
		      cmd_dphy_rx_delay, 3, 0),

	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(dphy_rx, &sub_dphy_rx, "Lattice DPHY RX commands", NULL);
