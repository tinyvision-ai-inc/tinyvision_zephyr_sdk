/*
 * Copyright (c) 2020 Innoseis BV
 * Copyright (c) 2024 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_pca9542a
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include <stdio.h>

#define BCD2BIN(bcd) (((10 * ((bcd) >> 4)) + ((bcd) & 0x0F)))

LOG_MODULE_REGISTER(pca9542a, CONFIG_I2C_LOG_LEVEL);

struct pca9542a_root_config {
	struct i2c_dt_spec i2c;
	uint8_t nchans;
};

struct pca9542a_root_data {
	struct k_mutex lock;
	uint8_t selected_chan;
};

struct pca9542a_channel_config {
	const struct device *root;
	uint8_t chan_mask;
};

static inline struct pca9542a_root_data *get_root_data_from_channel(const struct device *dev)
{
	const struct pca9542a_channel_config *channel_config = dev->config;

	return channel_config->root->data;
}

static inline const struct pca9542a_root_config *
get_root_config_from_channel(const struct device *dev)
{
	const struct pca9542a_channel_config *channel_config = dev->config;

	return channel_config->root->config;
}

static int pca9542a_configure(const struct device *dev, uint32_t dev_config)
{
	const struct pca9542a_root_config *cfg = get_root_config_from_channel(dev);

	return i2c_configure(cfg->i2c.bus, dev_config);
}

static int pca9542a_set_channel(const struct device *dev, uint8_t select_mask)
{
	struct pca9542a_root_data *data = dev->data;
	const struct pca9542a_root_config *cfg = dev->config;
	int res = 0;

	/* Only select the channel if its different from the last channel */
	if (data->selected_chan != select_mask) {
		res = i2c_write_dt(&cfg->i2c, &select_mask, 1);
		if (res == 0) {
			data->selected_chan = select_mask;
		} else {
			LOG_DBG("pca9542a: failed to set channel");
		}
	}
	return res;
}

static int pca9542a_transfer(const struct device *dev, struct i2c_msg *msgs, uint8_t num_msgs,
			     uint16_t addr)
{
	struct pca9542a_root_data *data = get_root_data_from_channel(dev);
	const struct pca9542a_root_config *config = get_root_config_from_channel(dev);
	const struct pca9542a_channel_config *down_cfg = dev->config;
	int res;

	res = k_mutex_lock(&data->lock, K_MSEC(5000));
	if (res != 0) {
		return res;
	}

	res = pca9542a_set_channel(down_cfg->root, down_cfg->chan_mask);
	if (res != 0) {
#ifdef CONFIG_I2C_PCA9542A_IGNORE_FAILURE
		res = i2c_transfer(config->i2c.bus, msgs, num_msgs, addr);
#endif
		goto end;
	}

	res = i2c_transfer(config->i2c.bus, msgs, num_msgs, addr);
end:
	k_mutex_unlock(&data->lock);
	return res;
}

static int pca9542a_root_init(const struct device *dev)
{
	struct pca9542a_root_data *i2c_pca9542a = dev->data;
	const struct pca9542a_root_config *config = dev->config;

	if (!device_is_ready(config->i2c.bus)) {
		LOG_ERR("I2C bus %s not ready", config->i2c.bus->name);
		return -ENODEV;
	}

	i2c_pca9542a->selected_chan = 0;

	return 0;
}

static int pca9542a_channel_init(const struct device *dev)
{
	const struct pca9542a_channel_config *chan_cfg = dev->config;
	const struct pca9542a_root_config *root_cfg = get_root_config_from_channel(dev);

	if (!device_is_ready(chan_cfg->root)) {
		LOG_ERR("I2C mux root %s not ready", chan_cfg->root->name);
		return -ENODEV;
	}

	if (chan_cfg->chan_mask >= BCD2BIN(root_cfg->nchans + 4)) {
		LOG_ERR("Wrong DTS address provided for %s", dev->name);
		return -EINVAL;
	}

	return 0;
}

static const struct i2c_driver_api pca9542a_api_funcs = {
	.configure = pca9542a_configure,
	.transfer = pca9542a_transfer,
};

BUILD_ASSERT(CONFIG_I2C_PCA9542A_CHANNEL_INIT_PRIO > CONFIG_I2C_PCA9542A_ROOT_INIT_PRIO,
	     "I2C multiplexer channels must be initialized after their root");

#define PCA9542A_CHILD_DEFINE(node_id)                                                             \
	static const struct pca9542a_channel_config pca9542a_down_config_##node_id = {             \
		.chan_mask = BCD2BIN(DT_REG_ADDR(node_id) + 4),                                    \
		.root = DEVICE_DT_GET(DT_PARENT(node_id)),                                         \
	};                                                                                         \
	DEVICE_DT_DEFINE(node_id, pca9542a_channel_init, NULL, NULL,                               \
			 &pca9542a_down_config_##node_id, POST_KERNEL,                             \
			 CONFIG_I2C_PCA9542A_CHANNEL_INIT_PRIO, &pca9542a_api_funcs);

#define PCA9542A_INIT(n)                                                                           \
	static const struct pca9542a_root_config pca9542a_cfg_##n = {                              \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.nchans = 2,                                                                       \
	};                                                                                         \
	static struct pca9542a_root_data pca9542a_data_##n = {                                     \
		.lock = Z_MUTEX_INITIALIZER(pca9542a_data_##n.lock),                               \
	};                                                                                         \
	I2C_DEVICE_DT_DEFINE(DT_INST(n, nxp_pca9542a), pca9542a_root_init, NULL,                   \
			     &pca9542a_data_##n, &pca9542a_cfg_##n, POST_KERNEL,                   \
			     CONFIG_I2C_PCA9542A_ROOT_INIT_PRIO, NULL);                            \
	DT_FOREACH_CHILD(DT_INST(n, nxp_pca9542a), PCA9542A_CHILD_DEFINE);

DT_INST_FOREACH_STATUS_OKAY(PCA9542A_INIT)
