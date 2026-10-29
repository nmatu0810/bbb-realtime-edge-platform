// SPDX-License-Identifier: MIT
/*
 * Chương trình test /dev/mpu6050 (project B, GĐ1). Chỉ dùng hợp đồng trong
 * <projectb/mpu6050.h>: poll() + read() bản ghi nhị phân, ioctl lấy cấu hình và bộ
 * đếm. Không đọc sysfs (sysfs chỉ để cấu hình, ROADMAP).
 *
 *   mpu6050-test [-n N]   đọc liên tục, mỗi giây in một dòng; -n: dừng sau N bản ghi
 *   mpu6050-test -t       tự kiểm ngữ nghĩa API, in PASS/FAIL, mã thoát = số FAIL
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <projectb/mpu6050.h>

#define DEV_PATH	"/dev/mpu6050"
#define BATCH		64	/* bản ghi mỗi lần read */

static volatile sig_atomic_t stop;

static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

/* Cùng đồng hồ với ktime_get_ns() trong driver. */
static uint64_t now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000u + ts.tv_nsec;
}

static void print_config(const struct mpu6050_config *c)
{
	printf("chip 0x%02x, %u Hz, +-%u g (%u LSB/g), accel DLPF %u Hz\n",
	       c->whoami, c->sample_rate_hz, c->accel_range_g, c->accel_lsb_per_g,
	       c->accel_bw_hz);
}

static int stream(long limit)
{
	struct mpu6050_sample buf[BATCH];
	struct mpu6050_config cfg;
	struct mpu6050_stats st0, st;
	struct pollfd pfd;
	uint64_t win_start = 0, prev_ts = 0, max_gap = 0;
	long total = 0, win_n = 0;
	int fd;

	fd = open(DEV_PATH, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open " DEV_PATH);
		return 1;
	}
	if (ioctl(fd, MPU6050_IOC_GET_CONFIG, &cfg) || ioctl(fd, MPU6050_IOC_GET_STATS, &st0)) {
		perror("ioctl");
		close(fd);
		return 1;
	}
	print_config(&cfg);

	pfd.fd = fd;
	pfd.events = POLLIN;
	while (!stop && (limit <= 0 || total < limit)) {
		int r = poll(&pfd, 1, 1000);

		if (r < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			break;
		}
		if (r == 0) {
			fprintf(stderr, "poll: 1 s without data\n");
			continue;
		}

		/* Rút hết rồi mới poll lại: read trả bội số 16 byte, EAGAIN là đã rỗng. */
		for (;;) {
			ssize_t n = read(fd, buf, sizeof(buf));
			const struct mpu6050_sample *s;
			int i, cnt;

			if (n < 0) {
				if (errno != EAGAIN && errno != EINTR)
					perror("read");
				break;
			}
			cnt = n / sizeof(buf[0]);
			for (i = 0; i < cnt; i++) {
				s = &buf[i];
				if (prev_ts && s->ts_ns - prev_ts > max_gap)
					max_gap = s->ts_ns - prev_ts;
				prev_ts = s->ts_ns;
				if (!win_start)
					win_start = s->ts_ns;
				win_n++;
				total++;
			}
			s = &buf[cnt - 1];

			/*
			 * Mỗi giây theo ts_ns của driver một dòng: tốc độ đo được, mẫu cuối.
			 * Lấy lại cấu hình mỗi dòng vì sysfs đổi được giữa chừng; driver xóa
			 * fifo khi đổi nên chỉ lệch nếu đổi trúng giữa read và ioctl này.
			 * sensord GĐ2 là bên duy nhất đổi cấu hình nên không cần làm vậy.
			 */
			if (s->ts_ns - win_start >= 1000000000u) {
				double lsb;

				ioctl(fd, MPU6050_IOC_GET_CONFIG, &cfg);
				ioctl(fd, MPU6050_IOC_GET_STATS, &st);
				lsb = cfg.accel_lsb_per_g;
				printf("%5ld/s [%u Hz +-%ug]  ax %+6.3f ay %+6.3f az %+6.3f g  "
				       "%5.2f C  max gap %5.1f ms  dropped +%llu  age %.1f ms\n",
				       win_n, cfg.sample_rate_hz, cfg.accel_range_g,
				       s->ax / lsb, s->ay / lsb, s->az / lsb,
				       (s->temp * 100000.0 / cfg.temp_sens_x100 +
					cfg.temp_offset_mc) / 1000.0,
				       max_gap / 1e6,
				       (unsigned long long)(st.dropped - st0.dropped),
				       (now_ns() - s->ts_ns) / 1e6);
				fflush(stdout);
				win_start = s->ts_ns;
				win_n = 0;
				max_gap = 0;
			}
		}
	}

	ioctl(fd, MPU6050_IOC_GET_STATS, &st);
	printf("total %ld records, dropped +%llu\n", total,
	       (unsigned long long)(st.dropped - st0.dropped));
	close(fd);
	return 0;
}

/* ---- tự kiểm ---- */

static int fails;

static void check(int ok, const char *what, const char *detail)
{
	printf("%s  %s%s%s\n", ok ? "PASS" : "FAIL", what, detail ? ": " : "",
	       detail ? detail : "");
	if (!ok)
		fails++;
}

/* Gọi rồi so errno: trả 1 khi ret < 0 và errno đúng. */
static int expect_errno(long ret, int want, char *detail, size_t len)
{
	if (ret >= 0)
		snprintf(detail, len, "returned %ld, want %s", ret, strerror(want));
	else
		snprintf(detail, len, "%s", strerror(errno));
	return ret < 0 && errno == want;
}

static int selftest(void)
{
	struct mpu6050_sample buf[4];
	struct mpu6050_config cfg;
	struct mpu6050_stats stats;
	struct pollfd pfd;
	char d[128];
	uint64_t t;
	ssize_t n;
	int fd, fd2, r;

	fd = open(DEV_PATH, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open " DEV_PATH);
		return 1;
	}

	/* Mẫu đầu đến sau một chu kỳ kể từ open: đọc ngay phải rỗng. */
	n = read(fd, buf, sizeof(buf));
	check(expect_errno(n, EAGAIN, d, sizeof(d)), "read right after open -> EAGAIN", d);

	fd2 = open(DEV_PATH, O_RDONLY | O_NONBLOCK);
	check(expect_errno(fd2, EBUSY, d, sizeof(d)), "second open -> EBUSY", d);
	if (fd2 >= 0)
		close(fd2);

	r = ioctl(fd, MPU6050_IOC_GET_CONFIG, &cfg);
	snprintf(d, sizeof(d), "ret %d", r);
	check(r == 0 && cfg.sample_rate_hz >= 4 && cfg.sample_rate_hz <= 1000 &&
	      cfg.accel_lsb_per_g == 16384u / (cfg.accel_range_g / 2),
	      "GET_CONFIG consistent", d);
	if (r == 0)
		print_config(&cfg);

	r = ioctl(fd, MPU6050_IOC_GET_STATS, &stats);
	snprintf(d, sizeof(d), "ret %d, dropped %llu", r, (unsigned long long)stats.dropped);
	check(r == 0, "GET_STATS", d);

	r = ioctl(fd, _IO(MPU6050_IOC_MAGIC, 0x7f));
	check(expect_errno(r, ENOTTY, d, sizeof(d)), "unknown ioctl -> ENOTTY", d);

	check(expect_errno(lseek(fd, 0, SEEK_SET), ESPIPE, d, sizeof(d)),
	      "lseek -> ESPIPE", d);

	check(expect_errno(write(fd, buf, sizeof(buf[0])), EBADF, d, sizeof(d)),
	      "write on O_RDONLY fd -> EBADF (VFS)", d);

	/* Chờ dữ liệu bằng poll, trong vòng hai chu kỳ cộng khoảng dư. */
	pfd.fd = fd;
	pfd.events = POLLIN;
	t = now_ns();
	r = poll(&pfd, 1, 2000000 / cfg.sample_rate_hz / 1000 + 100);
	snprintf(d, sizeof(d), "ret %d after %.1f ms", r, (now_ns() - t) / 1e6);
	check(r == 1 && (pfd.revents & POLLIN), "poll -> POLLIN", d);

	n = read(fd, buf, 8);
	check(expect_errno(n, EINVAL, d, sizeof(d)), "read 8 bytes -> EINVAL", d);

	/* Đợi có ít nhất hai bản ghi, đọc với buffer 40 byte: phải ra 32, bỏ phần lẻ. */
	usleep(3 * 1000000 / cfg.sample_rate_hz);
	n = read(fd, buf, 40);
	snprintf(d, sizeof(d), "returned %zd", n);
	check(n == 32, "read 40 bytes -> 32 (whole records only)", d);

	/* ts_ns cùng miền CLOCK_MONOTONIC: mẫu vừa đọc phải trong quá khứ gần. */
	t = now_ns();
	snprintf(d, sizeof(d), "age %.1f ms", n > 0 ? (double)(t - buf[1].ts_ns) / 1e6 : -1.0);
	check(n == 32 && buf[1].ts_ns > buf[0].ts_ns && buf[1].ts_ns < t &&
	      t - buf[1].ts_ns < 1000000000u, "ts_ns monotonic, CLOCK_MONOTONIC", d);

	/* FLUSH bỏ phần đang chờ; producer vẫn chạy nên có thể kịp chen một bản ghi. */
	usleep(5 * 1000000 / cfg.sample_rate_hz);
	r = ioctl(fd, MPU6050_IOC_FLUSH);
	n = read(fd, buf, sizeof(buf));
	snprintf(d, sizeof(d), "ioctl %d, read %zd %s", r, n, n < 0 ? strerror(errno) : "");
	check(r == 0 && ((n < 0 && errno == EAGAIN) || n == sizeof(buf[0])),
	      "FLUSH then read -> EAGAIN (or 1 record race)", d);

	close(fd);

	/* Đóng thì mở lại được. */
	fd = open(DEV_PATH, O_RDONLY);
	check(fd >= 0, "reopen after close", fd < 0 ? strerror(errno) : NULL);
	if (fd >= 0)
		close(fd);

	printf("%d failed\n", fails);
	return fails;
}

int main(int argc, char **argv)
{
	struct sigaction sa;
	long limit = 0;
	int opt;

	while ((opt = getopt(argc, argv, "n:t")) != -1) {
		switch (opt) {
		case 'n':
			limit = strtol(optarg, NULL, 10);
			break;
		case 't':
			return selftest();
		default:
			fprintf(stderr, "usage: %s [-n records] | -t\n", argv[0]);
			return 2;
		}
	}

	/* Không SA_RESTART: Ctrl-C làm poll trả EINTR, vòng lặp thấy stop và thoát. */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	return stream(limit);
}
