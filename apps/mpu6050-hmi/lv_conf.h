/*
 * Cấu hình LVGL cho mpu6050-hmi. Chỉ ghi chỗ khác mặc định, còn lại do
 * src/lv_conf_internal.h của LVGL điền.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Khớp fb0; driver fbdev đặt lại định dạng màu theo bits_per_pixel lúc chạy. */
#define LV_COLOR_DEPTH 16

#define LV_MEM_SIZE (128 * 1024U)

#define LV_USE_LINUX_FBDEV 1
#define LV_LINUX_FBDEV_BSD 0
#define LV_LINUX_FBDEV_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#define LV_LINUX_FBDEV_BUFFER_COUNT 0
#define LV_LINUX_FBDEV_BUFFER_SIZE 60

/* Chỉ cần hai cỡ chữ; font nằm trong .rodata của binary. */
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#endif
