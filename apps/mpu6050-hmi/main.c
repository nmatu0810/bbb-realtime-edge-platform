// SPDX-License-Identifier: MIT
/*
 * HMI bản mỏng: đọc /dev/mpu6050 qua poll() rồi vẽ lên /dev/fb0 bằng LVGL.
 *
 * Một luồng, một vòng lặp: poll() chờ dữ liệu với timeout lấy từ lv_timer_handler(),
 * nên không cần khóa giữa phần đọc và phần vẽ. Chỉ đọc, không đổi cấu hình driver.
 * /dev/mpu6050 chỉ cho một tiến trình mở: phải tắt app này trước khi chạy
 * mpu6050-test hay demo.sh. Không chạy app này khi đo real-time.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <projectb/mpu6050.h>

#include "lvgl.h"
#include "ui.h"

#define DEV_PATH	"/dev/mpu6050"
#define FB_PATH		"/dev/fb0"
#define CHART_HZ	25
#define VALUE_HZ	5
#define POLL_MAX_MS	100

/* Trung bình cộng dồn của một khoảng; làm mượt số hiển thị và giảm tốc cho biểu đồ. */
struct avg {
	int32_t sum[3];
	unsigned n, every;
};

static volatile sig_atomic_t stop;

static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

static uint32_t tick_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Trả 1 khi đủ một khoảng, out[] là trung bình theo mg. */
static int avg_add(struct avg *a, const int mg[3], int out[3])
{
	int i;

	for (i = 0; i < 3; i++)
		a->sum[i] += mg[i];
	if (++a->n < a->every)
		return 0;
	for (i = 0; i < 3; i++) {
		out[i] = a->sum[i] / (int32_t)a->n;
		a->sum[i] = 0;
	}
	a->n = 0;
	return 1;
}

static unsigned every_for(unsigned rate_hz, unsigned hz)
{
	return rate_hz > hz ? rate_hz / hz : 1;
}

int main(void)
{
	struct mpu6050_sample buf[64];
	struct mpu6050_config cfg;
	struct mpu6050_stats st;
	struct avg chart_avg = { 0 }, value_avg = { 0 };
	struct pollfd pfd;
	struct sigaction sa;
	lv_display_t *disp;
	uint32_t last_stats;
	int fd, tty, ret = 0;

	/* Driver fbdev của LVGL lỗi mở file chỉ in perror rồi chạy tiếp không có màn. */
	if (access(FB_PATH, W_OK)) {
		fprintf(stderr, "%s: %s (HDMI chua noi? bat man truoc khi bat bo)\n",
			FB_PATH, strerror(errno));
		return 1;
	}

	fd = open(DEV_PATH, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "%s: %s\n", DEV_PATH, strerror(errno));
		return 1;
	}
	if (ioctl(fd, MPU6050_IOC_GET_CONFIG, &cfg)) {
		perror("MPU6050_IOC_GET_CONFIG");
		return 1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* KD_GRAPHICS: fbcon thôi ghi chữ và con trỏ của getty tty1 đè lên hình. */
	tty = open("/dev/tty0", O_RDWR | O_CLOEXEC);
	if (tty >= 0 && ioctl(tty, KDSETMODE, KD_GRAPHICS))
		perror("KDSETMODE");

	lv_init();
	lv_tick_set_cb(tick_ms);
	disp = lv_linux_fbdev_create();
	lv_linux_fbdev_set_file(disp, FB_PATH);
	ui_create(cfg.sample_rate_hz, cfg.accel_range_g);

	chart_avg.every = every_for(cfg.sample_rate_hz, CHART_HZ);
	value_avg.every = every_for(cfg.sample_rate_hz, VALUE_HZ);
	pfd.fd = fd;
	pfd.events = POLLIN;
	last_stats = tick_ms();

	while (!stop) {
		uint32_t wait = lv_timer_handler();
		ssize_t n;

		if (wait > POLL_MAX_MS)
			wait = POLL_MAX_MS;
		if (poll(&pfd, 1, wait) < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			ret = 1;
			break;
		}

		/* Khi thiết bị bị gỡ, poll báo HUP|ERR, read trả nốt mẫu rồi ENODEV. */
		while (pfd.revents && !stop) {
			size_t i, count;

			n = read(fd, buf, sizeof(buf));
			if (n < 0) {
				if (errno == EAGAIN || errno == EINTR)
					break;
				perror("read");
				ret = 1;
				stop = 1;
				break;
			}
			count = (size_t)n / sizeof(buf[0]);
			for (i = 0; i < count; i++) {
				const int lsb = (int)cfg.accel_lsb_per_g;
				const int mg[3] = {
					buf[i].ax * 1000 / lsb,
					buf[i].ay * 1000 / lsb,
					buf[i].az * 1000 / lsb,
				};
				int out[3];

				if (avg_add(&chart_avg, mg, out))
					ui_push_point(out[0], out[1], out[2]);
				if (avg_add(&value_avg, mg, out))
					ui_set_values(out[0], out[1], out[2]);
			}
			if (count < sizeof(buf) / sizeof(buf[0]))
				break;
		}

		if (tick_ms() - last_stats >= 1000) {
			last_stats = tick_ms();
			if (!ioctl(fd, MPU6050_IOC_GET_STATS, &st))
				ui_set_dropped(st.dropped);
		}
	}

	if (tty >= 0) {
		ioctl(tty, KDSETMODE, KD_TEXT);
		close(tty);
	}
	close(fd);
	return ret;
}
