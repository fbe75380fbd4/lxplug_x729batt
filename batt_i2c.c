/*
 *      batt_i2c.c
 *
 *      Copyright 2024 Enrique Mejia.
 *      All rights reserved.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 */

#include "batt_i2c.h"
#include <fcntl.h>
#include <glib-2.0/glib/gstdio.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wiringPi.h>

/* Creates a new battery object */
battery *battery_new()
{
	battery *b = g_new0(battery, 1);

	b->voltage = 0.0;
	b->devicestatus = 0;
	b->percentage = 0;
	b->powersource = 0;

	return b;
}

/* Verify access to I2C device file */
static int open_i2c()
{
	char filename[20];
	int size = sizeof(filename);
	int file;

	snprintf(filename, size, I2C_DEV_PATH, I2C_DEV_BUS);
	filename[size - 1] = '\0';
	file = open(filename, O_RDWR);

	if (file < 0)
	{
		if (errno == EACCES)
			g_warning("Run as root?");
		else
			g_warning("Could not open file %s: %s", filename, strerror(errno));
	}
	else
		g_debug("Success opening file %s.", filename);

	return file;
}

/* Verify read capability and address access on I2C device */
static int read_i2c(int file)
{
	unsigned long funcs;

	// check adapter functionality
	if (ioctl(file, I2C_FUNCS, &funcs) < 0)
	{
		g_warning("Could not get the adapter functionality matrix: %s", strerror(errno));
		return -1;
	}

	// check read word capability
	if (!(funcs & I2C_FUNC_SMBUS_READ_WORD_DATA))
	{
		g_warning("Adapter does not have SMBus read word capability.");
		return -1;
	}

	// Attempt binding to address
	if (ioctl(file, I2C_SLAVE, I2C_DEV_ADDR) < 0)
	{
		g_warning("Could not set address to 0x%02x: %s", I2C_DEV_ADDR, strerror(errno));
		return -1;
	}

	g_debug("Found read word capability. Bound to address 0x%02x.", I2C_DEV_ADDR);

	return 0;
}

/* Retrieves registry data as word (32 bit signed integer) */
static __s32 get_i2c_data(int file, unsigned int daddress)
{
	union i2c_smbus_data data;
	struct i2c_smbus_ioctl_data args;

	args.read_write = (unsigned char)I2C_SMBUS_READ;
	args.command = (unsigned char)daddress;
	args.size = (unsigned int)I2C_SMBUS_WORD_DATA;
	args.data = &data;

	if (ioctl(file, I2C_SMBUS, &args))
		return -1;
	else
		return 0x0FFFF & data.word;
}

/* Reads data from registry address */
static int i2c_read_raw(unsigned int daddress, unsigned int *data)
{
	int res, file;
	*data = 0;

	file = open_i2c();

	if (file < 0)
		return -8;

	if (read_i2c(file))
	{
		if (close(file) < 0)
			g_warning("Could not close I2C device file: %s", strerror(errno));
		return -8;
	}

	res = get_i2c_data(file, daddress);
	int read_errno = errno;

	if (close(file) < 0)
		g_warning("Could not close I2C device file: %s", strerror(errno));

	if (res < 0)
	{
		g_warning("I2C read failed: %s", strerror(read_errno));
		return -4;
	}
	*data = res;

	return 0;
}

/* Swaps leftmost bits with rightmost bits */
static unsigned int swap_data(unsigned int data)
{
	return ((data & 0x00FF) << 8) | ((data & 0xFF00) >> 8);
}

/* Returns battery capacity */
int get_capacity()
{
	int capacity = 0;
	unsigned int data;
	int result = i2c_read_raw(I2C_BATT_DAT, &data);

	if (result < 0)
		return (result);

	capacity = (int)(swap_data(data) / 256);

	g_info("x729 battery capacity: %02d%%.", capacity);

	return capacity;
}

/* Returns battery voltage */
float get_voltage()
{
	float voltage = 0.0;
	unsigned int data;
	int result = i2c_read_raw(I2C_VOLT_DAT, &data);

	if (result < 0)
		return (result);

	voltage = (swap_data(data) * 1.25 / 1000 / 16);

	g_info("x729 battery voltage: %02.02fV.", voltage);

	return voltage;
}

/* Returns device status */
int get_devicestatus()
{
	int file = open_i2c();
	if (file < 0)
		return 0; // Unknown

	if (close(file) < 0)
		g_warning("Could not close I2C device file: %s", strerror(errno));

	g_info("x729 is present.");

	return 1; // Present
}

/* Returns power source */
int get_powersource()
{
	// uses BCM numbering of the GPIOs and directly accesses the GPIO registers.
	if (wiringPiSetupGpio() == -1)
	{
		g_warning("Could not initialize WiringPi GPIO access.");
		return -1;
	}

  	// set BCM GPIO pin to input
  	pinMode(BCM_GPIO_PIN, INPUT);

  	// get state BCM GPIO pin
  	int value = digitalRead(BCM_GPIO_PIN);

  	if (HIGH == value)
  	{
		g_info("x729 is running on battery.");
		return 1;
  	}
	
	g_info("x729 is running on AC adapter.");
	return 0;
}

/* Updates device status, voltage and capacity */
int battery_update(battery *b)
{
	g_return_val_if_fail(b != NULL, 0);

	b->devicestatus = get_devicestatus();
	if (b->devicestatus != 1)
		return 0;

	float voltage = get_voltage();
	if (voltage < 0.0f)
	{
		g_warning("Could not read x729 battery voltage.");
		return 0;
	}

	int percentage = get_capacity();
	if (percentage < 0)
	{
		g_warning("Could not read x729 battery capacity.");
		return 0;
	}

	b->voltage = voltage;
	b->percentage = percentage > 100 ? 100 : percentage;
	b->powersource = get_powersource();

	return 1;
}

/* Retrieves battery information */
battery *battery_get()
{
	battery *b = battery_new();

	if (!battery_update(b))
	{
		g_free(b);
		return NULL;
	}

	return b;
}
