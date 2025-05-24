x729batt.so:
	gcc -s -Wall `pkg-config --cflags gtk+-3.0 lxpanel` -shared -fPIC x729batt.c batt_i2c.c -o x729batt.so `pkg-config --libs lxpanel`

clean:
	rm -rf *.so

all: x729batt.so

install: all
	mv x729batt.so /usr/lib/aarch64-linux-gnu/lxpanel/plugins
	chown root:root /usr/lib/aarch64-linux-gnu/lxpanel/plugins/x729batt.so
	chmod 644 /usr/lib/aarch64-linux-gnu/lxpanel/plugins/x729batt.so

uninstall:
	rm -v /usr/lib/aarch64-linux-gnu/lxpanel/plugins/x729batt.so
