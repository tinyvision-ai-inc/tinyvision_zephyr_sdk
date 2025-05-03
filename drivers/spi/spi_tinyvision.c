/*
 * Copyright 2025 tinyVision.ai
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#define DT_DRV_COMPAT tinyvision_spi

#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(spi_tinyvision);

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/spi/rtio.h>

#define SPI_TINYVISION_DATA        0x00
#define SPI_TINYVISION_BUFFER      0x04
#define SPI_TINYVISION_CONFIG      0x08
#define SPI_TINYVISION_INTERRUPT   0x0C
#define SPI_TINYVISION_CLK_DIVIDER 0x20
#define SPI_TINYVISION_SS_SETUP    0x24
#define SPI_TINYVISION_SS_HOLD     0x28
#define SPI_TINYVISION_SS_DISABLE  0x2C

#define SPI_TINYVISION_CMD_WRITE (1 << 8)
#define SPI_TINYVISION_CMD_READ (1 << 9)
#define SPI_TINYVISION_CMD_SS (1 << 11)

#define SPI_TINYVISION_RSP_VALID (1 << 31)

#define SPI_TINYVISION_STATUS_CMD_INT_ENABLE (1 << 0)
#define SPI_TINYVISION_STATUS_RSP_INT_ENABLE (1 << 1)
#define SPI_TINYVISION_STATUS_CMD_INT_FLAG (1 << 8)
#define SPI_TINYVISION_STATUS_RSP_INT_FLAG (1 << 9)


#define SPI_TINYVISION_MODE_CPOL (1 << 0)
#define SPI_TINYVISION_MODE_CPHA (1 << 1)


/** Working data for the device */
struct spi_tinyvision_config {
	uint32_t reg;
	uint32_t cpol;
	uint32_t cpha;
	uint32_t mode;
	uint32_t clkDivider;
	uint32_t ssSetup;
	uint32_t ssHold;
	uint32_t ssDisable;
};

static inline uint32_t read_u32(uint32_t address){
    return *((volatile uint32_t*) address);
}

static inline void write_u32(uint32_t data, uint32_t address){
    *((volatile uint32_t*) address) = data;
}

static inline uint16_t read_u16(uint32_t address){
    return *((volatile uint16_t*) address);
}

static inline void write_u16(uint16_t data, uint32_t address){
    *((volatile uint16_t*) address) = data;
}

static inline uint8_t read_u8(uint32_t address){
    return *((volatile uint8_t*) address);
}

static inline void write_u8(uint8_t data, uint32_t address){
    *((volatile uint8_t*) address) = data;
}

static inline void write_u32_ad(uint32_t address, uint32_t data){
    *((volatile uint32_t*) address) = data;
}

static uint32_t spi_tinyvision_cmdAvailability(uint32_t reg)
{
    return read_u32(reg + SPI_TINYVISION_BUFFER) & 0xFFFF;
}
static uint32_t spi_tinyvision_rspOccupancy(uint32_t reg)
{
    return read_u32(reg + SPI_TINYVISION_BUFFER) >> 16;
}

static uint8_t spi_tinyvision_write_read(uint32_t reg, uint8_t data)
{
    while(spi_tinyvision_cmdAvailability(reg) == 0);
    write_u32(data | SPI_TINYVISION_CMD_READ | SPI_TINYVISION_CMD_WRITE, reg + SPI_TINYVISION_DATA);
    while(spi_tinyvision_rspOccupancy(reg) == 0);
    return read_u32(reg + SPI_TINYVISION_DATA);
}

static uint8_t spi_tinyvision_read(uint32_t reg)
{
    while(spi_tinyvision_cmdAvailability(reg) == 0);
    write_u32(SPI_TINYVISION_CMD_READ, reg + SPI_TINYVISION_DATA);
    while(spi_tinyvision_rspOccupancy(reg) == 0);
    return read_u32(reg + SPI_TINYVISION_DATA);
}

static void spi_tinyvision_write(uint32_t reg, uint8_t data)
{
    while(spi_tinyvision_cmdAvailability(reg) == 0);
    write_u32(data | SPI_TINYVISION_CMD_WRITE, reg + SPI_TINYVISION_DATA);
}

static void spi_tinyvision_select(uint32_t reg, uint32_t slaveId)
{
    while(spi_tinyvision_cmdAvailability(reg) == 0);
    write_u32(slaveId | 0x80 | SPI_TINYVISION_CMD_SS, reg + SPI_TINYVISION_DATA);
}

static void spi_tinyvision_deselect(uint32_t reg, uint32_t slaveId)
{
    while(spi_tinyvision_cmdAvailability(reg) == 0);
    write_u32(slaveId | 0x00 | SPI_TINYVISION_CMD_SS, reg + SPI_TINYVISION_DATA);
}

static void spi_tinyvision_spi_write_buf(uint32_t reg, const struct spi_buf *tx)
{
	for (int i = 0; i < tx->len; i++) {
		spi_tinyvision_write(reg, ((uint8_t *)tx->buf)[i]);
	}
}

static void spi_tinyvision_spi_read_buf(uint32_t reg, const struct spi_buf *rx)
{
	for (int i = 0; i < rx->len; i++) {
		((uint8_t *)rx->buf)[i] = spi_tinyvision_read(reg);
	}
}

static void spi_tinyvision_spi_write_read_buf(uint32_t reg, const struct spi_buf *tx, const struct spi_buf *rx)
{
	for (int i = 0; i < tx->len; i++) {
		((uint8_t *)rx->buf)[i] = spi_tinyvision_write_read(reg, ((uint8_t *)tx->buf)[i]);
	}
}

static int spi_tinyvision_io(const struct device *dev, const struct spi_config *config,
		       const struct spi_buf_set *tx_bufs, const struct spi_buf_set *rx_bufs)
{
	const struct spi_tinyvision_config *cfg = dev->config;
	int ret;
	uint32_t lock_key = irq_lock();
	size_t tx_count = 0;
	size_t rx_count = 0;
	const struct spi_buf *tx = NULL;
	const struct spi_buf *rx = NULL;

	printf("New transaction starts\n");

	if (tx_bufs) {
		tx = tx_bufs->buffers;
		tx_count = tx_bufs->count;
		for (int j = 0; j < tx_count; j++) {
			printf("write: ");
			for (int i = 0; i < tx[j].len; i++) {
				 printf("%x ", ((uint8_t *)(tx[j].buf))[i]);
			}
			printf("\n");
		}
	}

	if (rx_bufs) {
		rx = rx_bufs->buffers;
		rx_count = rx_bufs->count;
	} else {
		rx = NULL;
	}
	spi_tinyvision_select(cfg->reg, config->slave);

	while (tx_count != 0 && rx_count != 0) {

		if (tx->buf == NULL) {
			spi_tinyvision_spi_read_buf(cfg->reg, rx);
		} else if (rx->buf == NULL) {
			spi_tinyvision_spi_write_buf(cfg->reg, tx);
		} else {
			//spi_tinyvision_spi_write_buf(cfg->reg, tx);
			//spi_tinyvision_spi_read_buf(cfg->reg, rx);
			spi_tinyvision_spi_write_read_buf(cfg->reg, tx, rx);
		}

		tx++;
		tx_count--;
		rx++;
		rx_count--;
	}

	if (tx_count < 1 || rx_count < 2) {
			spi_tinyvision_deselect(cfg->reg, config->slave);
	}

	spi_tinyvision_select(cfg->reg, config->slave);

	for (; tx_count != 0; tx_count--) {
		spi_tinyvision_spi_write_buf(cfg->reg, tx++);
	}

	for (; rx_count != 0; rx_count--) {
		spi_tinyvision_spi_read_buf(cfg->reg, rx++);
	}

	spi_tinyvision_deselect(cfg->reg, config->slave);

	/*if (config->slave == 0) {
		int status = 1;
		while(status & 1) {
			spi_tinyvision_select(cfg->reg, 0);
			spi_tinyvision_write(cfg->reg, 5);
			status = spi_tinyvision_read(cfg->reg);
			spi_tinyvision_deselect(cfg->reg, 0);
		}
	}*/

	if (rx_bufs) {
		for (int j = 0; j < rx_bufs->count; j++) {
			printf("read: ");
			for (int i = 0; i < rx_bufs->buffers[j].len; i++) {
				printf("%x ", ((uint8_t *)(rx_bufs->buffers[j].buf))[i]);
			}
			printf("\n");
		}
	}


	irq_unlock(lock_key);
	return 0;
}

static int spi_tinyvision_release(const struct device *dev, const struct spi_config *config)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(config);

	return 0;
}

static int spi_tinyvision_init(const struct device *dev)
{
#if 0
	const struct spi_tinyvision_config *cfg = dev->config;
	write_u32((cfg->cpol << 0) | (cfg->cpha << 1) | (cfg->mode << 4), reg + SPI_TINYVISION_CONFIG);
    write_u32(cfg->clkDivider, reg + SPI_TINYVISION_CLK_DIVIDER);
    write_u32(cfg->ssSetup, reg + SPI_TINYVISION_SS_SETUP);
    write_u32(cfg->ssHold, reg + SPI_TINYVISION_SS_HOLD);
    write_u32(cfg->ssDisable, reg + SPI_TINYVISION_SS_DISABLE);
#endif
	return 0;
}

/* Device instantiation */

static DEVICE_API(spi, spi_tinyvision_api) = {
	.transceive = spi_tinyvision_io,
#ifdef CONFIG_SPI_RTIO
	.iodev_submit = spi_rtio_iodev_default_submit,
#endif
	.release = spi_tinyvision_release,
};

#define SPI_TINYVISION_INIT(n)                                                                     \
	static struct spi_tinyvision_config spi_tinyvision_cfg_##n = {                                 \
		.reg = DT_INST_REG_ADDR(n),                                                                \
	};                                                                                             \
	SPI_DEVICE_DT_INST_DEFINE(n, spi_tinyvision_init, NULL, NULL,                                  \
		&spi_tinyvision_cfg_##n, POST_KERNEL, CONFIG_SPI_INIT_PRIORITY, &spi_tinyvision_api);

DT_INST_FOREACH_STATUS_OKAY(SPI_TINYVISION_INIT)
