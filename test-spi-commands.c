// SPDX-License-Identifier: GPL-2.0
/*
 * SPI HID Command Tester for MSHW0231
 * Tests different initialization sequences
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#define MSHW0231_SPI_SPEED	17000000

static struct spi_device *test_spi;

struct spi_command {
	const char *name;
	u8 cmd[4];
	int len;
	int delay_ms;
};

/* Common SPI HID commands to test */
static struct spi_command test_commands[] = {
	{ "Reset",          {0x01, 0x00, 0x00, 0x00}, 4, 100 },
	{ "Get HID Desc",   {0x02, 0x00, 0x00, 0x00}, 4, 50 },
	{ "Set Power D0",   {0x08, 0x00, 0x00, 0x00}, 4, 100 },
	{ "Get Report",     {0x05, 0x00, 0x00, 0x00}, 4, 50 },
	{ "Set Mode",       {0x03, 0x00, 0x00, 0x00}, 4, 50 },
	{ "Vendor Cmd 1",   {0x80, 0x00, 0x00, 0x00}, 4, 100 },
	{ "Vendor Cmd 2",   {0x81, 0x00, 0x00, 0x00}, 4, 100 },
	{ "Collection 06",  {0x06, 0x00, 0x00, 0x00}, 4, 100 },
	{ "Init Sequence",  {0xFF, 0x00, 0x00, 0x00}, 4, 200 },
};

static int send_spi_command(struct spi_device *spi, struct spi_command *cmd)
{
	u8 rx[256] = {0};
	struct spi_transfer t = {
		.tx_buf = cmd->cmd,
		.rx_buf = rx,
		.len = cmd->len,
	};
	struct spi_message m;
	int ret;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	dev_info(&spi->dev, "Testing command: %s [%02x %02x %02x %02x]\n",
		cmd->name, cmd->cmd[0], cmd->cmd[1], cmd->cmd[2], cmd->cmd[3]);

	ret = spi_sync(spi, &m);
	if (ret) {
		dev_err(&spi->dev, "SPI transfer failed: %d\n", ret);
		return ret;
	}

	/* Check if we got non-0xFF response */
	if (rx[0] != 0xFF || rx[1] != 0xFF || rx[2] != 0xFF || rx[3] != 0xFF) {
		dev_info(&spi->dev, "SUCCESS! Got response: %02x %02x %02x %02x\n",
			rx[0], rx[1], rx[2], rx[3]);
		return 1; /* Success! */
	}

	if (cmd->delay_ms)
		msleep(cmd->delay_ms);

	return 0;
}

static int test_spi_probe(struct spi_device *spi)
{
	int i, j;
	int ret;

	dev_info(&spi->dev, "MSHW0231 Command Tester Starting\n");

	test_spi = spi;
	spi->max_speed_hz = MSHW0231_SPI_SPEED;
	spi->mode = SPI_MODE_0;

	ret = spi_setup(spi);
	if (ret) {
		dev_err(&spi->dev, "SPI setup failed: %d\n", ret);
		return ret;
	}

	/* Try each command */
	for (i = 0; i < ARRAY_SIZE(test_commands); i++) {
		ret = send_spi_command(spi, &test_commands[i]);
		if (ret > 0) {
			dev_info(&spi->dev, "Command '%s' woke up the device!\n",
				test_commands[i].name);
			break;
		}
	}

	/* Try command combinations */
	dev_info(&spi->dev, "Trying command combinations...\n");
	for (i = 0; i < ARRAY_SIZE(test_commands); i++) {
		for (j = 0; j < ARRAY_SIZE(test_commands); j++) {
			send_spi_command(spi, &test_commands[i]);
			ret = send_spi_command(spi, &test_commands[j]);
			if (ret > 0) {
				dev_info(&spi->dev, "Combination %s + %s worked!\n",
					test_commands[i].name, test_commands[j].name);
				return 0;
			}
		}
	}

	return 0;
}

static void test_spi_remove(struct spi_device *spi)
{
	dev_info(&spi->dev, "MSHW0231 Command Tester Removed\n");
}

static const struct acpi_device_id test_acpi_match[] = {
	{ "MSHW0231", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, test_acpi_match);

static struct spi_driver test_spi_driver = {
	.driver = {
		.name = "test-mshw0231",
		.acpi_match_table = test_acpi_match,
	},
	.probe = test_spi_probe,
	.remove = test_spi_remove,
};

module_spi_driver(test_spi_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MSHW0231 SPI Command Tester");
MODULE_AUTHOR("Surface Linux Team");