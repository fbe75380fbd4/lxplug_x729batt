/*
 *      batt_i2c.h
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

#ifndef BATT_I2C_H
#define BATT_I2C_H

#define I2C_DEV_PATH "/dev/i2c-%d" // I2C device path
#define I2C_DEV_BUS 1              // I2C device bus
#define I2C_DEV_ADDR 0x36          // I2C address
#define I2C_VOLT_DAT 2             // I2C voltage data address
#define I2C_BATT_DAT 4             // I2C battery data address

#define I2C_SMBUS_READ 1                         // I2C READ COMMAND
#define I2C_SMBUS_WORD_DATA 3                    // I2C WORD DATA TYPE
#define I2C_FUNC_SMBUS_READ_WORD_DATA 0x00200000 // I2C READ WORD DATA CAPABILITY

#define BCM_GPIO_PIN 6 // BCM numbering for GPIO register

#define VOLTAGE_REGISTER_LSB_MILLIVOLTS 1.25
#define MILLIVOLTS_PER_VOLT 1000.0
#define VOLTAGE_REGISTER_SCALE 16.0

/* SMBus response message */
union i2c_smbus_data
{
    unsigned int word;
};

/* Battery global data */
typedef struct battery
{
    float voltage;      // Voltage reading from I2C device
    int percentage;     // Capacity percentage reading from I2C device
    int device_status;  // I2C device state (present or unknown)
    int power_source;   // Running on battery or power supply
} battery;

/* Prototypes */
battery *battery_get();
int battery_update(battery *b);
int get_capacity();
int get_device_status();
int get_power_source();
float get_voltage();

#endif