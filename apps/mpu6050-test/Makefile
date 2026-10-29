# Yocto truyền CC, CFLAGS, LDFLAGS (recipe mpu6050-test); bỏ LDFLAGS là lỗi QA
# "No GNU_HASH". Header <projectb/mpu6050.h> đến từ sysroot (recipe mpu6050-uapi).
# Thử trên laptop: make CPPFLAGS=-I../../driver/mpu6050/include/uapi
CFLAGS ?= -O2 -g
override CFLAGS += -Wall -Wextra
bindir ?= /usr/bin

mpu6050-test: mpu6050-test.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

install: mpu6050-test
	install -D -m 0755 mpu6050-test $(DESTDIR)$(bindir)/mpu6050-test

clean:
	rm -f mpu6050-test

.PHONY: install clean
