/* Phần giao diện, không biết gì về /dev/mpu6050: nhận số đã đổi sang mg. */
#ifndef UI_H
#define UI_H

#include <stdint.h>

void ui_create(unsigned rate_hz, unsigned range_g);
void ui_set_values(int x_mg, int y_mg, int z_mg);
void ui_push_point(int x_mg, int y_mg, int z_mg);
void ui_set_dropped(uint64_t dropped);

#endif
