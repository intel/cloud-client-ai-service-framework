SOURCE = nss_ccai.c conf.c init.c
TARGET = libnss_ccai.so.2
CFLAGS = -g -O0

$(TARGET): $(SOURCE)
	$(CC) -o $(TARGET) $(CFLAGS) -shared -fpic $(SOURCE)

test: /lib/x86_64-linux-gnu/$(TARGET)
	NSS_CCAI_CONF=test.conf getent ahosts asdf3

/lib/x86_64-linux-gnu/$(TARGET): $(TARGET)
	sudo cp -fp $(TARGET) /lib/x86_64-linux-gnu/

clean:
	rm -f $(TARGET) $(SOURCE:.c=.o)

install: $(TARGET)
	cp -fp $(TARGET) /lib/x86_64-linux-gnu/
	sed -i '/^hosts:/{s/ ccai / /g;s/ dns/ ccai dns/g}' /etc/nsswitch.conf

uninstall:
	rm -f /lib/x86_64-linux-gnu/$(TARGET)
	sed -i '/^hosts:/s/ ccai / /g' /etc/nsswitch.conf
