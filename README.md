# lxplug_x729batt

## Description

LXDE panel plug-in for Geekworm's x729 Raspberry Pi Hat.

## Features

- Reads battery information through I2C device interface
- Displays tooltip with battery voltage and capacity/charge level
- Displays low battery (<20%) and low voltage (<3.33V) warning notifications
- Supported language(s): English only

## Caveats

- No external power supply detection: Low battery warnings will continue to be displayed until charge level is above 20%.

## Installation

```bash
$ sudo apt -y install lxpanel-dev
$ git clone https://github.com/fbe75380fbd4/lxplug_x729batt.git
$ cd lxplug_x729batt
$ make
$ sudo make install
$ lxpanelctl restart
```

## Uninstallation

```bash
$ sudo make uninstall
$ lxpanelctl restart
```

## Requirements

### I2C

Enable I2C interface using [raspi-config](https://www.raspberrypi.com/documentation/computers/configuration.html#interface-options) tool:

1. Select option 3, `Interface Options`
2. Then, select option I5, `I2C`
3. Now, select `Yes`
4. Reboot to apply changes

### Desktop Environment

Raspberry Pi Desktop is moving from X11 (LXDE) to Wayland (Wayfire), this plugin is intended for `lxpanel` (LXDE).

Switch from Wayland to X11 using [raspi-config](https://www.raspberrypi.com/documentation/computers/configuration.html#advanced-options) tool:

1. Select option 6, `Advanced Options`
2. Then, select option A6, `Wayland`
3. Now, select option W1, `X11`
4. Reboot to apply changes

### x729

Once I2C is enabled, verify board is working as expected.

1. Install `i2c-tools` package
2. Run command `sudo i2cdetect -y 1`
3. Confirm address `36` is listed, it belongs to the battery fuel gauging chip

```text
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- -- 
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
30: -- -- -- -- -- -- 36 -- -- -- -- -- -- -- -- -- 
40: 40 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
60: -- -- -- -- -- -- -- -- 68 -- -- -- -- -- -- -- 
70: -- -- -- -- -- -- -- --
```

4. Download and run the python script from Geekworm

```bash
$ wget 'https://raw.githubusercontent.com/geekworm-com/x729-script/main/sample/bat.py'
$ chmod +x bat.py
$ ./bat.py

******************
Voltage: 4.13V
Battery:   93%
******************
...
```

## References

- https://wiki.geekworm.com/X729
- https://wiki.geekworm.com/X729-script
- https://github.com/geekworm-com/x729-script/blob/main/sample/bat.py
- https://github.com/omapconf/omapconf/tree/master/i2c-tools
- https://github.com/raspberrypi-ui/lxplug-ptbatt/
- https://web.archive.org/web/20220422124713/https://wiki.lxde.org/en/How_to_write_plugins_for_LXPanel
