# LVGL biên dịch thẳng vào app (tĩnh), chỉ src/*.c: bỏ demos, examples, thorvg.
# LVGL_DIR do recipe chỉ tới bản đã ghim; thử trên laptop:
#   make LVGL_DIR=/đường/dẫn/lvgl CPPFLAGS=-I../../driver/mpu6050/include/uapi
LVGL_DIR ?= lvgl
CFLAGS ?= -O2 -g
override CPPFLAGS += -DLV_CONF_INCLUDE_SIMPLE -I. -I$(LVGL_DIR)
bindir ?= /usr/bin

LVGL_SRCS := $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_OBJS := $(patsubst $(LVGL_DIR)/%.c,obj/lvgl/%.o,$(LVGL_SRCS))
APP_OBJS := obj/main.o obj/ui.o

mpu6050-hmi: $(APP_OBJS) $(LVGL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

obj/lvgl/%.o: $(LVGL_DIR)/%.c lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

obj/%.o: %.c ui.h lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -c -o $@ $<

install: mpu6050-hmi
	install -D -m 0755 mpu6050-hmi $(DESTDIR)$(bindir)/mpu6050-hmi

clean:
	rm -rf obj mpu6050-hmi

.PHONY: install clean
