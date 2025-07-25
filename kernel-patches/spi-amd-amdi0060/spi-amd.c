// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
//
// AMD SPI controller driver
//
// Copyright (c) 2020, Advanced Micro Devices, Inc.
//
// Author: Sanjay R Mehta <sanju.mehta@amd.com>

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>

#define AMD_SPI_CTRL0_REG	0x00
#define AMD_SPI_EXEC_CMD	BIT(16)
#define AMD_SPI_FIFO_CLEAR	BIT(20)
#define AMD_SPI_BUSY		BIT(31)

#define AMD_SPI_OPCODE_REG	0x45
#define AMD_SPI_CMD_TRIGGER_REG	0x47
#define AMD_SPI_TRIGGER_CMD	BIT(7)

#define AMD_SPI_OPCODE_MASK	0xFF

#define AMD_SPI_ALT_CS_REG	0x1D
#define AMD_SPI_ALT_CS_MASK	0x3

#define AMD_SPI_FIFO_BASE	0x80
#define AMD_SPI_TX_COUNT_REG	0x48
#define AMD_SPI_RX_COUNT_REG	0x4B
#define AMD_SPI_STATUS_REG	0x4C
#define AMD_SPI_ADDR32CTRL_REG	0x50

#define AMD_SPI_FIFO_SIZE	70
#define AMD_SPI_MEM_SIZE	200
#define AMD_SPI_MAX_DATA	64
#define AMD_SPI_HID2_DMA_SIZE   4096

#define AMD_SPI_ENA_REG		0x20
#define AMD_SPI_ALT_SPD_SHIFT	20
#define AMD_SPI_ALT_SPD_MASK	GENMASK(23, AMD_SPI_ALT_SPD_SHIFT)
#define AMD_SPI_SPI100_SHIFT	0
#define AMD_SPI_SPI100_MASK	GENMASK(AMD_SPI_SPI100_SHIFT, AMD_SPI_SPI100_SHIFT)
#define AMD_SPI_SPEED_REG	0x6C
#define AMD_SPI_SPD7_SHIFT	8
#define AMD_SPI_SPD7_MASK	GENMASK(13, AMD_SPI_SPD7_SHIFT)

#define AMD_SPI_HID2_INPUT_RING_BUF0	0X100
#define AMD_SPI_HID2_CNTRL		0x150
#define AMD_SPI_HID2_INT_STATUS		0x154
#define AMD_SPI_HID2_CMD_START		0x156
#define AMD_SPI_HID2_INT_MASK		0x158
#define AMD_SPI_HID2_READ_CNTRL0	0x170
#define AMD_SPI_HID2_READ_CNTRL1	0x174
#define AMD_SPI_HID2_READ_CNTRL2	0x180

#define AMD_SPI_MAX_HZ		100000000
#define AMD_SPI_MIN_HZ		800000

#define AMD_SPI_IO_SLEEP_US	20
#define AMD_SPI_IO_TIMEOUT_US	2000000

/* SPI read command opcodes */
#define AMD_SPI_OP_READ          0x03	/* Read data bytes (low frequency) */
#define AMD_SPI_OP_READ_FAST     0x0b	/* Read data bytes (high frequency) */
#define AMD_SPI_OP_READ_1_1_2    0x3b	/* Read data bytes (Dual Output SPI) */
#define AMD_SPI_OP_READ_1_2_2    0xbb	/* Read data bytes (Dual I/O SPI) */
#define AMD_SPI_OP_READ_1_1_4    0x6b	/* Read data bytes (Quad Output SPI) */
#define AMD_SPI_OP_READ_1_4_4    0xeb	/* Read data bytes (Quad I/O SPI) */

/* SPI read command opcodes - 4B address */
#define AMD_SPI_OP_READ_FAST_4B		0x0c    /* Read data bytes (high frequency) */
#define AMD_SPI_OP_READ_1_1_2_4B	0x3c    /* Read data bytes (Dual Output SPI) */
#define AMD_SPI_OP_READ_1_2_2_4B	0xbc    /* Read data bytes (Dual I/O SPI) */
#define AMD_SPI_OP_READ_1_1_4_4B	0x6c    /* Read data bytes (Quad Output SPI) */
#define AMD_SPI_OP_READ_1_4_4_4B	0xec    /* Read data bytes (Quad I/O SPI) */

/**
 * enum amd_spi_versions - SPI controller versions
 * @AMD_SPI_V1:		AMDI0061 hardware version
 * @AMD_SPI_V2:		AMDI0062 hardware version
 * @AMD_HID2_SPI:	AMDI0063 hardware version
 */
enum amd_spi_versions {
	AMD_SPI_V1 = 1,
	AMD_SPI_V2,
	AMD_HID2_SPI,
};

enum amd_spi_speed {
	F_66_66MHz,
	F_33_33MHz,
	F_22_22MHz,
	F_16_66MHz,
	F_100MHz,
	F_800KHz,
	SPI_SPD7 = 0x7,
	F_50MHz = 0x4,
	F_4MHz = 0x32,
	F_3_17MHz = 0x3F
};

/**
 * struct amd_spi_freq - Matches device speed with values to write in regs
 * @speed_hz: Device frequency
 * @enable_val: Value to be written to "enable register"
 * @spd7_val: Some frequencies requires to have a value written at SPISPEED register
 */
struct amd_spi_freq {
	u32 speed_hz;
	u32 enable_val;
	u32 spd7_val;
};

/**
 * struct amd_spi - SPI driver instance
 * @io_remap_addr:	Start address of the SPI controller registers
 * @phy_dma_buf:	Physical address of DMA buffer
 * @dma_virt_addr:	Virtual address of DMA buffer
 * @version:		SPI controller hardware version
 * @speed_hz:		Device frequency
 */
struct amd_spi {
	void __iomem *io_remap_addr;
	dma_addr_t phy_dma_buf;
	void *dma_virt_addr;
	enum amd_spi_versions version;
	unsigned int speed_hz;
};

static inline u8 amd_spi_readreg8(struct amd_spi *amd_spi, int idx)
{
	return readb((u8 __iomem *)amd_spi->io_remap_addr + idx);
}

static inline void amd_spi_writereg8(struct amd_spi *amd_spi, int idx, u8 val)
{
	writeb(val, ((u8 __iomem *)amd_spi->io_remap_addr + idx));
}

static void amd_spi_setclear_reg8(struct amd_spi *amd_spi, int idx, u8 set, u8 clear)
{
	u8 tmp = amd_spi_readreg8(amd_spi, idx);

	tmp = (tmp & ~clear) | set;
	amd_spi_writereg8(amd_spi, idx, tmp);
}

static inline u16 amd_spi_readreg16(struct amd_spi *amd_spi, int idx)
{
	return readw((u8 __iomem *)amd_spi->io_remap_addr + idx);
}

static inline void amd_spi_writereg16(struct amd_spi *amd_spi, int idx, u16 val)
{
	writew(val, ((u8 __iomem *)amd_spi->io_remap_addr + idx));
}

static inline u32 amd_spi_readreg32(struct amd_spi *amd_spi, int idx)
{
	return readl((u8 __iomem *)amd_spi->io_remap_addr + idx);
}

static inline void amd_spi_writereg32(struct amd_spi *amd_spi, int idx, u32 val)
{
	writel(val, ((u8 __iomem *)amd_spi->io_remap_addr + idx));
}

static inline u64 amd_spi_readreg64(struct amd_spi *amd_spi, int idx)
{
	return readq((u8 __iomem *)amd_spi->io_remap_addr + idx);
}

static inline void amd_spi_writereg64(struct amd_spi *amd_spi, int idx, u64 val)
{
	writeq(val, ((u8 __iomem *)amd_spi->io_remap_addr + idx));
}

static inline void amd_spi_setclear_reg32(struct amd_spi *amd_spi, int idx, u32 set, u32 clear)
{
	u32 tmp = amd_spi_readreg32(amd_spi, idx);

	tmp = (tmp & ~clear) | set;
	amd_spi_writereg32(amd_spi, idx, tmp);
}

static void amd_spi_select_chip(struct amd_spi *amd_spi, u8 cs)
{
	amd_spi_setclear_reg8(amd_spi, AMD_SPI_ALT_CS_REG, cs, AMD_SPI_ALT_CS_MASK);
}

static inline void amd_spi_clear_chip(struct amd_spi *amd_spi, u8 chip_select)
{
	amd_spi_writereg8(amd_spi, AMD_SPI_ALT_CS_REG, chip_select & ~AMD_SPI_ALT_CS_MASK);
}

static void amd_spi_clear_fifo_ptr(struct amd_spi *amd_spi)
{
	amd_spi_setclear_reg32(amd_spi, AMD_SPI_CTRL0_REG, AMD_SPI_FIFO_CLEAR, AMD_SPI_FIFO_CLEAR);
}

static int amd_spi_set_opcode(struct amd_spi *amd_spi, u8 cmd_opcode)
{
	switch (amd_spi->version) {
	case AMD_SPI_V1:
		amd_spi_setclear_reg32(amd_spi, AMD_SPI_CTRL0_REG, cmd_opcode,
				       AMD_SPI_OPCODE_MASK);
		return 0;
	case AMD_SPI_V2:
	case AMD_HID2_SPI:
		amd_spi_writereg8(amd_spi, AMD_SPI_OPCODE_REG, cmd_opcode);
		return 0;
	default:
		return -ENODEV;
	}
}

static inline void amd_spi_set_rx_count(struct amd_spi *amd_spi, u8 rx_count)
{
	amd_spi_writereg8(amd_spi, AMD_SPI_RX_COUNT_REG, rx_count);
}

static inline void amd_spi_set_tx_count(struct amd_spi *amd_spi, u8 tx_count)
{
	amd_spi_writereg8(amd_spi, AMD_SPI_TX_COUNT_REG, tx_count);
}

static int amd_spi_busy_wait(struct amd_spi *amd_spi)
{
	u32 val;
	int reg;

	switch (amd_spi->version) {
	case AMD_SPI_V1:
		reg = AMD_SPI_CTRL0_REG;
		break;
	case AMD_SPI_V2:
	case AMD_HID2_SPI:
		reg = AMD_SPI_STATUS_REG;
		break;
	default:
		return -ENODEV;
	}

	return readl_poll_timeout(amd_spi->io_remap_addr + reg, val,
				  !(val & AMD_SPI_BUSY), 20, 2000000);
}

static int amd_spi_execute_opcode(struct amd_spi *amd_spi)
{
	int ret;

	ret = amd_spi_busy_wait(amd_spi);
	if (ret)
		return ret;

	switch (amd_spi->version) {
	case AMD_SPI_V1:
		/* Set ExecuteOpCode bit in the CTRL0 register */
		amd_spi_setclear_reg32(amd_spi, AMD_SPI_CTRL0_REG, AMD_SPI_EXEC_CMD,
				       AMD_SPI_EXEC_CMD);
		return 0;
	case AMD_SPI_V2:
	case AMD_HID2_SPI:
		/* Trigger the command execution */
		amd_spi_setclear_reg8(amd_spi, AMD_SPI_CMD_TRIGGER_REG,
				      AMD_SPI_TRIGGER_CMD, AMD_SPI_TRIGGER_CMD);
		return 0;
	default:
		return -ENODEV;
	}
}

static int amd_spi_host_setup(struct spi_device *spi)
{
	struct amd_spi *amd_spi = spi_controller_get_devdata(spi->controller);

	amd_spi_clear_fifo_ptr(amd_spi);

	return 0;
}

static const struct amd_spi_freq amd_spi_freq[] = {
	{ AMD_SPI_MAX_HZ,   F_100MHz,         0},
	{       66660000, F_66_66MHz,         0},
	{       50000000,   SPI_SPD7,   F_50MHz},
	{       33330000, F_33_33MHz,         0},
	{       22220000, F_22_22MHz,         0},
	{       16660000, F_16_66MHz,         0},
	{        4000000,   SPI_SPD7,    F_4MHz},
	{        3170000,   SPI_SPD7, F_3_17MHz},
	{ AMD_SPI_MIN_HZ,   F_800KHz,         0},
};

static void amd_set_spi_freq(struct amd_spi *amd_spi, u32 speed_hz)
{
	unsigned int i, spd7_val, alt_spd;

	for (i = 0; i < ARRAY_SIZE(amd_spi_freq)-1; i++)
		if (speed_hz >= amd_spi_freq[i].speed_hz)
			break;

	if (amd_spi->speed_hz == amd_spi_freq[i].speed_hz)
		return;

	amd_spi->speed_hz = amd_spi_freq[i].speed_hz;

	alt_spd = (amd_spi_freq[i].enable_val << AMD_SPI_ALT_SPD_SHIFT)
		   & AMD_SPI_ALT_SPD_MASK;
	amd_spi_setclear_reg32(amd_spi, AMD_SPI_ENA_REG, alt_spd,
			       AMD_SPI_ALT_SPD_MASK);

	if (amd_spi->speed_hz == AMD_SPI_MAX_HZ)
		amd_spi_setclear_reg32(amd_spi, AMD_SPI_ENA_REG, 1,
				       AMD_SPI_SPI100_MASK);

	if (amd_spi_freq[i].spd7_val) {
		spd7_val = (amd_spi_freq[i].spd7_val << AMD_SPI_SPD7_SHIFT)
			    & AMD_SPI_SPD7_MASK;
		amd_spi_setclear_reg32(amd_spi, AMD_SPI_SPEED_REG, spd7_val,
				       AMD_SPI_SPD7_MASK);
	}
}

static inline int amd_spi_fifo_xfer(struct amd_spi *amd_spi,
				    struct spi_controller *host,
				    struct spi_message *message)
{
	struct spi_transfer *xfer = NULL;
	struct spi_device *spi = message->spi;
	u8 cmd_opcode = 0, fifo_pos = AMD_SPI_FIFO_BASE;
	u8 *buf = NULL;
	u32 i = 0;
	u32 tx_len = 0, rx_len = 0;

	list_for_each_entry(xfer, &message->transfers,
			    transfer_list) {
		if (xfer->speed_hz)
			amd_set_spi_freq(amd_spi, xfer->speed_hz);
		else
			amd_set_spi_freq(amd_spi, spi->max_speed_hz);

		if (xfer->tx_buf) {
			buf = (u8 *)xfer->tx_buf;
			if (!tx_len) {
				cmd_opcode = *(u8 *)xfer->tx_buf;
				buf++;
				xfer->len--;
			}
			tx_len += xfer->len;

			/* Write data into the FIFO. */
			for (i = 0; i < xfer->len; i++)
				amd_spi_writereg8(amd_spi, fifo_pos + i, buf[i]);

			fifo_pos += xfer->len;
		}

		/* Store no. of bytes to be received from FIFO */
		if (xfer->rx_buf)
			rx_len += xfer->len;
	}

	if (!buf) {
		message->status = -EINVAL;
		goto fin_msg;
	}

	amd_spi_set_opcode(amd_spi, cmd_opcode);
	amd_spi_set_tx_count(amd_spi, tx_len);
	amd_spi_set_rx_count(amd_spi, rx_len);

	/* Execute command */
	message->status = amd_spi_execute_opcode(amd_spi);
	if (message->status)
		goto fin_msg;

	if (rx_len) {
		message->status = amd_spi_busy_wait(amd_spi);
		if (message->status)
			goto fin_msg;

		list_for_each_entry(xfer, &message->transfers, transfer_list)
			if (xfer->rx_buf) {
				buf = (u8 *)xfer->rx_buf;
				/* Read data from FIFO to receive buffer */
				for (i = 0; i < xfer->len; i++)
					buf[i] = amd_spi_readreg8(amd_spi, fifo_pos + i);
				fifo_pos += xfer->len;
			}
	}

	/* Update statistics */
	message->actual_length = tx_len + rx_len + 1;

fin_msg:
	switch (amd_spi->version) {
	case AMD_SPI_V1:
		break;
	case AMD_SPI_V2:
	case AMD_HID2_SPI:
		amd_spi_clear_chip(amd_spi, spi_get_chipselect(message->spi, 0));
		break;
	default:
		return -ENODEV;
	}

	spi_finalize_current_message(host);

	return message->status;
}

static inline bool amd_is_spi_read_cmd_4b(const u16 op)
{
	switch (op) {
	case AMD_SPI_OP_READ_FAST_4B:
	case AMD_SPI_OP_READ_1_1_2_4B:
	case AMD_SPI_OP_READ_1_2_2_4B:
	case AMD_SPI_OP_READ_1_1_4_4B:
	case AMD_SPI_OP_READ_1_4_4_4B:
		return true;
	default:
		return false;
	}
}

static inline bool amd_is_spi_read_cmd(const u16 op)
{
	switch (op) {
	case AMD_SPI_OP_READ:
	case AMD_SPI_OP_READ_FAST:
	case AMD_SPI_OP_READ_1_1_2:
	case AMD_SPI_OP_READ_1_2_2:
	case AMD_SPI_OP_READ_1_1_4:
	case AMD_SPI_OP_READ_1_4_4:
		return true;
	default:
		return amd_is_spi_read_cmd_4b(op);
	}
}

static bool amd_spi_supports_op(struct spi_mem *mem,
				const struct spi_mem_op *op)
{
	struct amd_spi *amd_spi = spi_controller_get_devdata(mem->spi->controller);

	/* bus width is number of IO lines used to transmit */
	if (op->cmd.buswidth > 1 || op->addr.buswidth > 4)
		return false;

	/* AMD SPI controllers support quad mode only for read operations */
	if (amd_is_spi_read_cmd(op->cmd.opcode)) {
		if (op->data.buswidth > 4)
			return false;

		/*
		 * HID2 SPI controller supports DMA read up to 4K bytes and
		 * doesn't support 4-byte address commands.
		 */
		if (amd_spi->version == AMD_HID2_SPI) {
			if (amd_is_spi_read_cmd_4b(op->cmd.opcode) ||
			    op->data.nbytes > AMD_SPI_HID2_DMA_SIZE)
				return false;
		} else if (op->data.nbytes > AMD_SPI_MAX_DATA) {
			return false;
		}
	} else if (op->data.buswidth > 1 || op->data.nbytes > AMD_SPI_MAX_DATA) {
		return false;
	}

	if (op->max_freq < mem->spi->controller->min_speed_hz)
		return false;

	return spi_mem_default_supports_op(mem, op);
}

static int amd_spi_adjust_op_size(struct spi_mem *mem, struct spi_mem_op *op)
{
	struct amd_spi *amd_spi = spi_controller_get_devdata(mem->spi->controller);

	/*
	 * HID2 SPI controller DMA read mode supports reading up to 4k
	 * bytes in single transaction, where as SPI0 and HID2 SPI
	 * controller index mode supports maximum of 64 bytes in a single
	 * transaction.
	 */
	if (amd_spi->version == AMD_HID2_SPI && amd_is_spi_read_cmd(op->cmd.opcode))
		op->data.nbytes = clamp_val(op->data.nbytes, 0, AMD_SPI_HID2_DMA_SIZE);
	else
		op->data.nbytes = clamp_val(op->data.nbytes, 0, AMD_SPI_MAX_DATA);

	return 0;
}

static void amd_spi_set_addr(struct amd_spi *amd_spi,
			     const struct spi_mem_op *op)
{
	u8 nbytes = op->addr.nbytes;
	u64 addr_val = op->addr.val;
	int base_addr, i;

	base_addr = AMD_SPI_FIFO_BASE + nbytes;

	for (i = 0; i < nbytes; i++) {
		amd_spi_writereg8(amd_spi, base_addr - i - 1, addr_val &
				  GENMASK(7, 0));
		addr_val >>= 8;
	}
}

static void amd_spi_mem_data_out(struct amd_spi *amd_spi,
				 const struct spi_mem_op *op)
{
	int base_addr = AMD_SPI_FIFO_BASE + op->addr.nbytes;
	u64 *buf_64 = (u64 *)op->data.buf.out;
	u32 nbytes = op->data.nbytes;
	u32 left_data = nbytes;
	u8 *buf;
	int i;

	amd_spi_set_opcode(amd_spi, op->cmd.opcode);
	amd_spi_set_addr(amd_spi, op);

	for (i = 0; left_data >= 8; i++, left_data -= 8)
		amd_spi_writereg64(amd_spi, base_addr + op->dummy.nbytes + (i * 8), *buf_64++);

	buf = (u8 *)buf_64;
	for (i = 0; i < left_data; i++) {
		amd_spi_writereg8(amd_spi, base_addr + op->dummy.nbytes + nbytes + i - left_data,
				  buf[i]);
	}

	amd_spi_set_tx_count(amd_spi, op->addr.nbytes + op->data.nbytes);
	amd_spi_set_rx_count(amd_spi, 0);
	amd_spi_clear_fifo_ptr(amd_spi);
	amd_spi_execute_opcode(amd_spi);
}

static void amd_spi_hiddma_read(struct amd_spi *amd_spi, const struct spi_mem_op *op)
{
	u16 hid_cmd_start, val;
	u32 hid_regval;

	/* Set the opcode in hid2_read_control0 register */
	hid_regval = amd_spi_readreg32(amd_spi, AMD_SPI_HID2_READ_CNTRL0);
	hid_regval = (hid_regval & ~GENMASK(7, 0)) | op->cmd.opcode;

	/*
	 * Program the address in the hid2_read_control0 register [8:31]. The address should
	 * be written starting from the 8th bit of the register, requiring an 8-bit shift.
	 * Additionally, to convert a 2-byte spinand address to a 3-byte address, another
	 * 8-bit shift is needed. Therefore, a total shift of 16 bits is required.
	 */
	hid_regval = (hid_regval & ~GENMASK(31, 8)) | (op->addr.val << 16);
	amd_spi_writereg32(amd_spi, AMD_SPI_HID2_READ_CNTRL0, hid_regval);

	/* Configure dummy clock cycles for fast read, dual, quad I/O commands */
	hid_regval = amd_spi_readreg32(amd_spi, AMD_SPI_HID2_READ_CNTRL2);
	/* Fast read dummy cycle */
	hid_regval &= ~GENMASK(4, 0);

	/* Fast read Dual I/O dummy cycle */
	hid_regval &= ~GENMASK(12, 8);

	/* Fast read Quad I/O dummy cycle */
	hid_regval = (hid_regval & ~GENMASK(20, 16)) | BIT(17);

	/* Set no of preamble bytecount */
	hid_regval &= ~GENMASK(27, 24);
	amd_spi_writereg32(amd_spi, AMD_SPI_HID2_READ_CNTRL2, hid_regval);

	/*
	 * Program the HID2 Input Ring Buffer0. 4k aligned buf_memory_addr[31:12],
	 * buf_size[4:0], end_input_ring[5].
	 */
	hid_regval = amd_spi->phy_dma_buf | BIT(5) | BIT(0);
	amd_spi_writereg32(amd_spi, AMD_SPI_HID2_INPUT_RING_BUF0, hid_regval);

	/* Program max read length(no of DWs) in hid2_read_control1 register */
	hid_regval = amd_spi_readreg32(amd_spi, AMD_SPI_HID2_READ_CNTRL1);
	hid_regval = (hid_regval & ~GENMASK(15, 0)) | ((op->data.nbytes / 4) - 1);
	amd_spi_writereg32(amd_spi, AMD_SPI_HID2_READ_CNTRL1, hid_regval);

	/* Set cmd start bit in hid2_cmd_start register to trigger HID basic read operation */
	hid_cmd_start = amd_spi_readreg16(amd_spi, AMD_SPI_HID2_CMD_START);
	amd_spi_writereg16(amd_spi, AMD_SPI_HID2_CMD_START, (hid_cmd_start | BIT(3)));

	/* Check interrupt status of HIDDMA basic read operation in hid2_int_status register */
	readw_poll_timeout(amd_spi->io_remap_addr + AMD_SPI_HID2_INT_STATUS, val,
			   (val & BIT(3)), AMD_SPI_IO_SLEEP_US, AMD_SPI_IO_TIMEOUT_US);

	/* Clear the interrupts by writing to hid2_int_status register */
	val = amd_spi_readreg16(amd_spi, AMD_SPI_HID2_INT_STATUS);
	amd_spi_writereg16(amd_spi, AMD_SPI_HID2_INT_STATUS, val);
}

static void amd_spi_mem_data_in(struct amd_spi *amd_spi,
				const struct spi_mem_op *op)
{
	int base_addr = AMD_SPI_FIFO_BASE + op->addr.nbytes;
	u64 *buf_64 = (u64 *)op->data.buf.in;
	u32 nbytes = op->data.nbytes;
	u32 left_data = nbytes;
	u32 data;
	u8 *buf;
	int i;

	/*
	 * Condition for using HID read mode. Only for reading complete page data, use HID read.
	 * Use index mode otherwise.
	 */
	if (amd_spi->version == AMD_HID2_SPI && amd_is_spi_read_cmd(op->cmd.opcode)) {
		amd_spi_hiddma_read(amd_spi, op);

		for (i = 0; left_data >= 8; i++, left_data -= 8)
			*buf_64++ = readq((u8 __iomem *)amd_spi->dma_virt_addr + (i * 8));

		buf = (u8 *)buf_64;
		for (i = 0; i < left_data; i++)
			buf[i] = readb((u8 __iomem *)amd_spi->dma_virt_addr +
				       (nbytes - left_data + i));

		/* Reset HID RX memory logic */
		data = amd_spi_readreg32(amd_spi, AMD_SPI_HID2_CNTRL);
		amd_spi_writereg32(amd_spi, AMD_SPI_HID2_CNTRL, data | BIT(5));
	} else {
		/* Index mode */
		amd_spi_set_opcode(amd_spi, op->cmd.opcode);
		amd_spi_set_addr(amd_spi, op);
		amd_spi_set_tx_count(amd_spi, op->addr.nbytes + op->dummy.nbytes);

		for (i = 0; i < op->dummy.nbytes; i++)
			amd_spi_writereg8(amd_spi, (base_addr + i), 0xff);

		amd_spi_set_rx_count(amd_spi, op->data.nbytes);
		amd_spi_clear_fifo_ptr(amd_spi);
		amd_spi_execute_opcode(amd_spi);
		amd_spi_busy_wait(amd_spi);

		for (i = 0; left_data >= 8; i++, left_data -= 8)
			*buf_64++ = amd_spi_readreg64(amd_spi, base_addr + op->dummy.nbytes +
						      (i * 8));

		buf = (u8 *)buf_64;
		for (i = 0; i < left_data; i++)
			buf[i] = amd_spi_readreg8(amd_spi, base_addr + op->dummy.nbytes +
						  nbytes + i - left_data);
	}

}

static void amd_set_spi_addr_mode(struct amd_spi *amd_spi,
				  const struct spi_mem_op *op)
{
	u32 val = amd_spi_readreg32(amd_spi, AMD_SPI_ADDR32CTRL_REG);

	if (amd_is_spi_read_cmd_4b(op->cmd.opcode))
		amd_spi_writereg32(amd_spi, AMD_SPI_ADDR32CTRL_REG, val | BIT(0));
	else
		amd_spi_writereg32(amd_spi, AMD_SPI_ADDR32CTRL_REG, val & ~BIT(0));
}

static int amd_spi_exec_mem_op(struct spi_mem *mem,
			       const struct spi_mem_op *op)
{
	struct amd_spi *amd_spi;

	amd_spi = spi_controller_get_devdata(mem->spi->controller);

	amd_set_spi_freq(amd_spi, op->max_freq);

	if (amd_spi->version == AMD_SPI_V2)
		amd_set_spi_addr_mode(amd_spi, op);

	switch (op->data.dir) {
	case SPI_MEM_DATA_IN:
		amd_spi_mem_data_in(amd_spi, op);
		break;
	case SPI_MEM_DATA_OUT:
		fallthrough;
	case SPI_MEM_NO_DATA:
		amd_spi_mem_data_out(amd_spi, op);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static const struct spi_controller_mem_ops amd_spi_mem_ops = {
	.exec_op = amd_spi_exec_mem_op,
	.adjust_op_size = amd_spi_adjust_op_size,
	.supports_op = amd_spi_supports_op,
};

static const struct spi_controller_mem_caps amd_spi_mem_caps = {
	.per_op_freq = true,
};

static int amd_spi_host_transfer(struct spi_controller *host,
				   struct spi_message *msg)
{
	struct amd_spi *amd_spi = spi_controller_get_devdata(host);
	struct spi_device *spi = msg->spi;

	amd_spi_select_chip(amd_spi, spi_get_chipselect(spi, 0));

	/*
	 * Extract spi_transfers from the spi message and
	 * program the controller.
	 */
	return amd_spi_fifo_xfer(amd_spi, host, msg);
}

static size_t amd_spi_max_transfer_size(struct spi_device *spi)
{
	return AMD_SPI_FIFO_SIZE;
}

static int amd_spi_setup_hiddma(struct amd_spi *amd_spi, struct device *dev)
{
	u32 hid_regval;

	/* Allocate DMA buffer to use for HID basic read operation */
	amd_spi->dma_virt_addr = dma_alloc_coherent(dev, AMD_SPI_HID2_DMA_SIZE,
						    &amd_spi->phy_dma_buf, GFP_KERNEL);
	if (!amd_spi->dma_virt_addr)
		return -ENOMEM;

	/*
	 * Enable interrupts and set mask bits in hid2_int_mask register to generate interrupt
	 * properly for HIDDMA basic read operations.
	 */
	hid_regval = amd_spi_readreg32(amd_spi, AMD_SPI_HID2_INT_MASK);
	hid_regval = (hid_regval & GENMASK(31, 8)) | BIT(19);
	amd_spi_writereg32(amd_spi, AMD_SPI_HID2_INT_MASK, hid_regval);

	/* Configure buffer unit(4k) in hid2_control register */
	hid_regval = amd_spi_readreg32(amd_spi, AMD_SPI_HID2_CNTRL);
	amd_spi_writereg32(amd_spi, AMD_SPI_HID2_CNTRL, hid_regval & ~BIT(3));

	return 0;
}

static int amd_spi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct spi_controller *host;
	struct amd_spi *amd_spi;
	int err;

	/* Allocate storage for host and driver private data */
	host = devm_spi_alloc_host(dev, sizeof(struct amd_spi));
	if (!host)
		return dev_err_probe(dev, -ENOMEM, "Error allocating SPI host\n");

	amd_spi = spi_controller_get_devdata(host);
	amd_spi->io_remap_addr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(amd_spi->io_remap_addr))
		return dev_err_probe(dev, PTR_ERR(amd_spi->io_remap_addr),
				     "ioremap of SPI registers failed\n");

	dev_dbg(dev, "io_remap_address: %p\n", amd_spi->io_remap_addr);

	amd_spi->version = (uintptr_t) device_get_match_data(dev);

	/* Initialize the spi_controller fields */
	host->bus_num = (amd_spi->version == AMD_HID2_SPI) ? 2 : 0;
	host->num_chipselect = 4;
	host->mode_bits = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD;
	host->flags = SPI_CONTROLLER_HALF_DUPLEX;
	host->max_speed_hz = AMD_SPI_MAX_HZ;
	host->min_speed_hz = AMD_SPI_MIN_HZ;
	host->setup = amd_spi_host_setup;
	host->transfer_one_message = amd_spi_host_transfer;
	host->mem_ops = &amd_spi_mem_ops;
	host->mem_caps = &amd_spi_mem_caps;
	host->max_transfer_size = amd_spi_max_transfer_size;
	host->max_message_size = amd_spi_max_transfer_size;

	/* Register the controller with SPI framework */
	err = devm_spi_register_controller(dev, host);
	if (err)
		return dev_err_probe(dev, err, "error registering SPI controller\n");

	if (amd_spi->version == AMD_HID2_SPI)
		err = amd_spi_setup_hiddma(amd_spi, dev);

	return err;
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id spi_acpi_match[] = {
	{ "AMDI0060", AMD_SPI_V1 },	/* Surface Laptop 4 AMD */
	{ "AMDI0061", AMD_SPI_V1 },
	{ "AMDI0062", AMD_SPI_V2 },
	{ "AMDI0063", AMD_HID2_SPI },
	{},
};
MODULE_DEVICE_TABLE(acpi, spi_acpi_match);
#endif

static struct platform_driver amd_spi_driver = {
	.driver = {
		.name = "amd_spi",
		.acpi_match_table = ACPI_PTR(spi_acpi_match),
	},
	.probe = amd_spi_probe,
};

module_platform_driver(amd_spi_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Sanjay Mehta <sanju.mehta@amd.com>");
MODULE_DESCRIPTION("AMD SPI Master Controller Driver");
