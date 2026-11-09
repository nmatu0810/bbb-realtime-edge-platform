/*
 * Một màn hình: ba số X/Y/Z và một biểu đồ đường. Không menu, không cảnh báo.
 * Font Montserrat của LVGL chỉ có ASCII nên chữ trên màn không dấu.
 */
#include <stdio.h>

#include "lvgl.h"
#include "ui.h"

#define CHART_POINTS 256

#define COLOR_BG	lv_color_hex(0x12161c)
#define COLOR_CARD	lv_color_hex(0x1e242c)
#define COLOR_MUTED	lv_color_hex(0x9aa5b1)

static const uint32_t axis_color[3] = { 0xff6b6b, 0x51cf66, 0x4dabf7 };
static const char *const axis_name[3] = { "X", "Y", "Z" };

static lv_obj_t *value_label[3];
static lv_obj_t *status_label;
static lv_obj_t *chart;
static lv_chart_series_t *series[3];
static unsigned status_rate, status_range;

static void tile_create(lv_obj_t *parent, int i)
{
	lv_obj_t *tile = lv_obj_create(parent);
	lv_obj_t *name;

	lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_flex_grow(tile, 1);
	lv_obj_set_height(tile, 110);
	lv_obj_set_style_bg_color(tile, COLOR_CARD, 0);
	lv_obj_set_style_border_width(tile, 0, 0);
	lv_obj_set_style_radius(tile, 6, 0);
	lv_obj_set_style_pad_all(tile, 10, 0);

	name = lv_label_create(tile);
	lv_label_set_text_fmt(name, "%s (g)", axis_name[i]);
	lv_obj_set_style_text_color(name, lv_color_hex(axis_color[i]), 0);
	lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 0);

	value_label[i] = lv_label_create(tile);
	lv_label_set_text(value_label[i], "--");
	lv_obj_set_style_text_font(value_label[i], &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(value_label[i], lv_color_white(), 0);
	lv_obj_align(value_label[i], LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void ui_create(unsigned rate_hz, unsigned range_g)
{
	lv_obj_t *scr = lv_screen_active();
	lv_obj_t *row;
	int i;

	status_rate = rate_hz;
	status_range = range_g;

	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
	lv_obj_set_style_text_color(scr, lv_color_white(), 0);
	lv_obj_set_style_pad_all(scr, 12, 0);
	lv_obj_set_style_pad_row(scr, 10, 0);
	lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

	row = lv_obj_create(scr);
	lv_obj_remove_style_all(row);
	lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
	lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
	lv_obj_set_style_pad_column(row, 10, 0);
	for (i = 0; i < 3; i++)
		tile_create(row, i);

	chart = lv_chart_create(scr);
	lv_obj_set_width(chart, LV_PCT(100));
	lv_obj_set_flex_grow(chart, 1);
	lv_obj_set_style_bg_color(chart, COLOR_CARD, 0);
	lv_obj_set_style_border_width(chart, 0, 0);
	lv_obj_set_style_radius(chart, 6, 0);
	lv_obj_set_style_line_color(chart, lv_color_hex(0x38414d), LV_PART_MAIN);
	lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
	lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
	lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
	lv_chart_set_div_line_count(chart, 5, 9);
	lv_chart_set_point_count(chart, CHART_POINTS);
	lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y,
			   -(int32_t)range_g * 1000, (int32_t)range_g * 1000);
	/* CIRCULAR chỉ vẽ lại quanh điểm mới; SHIFT vẽ lại cả biểu đồ mỗi điểm. */
	lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_CIRCULAR);
	for (i = 0; i < 3; i++)
		series[i] = lv_chart_add_series(chart, lv_color_hex(axis_color[i]),
						LV_CHART_AXIS_PRIMARY_Y);

	status_label = lv_label_create(scr);
	lv_obj_set_style_text_color(status_label, COLOR_MUTED, 0);
	ui_set_dropped(0);
}

void ui_set_values(int x_mg, int y_mg, int z_mg)
{
	const int v[3] = { x_mg, y_mg, z_mg };
	char buf[16];
	int i;

	for (i = 0; i < 3; i++) {
		snprintf(buf, sizeof(buf), "%c%d.%03d", v[i] < 0 ? '-' : '+',
			 (v[i] < 0 ? -v[i] : v[i]) / 1000,
			 (v[i] < 0 ? -v[i] : v[i]) % 1000);
		lv_label_set_text(value_label[i], buf);
	}
}

void ui_push_point(int x_mg, int y_mg, int z_mg)
{
	lv_chart_set_next_value(chart, series[0], x_mg);
	lv_chart_set_next_value(chart, series[1], y_mg);
	lv_chart_set_next_value(chart, series[2], z_mg);
}

void ui_set_dropped(uint64_t dropped)
{
	char buf[80];

	snprintf(buf, sizeof(buf), "/dev/mpu6050   %u Hz   +/-%u g   dropped %llu",
		 status_rate, status_range, (unsigned long long)dropped);
	lv_label_set_text(status_label, buf);
}
