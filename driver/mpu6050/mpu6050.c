// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver MPU6050 / MPU-6500 trên I2C cho project B (BeagleBone Black).
 *
 * Chip thật trên module là họ MPU-6500 (WHO_AM_I = 0x70), tên mpu6050 giữ cho cả
 * project. Thanh ghi theo MPU-6500 Register Map rev 2.1 (RM-MPU-6500A-00); thông
 * số MPU-6050 lấy theo inv_mpu6050 mainline, chưa kiểm trên chip thật.
 *
 * Probe reset chip, ghi tường minh mọi thanh ghi cấu hình rồi đọc lại để kiểm;
 * remove cho chip ngủ. Tuần 5: /dev/mpu6050 (misc device) với open, release,
 * read, poll trên kfifo; sysfs sample_rate, accel_range, dropped; ioctl
 * GET_CONFIG, GET_STATS, FLUSH. Tuần 6: nguồn dữ liệu là ngắt DATA_RDY, threaded
 * IRQ (thay hrtimer + work của tuần 5). Tuần 7: chip ghi accel + temp vào FIFO của
 * nó, thread đọc theo lô (~10 ms), mỗi mẫu mang ts của chính cạnh INT của nó.
 */
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kfifo.h>
#include <linux/kstrtox.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/property.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <projectb/mpu6050.h>

#define MPU6050_REG_SMPLRT_DIV		0x19
#define MPU6050_REG_CONFIG		0x1A
#define MPU6050_REG_GYRO_CONFIG		0x1B
#define MPU6050_REG_ACCEL_CONFIG	0x1C
#define MPU6050_REG_ACCEL_CONFIG2	0x1D	/* chỉ có ở họ 6500 */
#define MPU6050_REG_FIFO_EN		0x23
#define MPU6050_REG_INT_PIN_CFG		0x37
#define MPU6050_REG_INT_ENABLE		0x38
#define MPU6050_REG_ACCEL_XOUT_H	0x3B
#define MPU6050_REG_SIGNAL_PATH_RESET	0x68
#define MPU6050_REG_USER_CTRL		0x6A
#define MPU6050_REG_PWR_MGMT_1		0x6B
#define MPU6050_REG_PWR_MGMT_2		0x6C
#define MPU6050_REG_FIFO_COUNT_H	0x72	/* đọc byte H chốt cả H lẫn L */
#define MPU6050_REG_FIFO_R_W		0x74
#define MPU6050_REG_WHO_AM_I		0x75

#define MPU6050_PWR_MGMT_1_DEVICE_RESET	BIT(7)
#define MPU6050_PWR_MGMT_1_SLEEP	BIT(6)
#define MPU6050_PWR_MGMT_1_CLKSEL_PLL	0x01	/* PLL khi sẵn sàng, không thì dao động trong */
#define MPU6050_SIGNAL_PATH_RESET_ALL	0x07	/* GYRO_RST | ACCEL_RST | TEMP_RST */
#define MPU6050_INT_ENABLE_RAW_RDY	BIT(0)
/* FIFO ghi theo thứ tự số thanh ghi: ACCEL (0x3B..0x40) rồi TEMP (0x41..0x42), cùng
 * bố cục với burst 8 byte và với bốn trường đầu của mpu6050_sample. */
#define MPU6050_FIFO_EN_TEMP		BIT(7)
#define MPU6050_FIFO_EN_ACCEL		BIT(3)
#define MPU6050_USER_CTRL_FIFO_EN	BIT(6)
#define MPU6050_USER_CTRL_FIFO_RST	BIT(2)	/* tự xóa */
#define MPU6050_FIFO_FRAME		8
#define MPU6050_FIFO_MAX		1024	/* FIFO lớn nhất trong mpu6050_chips[] */

/* Chờ sau mỗi bước reset, như inv_mpu6050 (INV_MPU6050_POWER_UP_TIME). */
#define MPU6050_RESET_DELAY_MS		100

/* Burst ACCEL_XOUT_H..TEMP_OUT_L: ax ay az temp, big-endian s16, đúng bốn trường đầu
 * của mpu6050_sample. Gyro không có trong bản ghi nên không đọc. */
#define MPU6050_SAMPLE_REGS		4

/* Tốc độ lấy mẫu nội khi DLPF bật; sample_rate = 1000 / (1 + SMPLRT_DIV). */
#define MPU6050_INTERNAL_RATE_HZ	1000
/* sysfs sample_rate nhận 4..1000 Hz: SMPLRT_DIV 249..0. */
#define MPU6050_RATE_MIN_HZ		4
#define MPU6050_RATE_MAX_HZ		1000
/* Mặc định lúc probe: 100 Hz, ±2 g */
#define MPU6050_DEFAULT_RATE_DIV	9
#define MPU6050_DEFAULT_ACCEL_FS	0

/* Bản ghi chờ đọc, phải là lũy thừa của 2: 256 × 16 byte = 4 KB, 2.56 s ở 100 Hz. */
#define MPU6050_FIFO_LEN		256
/* ts các cạnh INT chưa ghép với mẫu: lớn hơn FIFO của chip (64 hay 128 mẫu), nên
 * FIFO chip tràn trước hàng đợi này. */
#define MPU6050_EDGE_LEN		128
/* Lô mặc định: đánh thức thread khoảng 100 lần/s bất kể tốc độ. */
#define MPU6050_BATCH_HZ		100
/* Neo mẫu mới nhất của FIFO vào cạnh mới nhất chỉ khi đọc xong FIFO_COUNT trước
 * nhịp lấy mẫu kế, chừa khoảng này cho độ trễ cạnh → nửa trên (đo tuần 6: max
 * 208 µs trên kernel debug). */
#define MPU6050_ANCHOR_MARGIN_NS	(300 * NSEC_PER_USEC)
/*
 * Lần đọc lô phải xong trước lần ghi kế của chip (xem mpu6050_batch). Từ cạnh tới lúc
 * bắt đầu đọc khung (đánh thức thread + đọc FIFO_COUNT): ~450 µs trên LA, 400 kHz,
 * kernel debug; chừa thêm 150 µs trước cạnh kế.
 */
#define MPU6050_READ_START_NS		(450 * NSEC_PER_USEC)
#define MPU6050_WRITE_GUARD_NS		(150 * NSEC_PER_USEC)
/*
 * Không đụng chip trong lúc xung INT (50 µs) sau một mẫu: trên non-RT thread đọc count
 * chỉ ~26 µs sau cạnh và chip thỉnh thoảng NACK byte thanh ghi (LA 2026-10-30, 21 µs;
 * chờ tới 60 µs sau cạnh thì 0 NACK). Mốc là cạnh, nhưng driver chỉ có ts: trên RT ts
 * lấy trong thread irq/26, đã trễ hơn cạnh ít nhất ~40 µs (ftrace: irq26 → irq50 min
 * 35..42 µs), trên non-RT lấy trong hardirq, gần như ngay cạnh. Chờ thừa trên RT đẩy lô
 * qua lúc chip ghi mẫu kế (1000 Hz: lệch FIFO 17..36/30 s thay vì 0..4).
 */
#define MPU6050_TS_LAG_MIN_NS		(IS_ENABLED(CONFIG_PREEMPT_RT) ? 40 * NSEC_PER_USEC : 0)
#define MPU6050_SETTLE_NS		(60 * NSEC_PER_USEC - MPU6050_TS_LAG_MIN_NS)

/* Bit trong mpu6050::flags */
#define MPU6050_FLAG_OPEN		0

struct mpu6050_chip {
	const char *name;
	u8 whoami;
	u8 pwr_mgmt_1_reset;		/* PWR_MGMT_1 ngay sau DEVICE_RESET */
	bool has_accel_config2;
	bool signal_path_reset;
	int temp_sens_x100;		/* LSB/°C × 100 */
	int temp_offset_mc;		/* m°C khi TEMP_OUT = 0 */
	unsigned int fifo_size;		/* byte */
};

/*
 * Trạng thái một chip. Cấp bằng devm nên được giải phóng ngay sau remove: unbind khi
 * fd còn mở thì file trỏ vào vùng đã giải phóng (rmmod thì không, vì fops.owner giữ
 * refcount module). Tuần 8 sửa bằng kref; trước đó đừng unbind khi có fd mở.
 *
 * Khóa: cfg_lock rồi mới read_lock, không bao giờ ngược lại. Phía ghi fifo chỉ có
 * một producer (thread IRQ) nên không khóa. Thread IRQ không lấy khóa nào của
 * driver: mpu6050_stop chờ nó trong lúc giữ cfg_lock.
 */
struct mpu6050 {
	struct i2c_client *client;
	const struct mpu6050_chip *chip;
	struct miscdevice misc;

	/* Thanh ghi chip, cấu hình cache, bật/tắt nguồn dữ liệu. */
	struct mutex cfg_lock;
	unsigned int rate_div;		/* SMPLRT_DIV đang ghi trong chip */
	unsigned int accel_fs;		/* ACCEL_FS_SEL 0..3: ±2, 4, 8, 16 g */
	bool streaming;

	/*
	 * Nguồn dữ liệu: ngắt DATA_RDY + FIFO của chip. edges: ts các cạnh INT chưa ghép
	 * với mẫu, nửa trên ghi, thread đọc (một người ghi một người đọc, không khóa).
	 * edges_lost: cạnh không vào được edges (đầy) từ lần thread trước. inflight:
	 * số cạnh thread đã chụp mà chưa bỏ khỏi edges, để nửa trên chỉ đếm cạnh mới
	 * khi so với batch. batch,
	 * period_ns chỉ đổi lúc ngắt tắt (mpu6050_start). Các trường còn lại chỉ thread
	 * dùng.
	 */
	int irq;
	DECLARE_KFIFO(edges, u64, MPU6050_EDGE_LEN);
	atomic_t edges_lost;
	unsigned int inflight;
	unsigned int batch;
	u64 period_ns;
	u32 bus_hz;			/* clock-frequency của adapter */
	u64 last_edge;			/* ts cạnh mới nhất đã ghép, 0 sau start */
	bool reset_pending;		/* reset FIFO lỗi giữa chừng, lô sau làm lại */
	u64 edge_ts[MPU6050_EDGE_LEN];
	u64 frame_ts[MPU6050_FIFO_MAX / MPU6050_FIFO_FRAME];
	u8 fill[MPU6050_EDGE_LEN];		/* mẫu không cạnh trước mỗi cạnh của lô */
	u8 fifo_buf[MPU6050_FIFO_MAX];
	/* Chẩn đoán (debugfs, chỉ thread ghi): lô neo được / không, mẫu không có cạnh,
	 * số lần reset FIFO vì đầy, lệch hay đọc lỗi. */
	u32 n_anchored, n_unanchored, n_extra, n_reset, n_misaligned, n_settle;

	/*
	 * Phía đọc của fifo. Một người mở chưa phải một người đọc: hai luồng (hoặc
	 * fork) dùng chung fd vẫn read() song song được. Mutex vì copy_to_user ngủ.
	 */
	struct mutex read_lock;
	DECLARE_KFIFO(fifo, struct mpu6050_sample, MPU6050_FIFO_LEN);
	wait_queue_head_t wq;
	atomic64_t dropped;		/* fifo đầy hoặc lỡ nhịp; u64 nên phải atomic trên ARM32 */

	unsigned long flags;
};

static const struct mpu6050_chip mpu6050_chips[] = {
	{
		.name = "MPU-6500",
		.whoami = 0x70,
		.pwr_mgmt_1_reset = 0x01,
		.has_accel_config2 = true,
		.signal_path_reset = true,
		.temp_sens_x100 = 33387,
		.temp_offset_mc = 21000,
		.fifo_size = 512,
	},
	{
		.name = "MPU-6050",
		.whoami = 0x68,
		.pwr_mgmt_1_reset = 0x40,
		.has_accel_config2 = false,
		.signal_path_reset = false,
		.temp_sens_x100 = 34000,
		.temp_offset_mc = 36530,
		.fifo_size = 1024,
	},
};

static const struct mpu6050_chip *mpu6050_find_chip(u8 whoami)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mpu6050_chips); i++)
		if (mpu6050_chips[i].whoami == whoami)
			return &mpu6050_chips[i];
	return NULL;
}

/* Ghi rồi đọc lại: lỗi bus và giá trị không "ăn" đều làm probe thất bại. */
static int mpu6050_write_check(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_err(&client->dev, "write reg 0x%02x failed: %d\n", reg, ret);
		return ret;
	}

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "read back reg 0x%02x failed: %d\n", reg, ret);
		return ret;
	}
	if (ret != val) {
		dev_err(&client->dev, "reg 0x%02x: wrote 0x%02x, read 0x%02x\n",
			reg, val, ret);
		return -EIO;
	}
	return 0;
}

/*
 * Reset bằng DEVICE_RESET thay vì tin giá trị lúc cấp nguồn: insmod/rmmod không làm
 * chip mất nguồn, lần probe sau gặp lại cấu hình (và trạng thái ngủ) của lần trước.
 */
static int mpu6050_reset(struct i2c_client *client,
			 const struct mpu6050_chip *chip)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1,
					MPU6050_PWR_MGMT_1_DEVICE_RESET);
	if (ret < 0)
		return ret;
	msleep(MPU6050_RESET_DELAY_MS);

	/* Register Map ghi bước này cho SPI; inv_mpu6050 làm cho cả họ 6500. */
	if (chip->signal_path_reset) {
		ret = i2c_smbus_write_byte_data(client,
						MPU6050_REG_SIGNAL_PATH_RESET,
						MPU6050_SIGNAL_PATH_RESET_ALL);
		if (ret < 0)
			return ret;
		msleep(MPU6050_RESET_DELAY_MS);
	}

	/* DEVICE_RESET tự xóa; thanh ghi phải về đúng giá trị reset. */
	ret = i2c_smbus_read_byte_data(client, MPU6050_REG_PWR_MGMT_1);
	if (ret < 0)
		return ret;
	if (ret != chip->pwr_mgmt_1_reset) {
		dev_err(&client->dev, "PWR_MGMT_1 after reset 0x%02x, expected 0x%02x\n",
			ret, chip->pwr_mgmt_1_reset);
		return -EIO;
	}
	return 0;
}

/*
 * Bộ lọc thông thấp: DLPF_CFG (gyro, nhiệt) và A_DLPF_CFG (accel) 1..6 cho cùng dãy
 * băng thông (Register Map 4.5, 4.8), chỉ số mảng + 1 là giá trị thanh ghi. Không
 * dùng 0 và 7: SMPLRT_DIV chỉ có tác dụng khi 0 < DLPF_CFG < 7.
 */
static const unsigned int mpu6050_dlpf_bw_hz[] = { 184, 92, 41, 20, 10, 5 };

/*
 * Băng thông lớn nhất không quá nửa tốc độ thật (Nyquist). So bằng số nguyên:
 * 2·bw ≤ 1000/(div+1) tương đương 2·bw·(div+1) ≤ 1000, không chia nên không lệch vì
 * làm tròn. Dưới 10 Hz không có phần tử nào đạt, dùng 5 Hz (sẽ có răng cưa).
 */
static unsigned int mpu6050_dlpf_index(unsigned int rate_div)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mpu6050_dlpf_bw_hz) - 1; i++)
		if (2 * mpu6050_dlpf_bw_hz[i] * (rate_div + 1) <= MPU6050_INTERNAL_RATE_HZ)
			break;
	return i;
}

/* Tốc độ và DLPF đi cùng nhau: đổi tốc độ mà giữ bộ lọc thì băng thông có thể vượt
 * Nyquist. */
static int mpu6050_write_rate(struct mpu6050 *st, unsigned int rate_div)
{
	u8 dlpf = mpu6050_dlpf_index(rate_div) + 1;
	int ret;

	/* FIFO_MODE 0, EXT_SYNC_SET 0 */
	ret = mpu6050_write_check(st->client, MPU6050_REG_CONFIG, dlpf);
	if (ret)
		return ret;
	ret = mpu6050_write_check(st->client, MPU6050_REG_SMPLRT_DIV, rate_div);
	if (ret)
		return ret;
	/* ACCEL_FCHOICE_B 0 (đi qua DLPF). 6050 không có thanh ghi này: CONFIG lọc cả
	 * accel. */
	if (st->chip->has_accel_config2)
		ret = mpu6050_write_check(st->client, MPU6050_REG_ACCEL_CONFIG2, dlpf);
	return ret;
}

/* ACCEL_FS_SEL ở bit [4:3], tự kiểm tắt. */
static int mpu6050_write_range(struct mpu6050 *st, unsigned int accel_fs)
{
	return mpu6050_write_check(st->client, MPU6050_REG_ACCEL_CONFIG,
				   accel_fs << 3);
}

/* Ghi mọi thanh ghi cấu hình, kể cả những cái trùng giá trị reset. */
static int mpu6050_configure(struct mpu6050 *st)
{
	struct i2c_client *client = st->client;
	int ret;

	/* PLL lấy nhịp từ gyro, chính xác hơn dao động trong 20 MHz: nhịp lấy mẫu ổn
	 * định thì timestamp nội suy ở tuần 7 mới đúng. Vì vậy giữ gyro bật. */
	ret = mpu6050_write_check(client, MPU6050_REG_PWR_MGMT_1,
				  MPU6050_PWR_MGMT_1_CLKSEL_PLL);
	if (ret)
		return ret;
	ret = mpu6050_write_check(client, MPU6050_REG_PWR_MGMT_2, 0x00);
	if (ret)
		return ret;

	ret = mpu6050_write_rate(st, st->rate_div);
	if (ret)
		return ret;
	/* ±250 °/s, FCHOICE_B 00 (đi qua DLPF) */
	ret = mpu6050_write_check(client, MPU6050_REG_GYRO_CONFIG, 0x00);
	if (ret)
		return ret;
	ret = mpu6050_write_range(st, st->accel_fs);
	if (ret)
		return ret;

	/* INT active-high, push-pull, xung 50 µs: khớp IRQ_TYPE_EDGE_RISING trong
	 * overlay. Xung lặp mỗi mẫu dù không ai đọc INT_STATUS (LA 2026-10-30: 50 xung
	 * rộng 49 µs trong 0.5 s), nên không cần đọc nó. Không bật INT_ANYRD_2CLEAR:
	 * tuần 7 đọc FIFO_COUNT sẽ xóa mất cờ FIFO_OFLOW. INT_ENABLE bật ở
	 * mpu6050_start. */
	ret = mpu6050_write_check(client, MPU6050_REG_INT_PIN_CFG, 0x00);
	if (ret)
		return ret;
	ret = mpu6050_write_check(client, MPU6050_REG_INT_ENABLE, 0x00);
	if (ret)
		return ret;
	/* FIFO bật ở mpu6050_start. */
	ret = mpu6050_write_check(client, MPU6050_REG_FIFO_EN, 0x00);
	if (ret)
		return ret;
	return mpu6050_write_check(client, MPU6050_REG_USER_CTRL, 0x00);
}

/* Tốc độ thật làm tròn xuống, như sysfs sample_rate và mpu6050_config. */
static unsigned int mpu6050_rate_hz(const struct mpu6050 *st)
{
	return MPU6050_INTERNAL_RATE_HZ / (st->rate_div + 1);
}

static unsigned int mpu6050_range_g(const struct mpu6050 *st)
{
	return 2u << st->accel_fs;
}

/* Chỉ để thử nghiệm (tuần 7), không phải giao diện: lô cố định thay cho ~10 ms, và
 * tắt reset FIFO khi tràn để kiểm giả thuyết bản ghi không lệch khi chip ghi đè. */
static unsigned int batch;
module_param(batch, uint, 0644);
MODULE_PARM_DESC(batch, "test: samples per FIFO read, 0 = about 10 ms (read at open)");
static bool oflow_reset = true;
module_param(oflow_reset, bool, 0644);
MODULE_PARM_DESC(oflow_reset, "test: reset the chip FIFO when it overflows");
static unsigned int stall_ms;
module_param(stall_ms, uint, 0644);
MODULE_PARM_DESC(stall_ms, "test: stall the irq thread once for this long (FIFO overflow)");

/*
 * Nửa trên: ghi thời điểm cạnh vào edges, chỉ đánh thức thread khi đủ một lô. Trên
 * RT hàm này chạy trong thread irq/26 của gpio-omap (IRQF_ONESHOT giữ nó khỏi bị ép
 * thành thread riêng, manage.c), nên ts đã gồm độ trễ đánh thức thread đó.
 *
 * gpio-omap dùng handle_simple_irq cho ngắt cạnh, không mask: cạnh tới lúc thread
 * đang đọc vẫn vào đây, nên mỗi mẫu của chip có đúng một ts. edges đầy (thread kẹt
 * hơn 128 mẫu) thì cạnh đó mất: mẫu của nó tính vào dropped ngay, và thread biết qua
 * edges_lost để không trả mẫu đó lần nữa.
 */
static irqreturn_t mpu6050_irq(int irq, void *data)
{
	struct mpu6050 *st = data;

	if (!kfifo_put(&st->edges, ktime_get_ns())) {
		atomic64_inc(&st->dropped);
		atomic_inc(&st->edges_lost);
	}
	/* Cạnh thread đang xử lý không tính: không thì thread chạy lại ngay sau lô, lúc
	 * cạnh mới nhất đã cũ, và lô đó không neo được (1000 Hz, 2026-10-30). */
	return kfifo_len(&st->edges) - READ_ONCE(st->inflight) >= st->batch ?
		IRQ_WAKE_THREAD : IRQ_HANDLED;
}

/* Đầy thì bỏ mẫu mới: bỏ mẫu cũ là thao tác của phía đọc, cần khóa. */
static void mpu6050_push(struct mpu6050 *st, const u8 *frame, u64 ts)
{
	const __be16 *raw = (const __be16 *)frame;
	struct mpu6050_sample s = {
		.ax = be16_to_cpu(raw[0]),
		.ay = be16_to_cpu(raw[1]),
		.az = be16_to_cpu(raw[2]),
		.temp = be16_to_cpu(raw[3]),
		.ts_ns = ts,
	};

	if (!kfifo_put(&st->fifo, s))
		atomic64_inc(&st->dropped);
}

/* FIFO_R_W đọc liên tiếp không tăng địa chỉ: một giao dịch lấy n mẫu. */
static int mpu6050_read_frames(struct mpu6050 *st, unsigned int n)
{
	u8 reg = MPU6050_REG_FIFO_R_W;
	struct i2c_msg msg[2] = {
		{ .addr = st->client->addr, .len = 1, .buf = &reg },
		{ .addr = st->client->addr, .flags = I2C_M_RD,
		  .len = n * MPU6050_FIFO_FRAME, .buf = st->fifo_buf },
	};
	int ret;

	ret = i2c_transfer(st->client->adapter, msg, ARRAY_SIZE(msg));
	return ret == ARRAY_SIZE(msg) ? 0 : ret < 0 ? ret : -EIO;
}

/*
 * Tắt đường ghi (FIFO_EN = 0) rồi mới reset, bật lại sau, như inv_mpu6050 mainline.
 * Reset lúc FIFO_EN còn bật thì có lúc rơi đúng lúc chip đang ghi dở một khung: FIFO
 * giữ lại một mẩu lẻ, mọi khung sau lệch byte (1000 Hz, 2026-10-30: count dư 1..6
 * byte ở mọi lần đọc, 75% bản ghi hỏng). Bật lại thì mẫu kế được ghi trọn từ đầu.
 */
static int mpu6050_fifo_reset(struct mpu6050 *st)
{
	struct i2c_client *c = st->client;
	int ret;

	ret = i2c_smbus_write_byte_data(c, MPU6050_REG_FIFO_EN, 0);
	if (!ret)
		ret = i2c_smbus_write_byte_data(c, MPU6050_REG_USER_CTRL,
						MPU6050_USER_CTRL_FIFO_RST);
	if (!ret)
		ret = i2c_smbus_write_byte_data(c, MPU6050_REG_USER_CTRL,
						MPU6050_USER_CTRL_FIFO_EN);
	if (!ret)
		ret = i2c_smbus_write_byte_data(c, MPU6050_REG_FIFO_EN,
						MPU6050_FIFO_EN_TEMP |
						MPU6050_FIFO_EN_ACCEL);
	if (ret < 0)
		dev_err_ratelimited(&c->dev, "FIFO reset failed: %d\n", ret);
	return ret;
}

/*
 * Mẫu mới nhất trong FIFO có phải của cạnh mới nhất trong snapshot không. Đúng khi
 * FIFO_COUNT được chốt trước nhịp lấy mẫu kế (now - newest < chu kỳ - lề), và cạnh
 * mới nhất không đến trễ bất thường (cách cạnh trước một chu kỳ ± lề; trễ thì nhịp
 * kế có thể đã vào FIFO dù now - newest còn nhỏ).
 */
static bool mpu6050_anchored(struct mpu6050 *st, unsigned int q, u64 now)
{
	u64 newest = st->edge_ts[q - 1];
	u64 prev = q >= 2 ? st->edge_ts[q - 2] : st->last_edge;
	u64 p = st->period_ns, m = MPU6050_ANCHOR_MARGIN_NS;

	if (now - newest + m >= p)
		return false;
	return !prev || (newest - prev + m >= p && newest - prev <= p + m);
}

/*
 * ts cho n khung của lô, cạnh used cuối trong edge_ts ghép với used khung mới nhất,
 * extra = n - used khung còn lại không có cạnh. Sau start/reset (last_edge 0) chúng là
 * mẫu vào FIFO trước khi ngắt mở: nằm trước cạnh đầu, lùi từng chu kỳ danh định.
 * Đang chạy thì cạnh của chúng bị dồn: irq/26 bị chặn hơn một chu kỳ (trên RT có lúc
 * vài ms, chờ khóa BH của một kworker), GPIO chỉ chốt một cạnh, và cạnh đó đến trễ.
 * Mẫu thiếu cạnh vì vậy nằm ở khoảng giữa hai cạnh dài bất thường, không nhất thiết ở
 * đầu lô: lần lượt đặt từng mẫu vào khoảng còn thừa thời gian nhiều nhất
 * (Δt - (số mẫu đã đặt + 1) · chu kỳ), rồi chia đều khoảng đó.
 */
static void mpu6050_frame_ts(struct mpu6050 *st, unsigned int used,
			     unsigned int extra)
{
	u64 p = st->period_ns, a, b;
	unsigned int i, j, k, best;
	s64 slack, most;

	if (!st->last_edge) {
		for (i = 0; i < extra; i++)
			st->frame_ts[i] = st->edge_ts[0] - (extra - i) * p;
		for (j = 0; j < used; j++)
			st->frame_ts[extra + j] = st->edge_ts[j];
		return;
	}

	memset(st->fill, 0, used);
	for (k = 0; k < extra; k++) {
		best = 0;
		most = S64_MIN;
		for (j = 0; j < used; j++) {
			a = j ? st->edge_ts[j - 1] : st->last_edge;
			slack = (s64)(st->edge_ts[j] - a) - (s64)(st->fill[j] + 1) * p;
			if (slack > most) {
				most = slack;
				best = j;
			}
		}
		st->fill[best]++;
	}

	for (i = 0, j = 0; j < used; j++) {
		a = j ? st->edge_ts[j - 1] : st->last_edge;
		b = st->edge_ts[j];
		for (k = 1; k <= st->fill[j]; k++)
			st->frame_ts[i++] = a + div_u64((b - a) * k, st->fill[j] + 1);
		st->frame_ts[i++] = b;
	}
}

/*
 * Nửa dưới (thread irq/N-mpu6050, SCHED_FIFO 50): một lô. Không lấy khóa nào của
 * driver: mpu6050_stop chờ hàm này trong lúc giữ cfg_lock.
 *
 * Chụp q ts trong edges trước, rồi đọc FIFO_COUNT (n mẫu). Neo được thì ghép từ mẫu
 * mới nhất: n < q nghĩa là q - n mẫu cũ nhất đã mất (FIFO bị ghi đè hay vừa reset),
 * cộng dropped; n > q nghĩa là có mẫu chưa có cạnh (vào FIFO trước khi ngắt mở, lúc
 * start), gán ts lùi từng chu kỳ, trừ khi cạnh của chúng đã bị tính mất (edges_lost).
 * Không neo được (thread hay irq/26 trễ) thì ghép từ cũ nhất, phần dư để lô sau neo
 * lại. FIFO đầy: reset, mọi cạnh đang chờ là mẫu mất.
 */
static irqreturn_t mpu6050_irq_thread(int irq, void *data)
{
	struct mpu6050 *st = data;
	unsigned int q, n, used, extra, i, lost;
	__be16 count;
	u64 t0, now, newest;
	int ret;

	/* Thí nghiệm tràn: chặn riêng thread này, nửa trên vẫn ghi cạnh. */
	i = xchg(&stall_ms, 0);
	if (i)
		msleep(i);

	t0 = ktime_get_ns();
	q = kfifo_out_peek(&st->edges, st->edge_ts, MPU6050_EDGE_LEN);
	lost = atomic_xchg(&st->edges_lost, 0);
	if (!q)
		return IRQ_HANDLED;
	WRITE_ONCE(st->inflight, q);
	newest = st->edge_ts[q - 1];
	/* Reset trước bị lỗi bus giữa chừng có thể để FIFO_EN = 0 (chip thôi ghi FIFO):
	 * làm lại, lô này không có dữ liệu đáng tin. */
	if (st->reset_pending)
		goto reset;

	/* Chờ bận, không ngủ: usleep_range trên RT debug dậy trễ hơn nhiều. Tối đa
	 * MPU6050_SETTLE_NS, chỉ khi thread dậy sớm. */
	now = ktime_get_ns();
	if (now < newest + MPU6050_SETTLE_NS) {
		st->n_settle++;
		ndelay(newest + MPU6050_SETTLE_NS - now);
	}
	ret = i2c_smbus_read_i2c_block_data(st->client, MPU6050_REG_FIFO_COUNT_H,
					    sizeof(count), (u8 *)&count);
	now = ktime_get_ns();
	if (ret != sizeof(count)) {
		/* Cạnh còn nguyên trong edges, lô sau đọc lại. */
		dev_err_ratelimited(&st->client->dev, "FIFO_COUNT read failed: %d\n",
				    ret < 0 ? ret : -EIO);
		goto out;
	}
	/*
	 * Count không chia hết cho khung: FIFO đang giữ một mẩu lẻ, khung nào đọc ra cũng
	 * lệch byte. Không gỡ được, reset và tính cả lô là mất.
	 */
	if (be16_to_cpu(count) % MPU6050_FIFO_FRAME) {
		st->n_misaligned++;
		dev_warn_ratelimited(&st->client->dev, "FIFO count %u not a multiple of %u, reset\n",
				     be16_to_cpu(count) & 0x1fff, MPU6050_FIFO_FRAME);
		goto reset;
	}

	/* Chặn theo sức chứa: count rác hay oflow_reset = 0 không được tràn fifo_buf. */
	n = min((be16_to_cpu(count) & 0x1fff) / MPU6050_FIFO_FRAME,
		st->chip->fifo_size / MPU6050_FIFO_FRAME);

	if (n * MPU6050_FIFO_FRAME >= st->chip->fifo_size && READ_ONCE(oflow_reset))
		goto reset;

	extra = 0;
	if (mpu6050_anchored(st, q, now)) {
		st->n_anchored++;
		if (n < q) {
			atomic64_add(q - n, &st->dropped);
			kfifo_skip_count(&st->edges, q - n);
			memmove(st->edge_ts, st->edge_ts + q - n, n * sizeof(u64));
			q = n;
		}
		extra = n - q;
		st->n_extra += extra;
	} else {
		/*
		 * Khung chưa có cạnh trong snapshot mà vẫn kịp vào count có mẫu trong
		 * (t0 - lề, now). Cửa sổ đó ngắn hơn một chu kỳ thì có nhiều nhất một khung
		 * như vậy, và nó là khung mới nhất: dư từ hai trở lên thì n - q - 1 khung cũ
		 * nhất chắc chắn không có cạnh (cạnh dồn), xử lý luôn thay vì để FIFO tồn đọng.
		 */
		st->n_unanchored++;
		if (n >= q + 2 && now - t0 + MPU6050_ANCHOR_MARGIN_NS < st->period_ns) {
			extra = n - q - 1;
			st->n_extra += extra;
			n = q + extra;
		} else {
			n = min(n, q);
		}
	}
	if (!n)
		goto out;

	ret = mpu6050_read_frames(st, n);
	if (ret) {
		/* FIFO đã bị đọc dở, không biết còn khớp với cạnh không: bỏ cả lô. */
		dev_err_ratelimited(&st->client->dev, "FIFO read of %u failed: %d\n",
				    n, ret);
		goto reset;
	}

	/*
	 * edges_lost: có mẫu đã tính vào dropped ở nửa trên vì hàng đợi cạnh đầy; không
	 * biết khung nào là của chúng, nên chỉ trả khung có cạnh (lúc đó FIFO thường đã
	 * tràn và bị reset ở trên, nhánh này chỉ gặp khi oflow_reset = 0).
	 */
	used = n - extra;
	if (lost && extra) {
		for (i = 0; i < used; i++)
			mpu6050_push(st, st->fifo_buf + (extra + i) * MPU6050_FIFO_FRAME,
				     st->edge_ts[i]);
	} else {
		mpu6050_frame_ts(st, used, extra);
		for (i = 0; i < n; i++)
			mpu6050_push(st, st->fifo_buf + i * MPU6050_FIFO_FRAME,
				     st->frame_ts[i]);
	}
	kfifo_skip_count(&st->edges, used);
	if (used)
		st->last_edge = st->edge_ts[used - 1];
	wake_up_interruptible(&st->wq);
	goto out;

reset:
	/* Đầy, lệch hay đọc lỗi: mọi cạnh đang chờ là mẫu mất. */
	st->n_reset++;
	st->reset_pending = mpu6050_fifo_reset(st) < 0;
	atomic64_add(q, &st->dropped);
	kfifo_skip_count(&st->edges, q);
	st->last_edge = 0;
out:
	WRITE_ONCE(st->inflight, 0);
	return IRQ_HANDLED;
}

/*
 * Gọi trong cfg_lock. FIFO chip reset + bật trước, rồi mới bật DATA_RDY và mở đường
 * GPIO: mẫu vào FIFO trước khi cạnh được đếm thành mẫu dư (có ts lùi), không thành
 * cạnh thiếu mẫu (bị tính mất). Lúc ngắt còn mask, gpio-omap tắt dò cạnh (trigger
 * NONE) nên enable_irq không phát lại cạnh cũ. Mẫu đầu đến sau tối đa một lô, nên
 * read ngay sau open là -EAGAIN.
 */
/*
 * Số mẫu mỗi lô: khoảng 10 ms, nhưng không quá số khung đọc kịp trước mẫu kế. Chip
 * ghi một mẫu vào FIFO giữa lúc burst đang đọc FIFO thì khoảng 1% số lần con trỏ FIFO
 * trượt vài byte, mọi khung sau lệch (LA 2026-10-30, 400 kHz: 1000 Hz lô 3..10 và
 * 500 Hz lô 10 lệch ~1% số lô; 1000 Hz lô 1, 2 và 500 Hz lô 5 sạch). Một khung trên
 * dây là 8 byte × 9 bit. 400 kHz: 1000 Hz → 2, 500 Hz → 5; 100 kHz: 1000 Hz → 1.
 */
static unsigned int mpu6050_batch(struct mpu6050 *st, unsigned int hz)
{
	u64 frame_ns = div_u64(8ULL * 9 * NSEC_PER_SEC, st->bus_hz);
	s64 room = (s64)st->period_ns - MPU6050_READ_START_NS - MPU6050_WRITE_GUARD_NS;
	unsigned int fit = room > 0 ? div64_u64(room, frame_ns) : 0;

	return max(1u, min(DIV_ROUND_UP(hz, MPU6050_BATCH_HZ), fit));
}

static int mpu6050_start(struct mpu6050 *st)
{
	unsigned int hz = mpu6050_rate_hz(st);
	int ret;

	st->period_ns = (u64)(st->rate_div + 1) * NSEC_PER_MSEC;
	st->batch = READ_ONCE(batch) ?: mpu6050_batch(st, hz);
	st->batch = clamp_t(unsigned int, st->batch, 1, MPU6050_EDGE_LEN / 2);
	st->last_edge = 0;
	st->inflight = 0;
	st->reset_pending = false;
	kfifo_reset(&st->edges);
	atomic_set(&st->edges_lost, 0);

	ret = mpu6050_fifo_reset(st);
	if (ret < 0)
		return ret;
	ret = mpu6050_write_check(st->client, MPU6050_REG_FIFO_EN,
				  MPU6050_FIFO_EN_TEMP | MPU6050_FIFO_EN_ACCEL);
	if (ret)
		return ret;
	ret = mpu6050_write_check(st->client, MPU6050_REG_INT_ENABLE,
				  MPU6050_INT_ENABLE_RAW_RDY);
	if (ret)
		return ret;
	st->streaming = true;
	enable_irq(st->irq);
	return 0;
}

/*
 * Gọi trong cfg_lock, chỉ khi streaming. disable_irq mask ngay ở GPIO
 * (IRQ_DISABLE_UNLAZY) và chờ cả nửa trên lẫn thread chạy xong: trả về là không còn
 * producer, kể cả khi bus I2C hỏng. Tắt DATA_RDY và FIFO sau, chỉ để chip thôi phát
 * xung và thôi ghi FIFO.
 */
static void mpu6050_stop(struct mpu6050 *st)
{
	int ret;

	st->streaming = false;
	disable_irq(st->irq);
	ret = i2c_smbus_write_byte_data(st->client, MPU6050_REG_INT_ENABLE, 0);
	if (ret >= 0)
		ret = i2c_smbus_write_byte_data(st->client, MPU6050_REG_USER_CTRL, 0);
	if (ret >= 0)
		ret = i2c_smbus_write_byte_data(st->client, MPU6050_REG_FIFO_EN, 0);
	if (ret < 0)
		dev_warn(&st->client->dev, "cannot stop DATA_RDY/FIFO: %d\n", ret);
}

static int mpu6050_open(struct inode *inode, struct file *filp)
{
	/* misc_open đã đặt private_data = &st->misc trước khi gọi hàm này. */
	struct mpu6050 *st = container_of(filp->private_data, struct mpu6050, misc);
	int ret;

	if (test_and_set_bit_lock(MPU6050_FLAG_OPEN, &st->flags))
		return -EBUSY;
	filp->private_data = st;

	/*
	 * Bật nguồn trong cfg_lock để không chen với đổi cấu hình (bước 3).
	 * kfifo_reset đụng cả in lẫn out nên chỉ được gọi khi producer đã dừng và không
	 * ai đang đọc. Ở đây cả hai đúng: một người mở thì fd trước đã release xong,
	 * nguồn chỉ bật ngay sau dòng này. Người mở chỉ thấy mẫu mới.
	 */
	mutex_lock(&st->cfg_lock);
	kfifo_reset(&st->fifo);
	ret = mpu6050_start(st);
	mutex_unlock(&st->cfg_lock);
	if (ret) {
		clear_bit_unlock(MPU6050_FLAG_OPEN, &st->flags);
		return ret;
	}

	/* Không có vị trí đọc: lseek trả -ESPIPE, *ppos bị bỏ qua. */
	return stream_open(inode, filp);
}

static int mpu6050_release(struct inode *inode, struct file *filp)
{
	struct mpu6050 *st = filp->private_data;

	/*
	 * Tắt nguồn trước khi nhả cờ: người mở sau không gặp producer cũ. streaming có
	 * thể đã false nếu đổi cấu hình không bật lại được nguồn; disable_irq hai lần
	 * thì độ sâu lệch, enable_irq sau đó không mở được ngắt.
	 */
	mutex_lock(&st->cfg_lock);
	if (st->streaming)
		mpu6050_stop(st);
	mutex_unlock(&st->cfg_lock);

	clear_bit_unlock(MPU6050_FLAG_OPEN, &st->flags);
	return 0;
}

/*
 * Chỉ trả trọn bản ghi: buffer nhỏ hơn một bản ghi là -EINVAL, phần lẻ cuối buffer bị
 * bỏ. Rỗng thì -EAGAIN (O_NONBLOCK) hoặc ngủ tới khi có dữ liệu hay có signal.
 */
static ssize_t mpu6050_read(struct file *filp, char __user *buf, size_t count,
			    loff_t *ppos)
{
	struct mpu6050 *st = filp->private_data;
	unsigned int copied;
	int ret;

	if (count < sizeof(struct mpu6050_sample))
		return -EINVAL;
	/* kfifo_to_user nhận unsigned int: không cắt thì count > 4 GB trên 64-bit có
	 * thể thành < 16 byte, copy được 0 và vòng dưới quay mãi. */
	count = min_t(size_t, count, sizeof(struct mpu6050_sample) * MPU6050_FIFO_LEN);

	for (;;) {
		if (kfifo_is_empty(&st->fifo)) {
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;
			/*
			 * Ngủ mà không giữ read_lock: đổi cấu hình dừng producer rồi
			 * lấy read_lock để reset fifo; giữ khóa ở đây là chờ nhau mãi.
			 */
			ret = wait_event_interruptible(st->wq,
						       !kfifo_is_empty(&st->fifo));
			if (ret)
				return ret;
		}

		ret = mutex_lock_interruptible(&st->read_lock);
		if (ret)
			return ret;
		ret = kfifo_to_user(&st->fifo, buf, count, &copied);
		mutex_unlock(&st->read_lock);
		if (ret)
			return ret;
		if (copied)
			return copied;
		/* Luồng khác cùng fd đọc mất, hoặc fifo vừa bị xóa: chờ lại. */
	}
}

static __poll_t mpu6050_poll(struct file *filp, poll_table *wait)
{
	struct mpu6050 *st = filp->private_data;

	poll_wait(filp, &st->wq, wait);
	return kfifo_is_empty(&st->fifo) ? 0 : EPOLLIN | EPOLLRDNORM;
}

static long mpu6050_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct mpu6050 *st = filp->private_data;
	void __user *uarg = (void __user *)arg;
	int ret;

	switch (cmd) {
	case MPU6050_IOC_GET_CONFIG: {
		/* {} xóa cả phần đệm nếu struct có sau này: không lộ byte của kernel. */
		struct mpu6050_config cfg = {};

		mutex_lock(&st->cfg_lock);
		cfg.sample_rate_hz = mpu6050_rate_hz(st);
		cfg.accel_range_g = mpu6050_range_g(st);
		cfg.accel_lsb_per_g = 16384 >> st->accel_fs;
		/* Bảng accel của họ 6500; 6050 lọc accel theo CONFIG với dãy hơi khác. */
		cfg.accel_bw_hz = mpu6050_dlpf_bw_hz[mpu6050_dlpf_index(st->rate_div)];
		mutex_unlock(&st->cfg_lock);
		cfg.whoami = st->chip->whoami;
		cfg.temp_sens_x100 = st->chip->temp_sens_x100;
		cfg.temp_offset_mc = st->chip->temp_offset_mc;
		return copy_to_user(uarg, &cfg, sizeof(cfg)) ? -EFAULT : 0;
	}
	case MPU6050_IOC_GET_STATS: {
		struct mpu6050_stats stats = {
			.dropped = atomic64_read(&st->dropped),
		};

		return copy_to_user(uarg, &stats, sizeof(stats)) ? -EFAULT : 0;
	}
	case MPU6050_IOC_FLUSH:
		/* Thao tác của phía đọc: kfifo_reset_out chỉ dời out, producer cứ chạy. */
		ret = mutex_lock_interruptible(&st->read_lock);
		if (ret)
			return ret;
		kfifo_reset_out(&st->fifo);
		mutex_unlock(&st->read_lock);
		return 0;
	default:
		return -ENOTTY;
	}
}

/*
 * Không có .write: write() trên fd chỉ đọc bị VFS chặn trước (-EBADF), fd O_RDWR
 * thì -EINVAL. Không có .llseek: stream_open cho -ESPIPE.
 */
static const struct file_operations mpu6050_fops = {
	.owner = THIS_MODULE,		/* misc_open lấy refcount module: chặn rmmod */
	.open = mpu6050_open,
	.release = mpu6050_release,
	.read = mpu6050_read,
	.poll = mpu6050_poll,
	.unlocked_ioctl = mpu6050_ioctl,
	.compat_ioctl = compat_ptr_ioctl,	/* kernel 64-bit chạy app 32-bit */
};

/* Thuộc tính nằm trên misc device: drvdata của nó là &st->misc (misc_register). */
static struct mpu6050 *mpu6050_from_dev(struct device *dev)
{
	struct miscdevice *misc = dev_get_drvdata(dev);

	return container_of(misc, struct mpu6050, misc);
}

/*
 * Đổi cấu hình, gọi trong cfg_lock. Không ai mở (streaming false) thì chỉ ghi thanh
 * ghi: open sẽ reset fifo và bật nguồn. Đang có người mở thì dừng nguồn, ghi, xóa
 * fifo, chạy lại, để mọi bản ghi đọc được sau khi store trả về đều theo cấu hình
 * mới. kfifo_reset chỉ khi producer đã dừng, và dưới read_lock vì có thể có luồng
 * đang trong read() (thứ tự cfg_lock rồi read_lock). Lỗi I2C giữa chừng: cố ghi lại
 * cấu hình cũ, trả lỗi cho userspace. Không bật lại được nguồn thì streaming ở
 * false, người đọc chờ tới khi mở lại (đường lỗi làm ở tuần 8).
 */
static int mpu6050_reconfig(struct mpu6050 *st, unsigned int rate_div,
			    unsigned int accel_fs)
{
	bool streaming = st->streaming;
	int ret, err;

	if (streaming)
		mpu6050_stop(st);

	ret = mpu6050_write_rate(st, rate_div);
	if (!ret)
		ret = mpu6050_write_range(st, accel_fs);
	if (ret) {
		mpu6050_write_rate(st, st->rate_div);
		mpu6050_write_range(st, st->accel_fs);
	} else {
		st->rate_div = rate_div;
		st->accel_fs = accel_fs;
	}

	if (streaming) {
		mutex_lock(&st->read_lock);
		kfifo_reset(&st->fifo);
		mutex_unlock(&st->read_lock);
		err = mpu6050_start(st);
		if (!ret)
			ret = err;
	}
	return ret;
}

/*
 * Hz. Ghi 4..1000; SMPLRT_DIV = 1000/hz - 1 (chia nguyên) nên tốc độ thật luôn ≥ giá
 * trị ghi; đọc lại trả tốc độ thật làm tròn xuống (ghi 300 đọc 333).
 */
static ssize_t sample_rate_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct mpu6050 *st = mpu6050_from_dev(dev);
	unsigned int hz;

	mutex_lock(&st->cfg_lock);
	hz = mpu6050_rate_hz(st);
	mutex_unlock(&st->cfg_lock);
	return sysfs_emit(buf, "%u\n", hz);
}

static ssize_t sample_rate_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct mpu6050 *st = mpu6050_from_dev(dev);
	unsigned int hz;
	int ret;

	ret = kstrtouint(buf, 10, &hz);
	if (ret)
		return ret;
	if (hz < MPU6050_RATE_MIN_HZ || hz > MPU6050_RATE_MAX_HZ)
		return -EINVAL;

	mutex_lock(&st->cfg_lock);
	ret = mpu6050_reconfig(st, MPU6050_INTERNAL_RATE_HZ / hz - 1, st->accel_fs);
	mutex_unlock(&st->cfg_lock);
	return ret ?: count;
}
static DEVICE_ATTR_RW(sample_rate);

/* g. Chỉ nhận 2, 4, 8, 16; độ nhạy 16384 >> ACCEL_FS_SEL LSB/g. */
static ssize_t accel_range_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct mpu6050 *st = mpu6050_from_dev(dev);
	unsigned int g;

	mutex_lock(&st->cfg_lock);
	g = mpu6050_range_g(st);
	mutex_unlock(&st->cfg_lock);
	return sysfs_emit(buf, "%u\n", g);
}

static ssize_t accel_range_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct mpu6050 *st = mpu6050_from_dev(dev);
	unsigned int g, fs;
	int ret;

	ret = kstrtouint(buf, 10, &g);
	if (ret)
		return ret;
	switch (g) {
	case 2:
		fs = 0;
		break;
	case 4:
		fs = 1;
		break;
	case 8:
		fs = 2;
		break;
	case 16:
		fs = 3;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&st->cfg_lock);
	ret = mpu6050_reconfig(st, st->rate_div, fs);
	mutex_unlock(&st->cfg_lock);
	return ret ?: count;
}
static DEVICE_ATTR_RW(accel_range);

/* Mẫu mất từ lúc probe, không xóa khi mở lại: userspace lấy hiệu hai lần đọc. */
static ssize_t dropped_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mpu6050 *st = mpu6050_from_dev(dev);

	return sysfs_emit(buf, "%llu\n", (u64)atomic64_read(&st->dropped));
}
static DEVICE_ATTR_RO(dropped);

/* Tên thuộc tính chốt từ GĐ1 (spec quy tắc 2), không đổi ở GĐ2. */
static struct attribute *mpu6050_attrs[] = {
	&dev_attr_sample_rate.attr,
	&dev_attr_accel_range.attr,
	&dev_attr_dropped.attr,
	NULL
};
ATTRIBUTE_GROUPS(mpu6050);

static void mpu6050_debugfs_remove(void *data)
{
	debugfs_remove_recursive(data);
}

/* Bộ đếm chẩn đoán của thread, không phải giao diện: /sys/kernel/debug/mpu6050/. */
static int mpu6050_debugfs_init(struct mpu6050 *st)
{
	struct dentry *d = debugfs_create_dir("mpu6050", NULL);

	debugfs_create_u32("anchored", 0444, d, &st->n_anchored);
	debugfs_create_u32("unanchored", 0444, d, &st->n_unanchored);
	debugfs_create_u32("extra", 0444, d, &st->n_extra);
	debugfs_create_u32("fifo_reset", 0444, d, &st->n_reset);
	debugfs_create_u32("misaligned", 0444, d, &st->n_misaligned);
	debugfs_create_u32("batch", 0444, d, &st->batch);
	debugfs_create_u32("settle", 0444, d, &st->n_settle);
	return devm_add_action_or_reset(&st->client->dev, mpu6050_debugfs_remove, d);
}

/* Cờ nằm trên irq_desc, sống lâu hơn module: trả lại khi gỡ. */
static void mpu6050_clear_unlazy(void *data)
{
	irq_clear_status_flags((long)data, IRQ_DISABLE_UNLAZY);
}

static int mpu6050_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	const struct mpu6050_chip *chip;
	struct mpu6050 *st;
	int ret;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return dev_err_probe(dev, -EOPNOTSUPP,
				     "adapter lacks byte/i2c-block SMBus\n");

	ret = i2c_smbus_read_byte_data(client, MPU6050_REG_WHO_AM_I);
	if (ret < 0)
		return dev_err_probe(dev, ret, "cannot read WHO_AM_I\n");

	chip = mpu6050_find_chip(ret);
	if (!chip)
		return dev_err_probe(dev, -ENODEV, "unknown WHO_AM_I 0x%02x\n", ret);

	/* Kiểu trigger lấy từ overlay, driver không ép; INT_PIN_CFG 0x00 (xung
	 * active-high) chỉ khớp cạnh lên. */
	if (client->irq <= 0)
		return dev_err_probe(dev, -EINVAL, "no interrupt in device tree\n");
	if (irq_get_trigger_type(client->irq) != IRQ_TYPE_EDGE_RISING)
		return dev_err_probe(dev, -EINVAL, "irq %d is not IRQ_TYPE_EDGE_RISING\n",
				     client->irq);

	/* Cấp phát trước khi đụng vào chip: hết bộ nhớ thì chip vẫn y nguyên. */
	st = devm_kzalloc(dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;
	st->client = client;
	st->chip = chip;
	ret = devm_mutex_init(dev, &st->cfg_lock);
	if (ret)
		return ret;
	ret = devm_mutex_init(dev, &st->read_lock);
	if (ret)
		return ret;
	st->rate_div = MPU6050_DEFAULT_RATE_DIV;
	st->accel_fs = MPU6050_DEFAULT_ACCEL_FS;
	INIT_KFIFO(st->fifo);
	INIT_KFIFO(st->edges);
	init_waitqueue_head(&st->wq);
	st->irq = client->irq;
	i2c_set_clientdata(client, st);

	/* Tốc độ bus quyết định cỡ lô; không khai thì i2c core coi là 100 kHz. */
	if (device_property_read_u32(client->adapter->dev.parent, "clock-frequency",
				     &st->bus_hz) || !st->bus_hz)
		st->bus_hz = I2C_MAX_STANDARD_MODE_FREQ;

	ret = mpu6050_reset(client, chip);
	if (ret)
		return dev_err_probe(dev, ret, "reset failed\n");

	ret = mpu6050_configure(st);
	if (ret)
		return dev_err_probe(dev, ret, "configure failed\n");

	/*
	 * Xin ngắt sau khi INT_ENABLE đã về 0, để tắt (NO_AUTOEN); mpu6050_start mở.
	 * ONESHOT: trên RT nửa trên không bị ép thành thread riêng, ts lấy sớm hơn một
	 * lần đánh thức. UNLAZY: disable_irq mask ngay ở GPIO, cạnh tới lúc tắt không bị
	 * ghi nhớ rồi phát lại khi bật. Thread mặc định SCHED_FIFO 50.
	 */
	irq_set_status_flags(st->irq, IRQ_DISABLE_UNLAZY);
	ret = devm_add_action_or_reset(dev, mpu6050_clear_unlazy,
				       (void *)(long)st->irq);
	if (ret)
		return ret;
	ret = devm_request_threaded_irq(dev, st->irq, mpu6050_irq, mpu6050_irq_thread,
					IRQF_ONESHOT | IRQF_NO_AUTOEN, "mpu6050", st);
	if (ret)
		return dev_err_probe(dev, ret, "cannot request irq %d\n", st->irq);

	ret = mpu6050_debugfs_init(st);
	if (ret)
		return ret;

	/*
	 * Đăng ký sau cùng: userspace chỉ thấy /dev/mpu6050 khi mọi thứ đã sẵn. parent
	 * để /sys/class/misc/mpu6050/device trỏ về 2-0068. Tên cố định nên chỉ một chip.
	 * groups: thuộc tính tạo cùng lúc với thiết bị, trước uevent, udev không đua.
	 */
	st->misc.minor = MISC_DYNAMIC_MINOR;
	st->misc.name = "mpu6050";
	st->misc.fops = &mpu6050_fops;
	st->misc.parent = dev;
	st->misc.groups = mpu6050_groups;
	ret = misc_register(&st->misc);
	if (ret)
		return dev_err_probe(dev, ret, "misc_register failed\n");

	dev_info(dev, "%s (WHO_AM_I 0x%02x), irq %d, /dev/%s minor %d, %u Hz, +-%u g, bus %u Hz\n",
		 chip->name, chip->whoami, client->irq, st->misc.name, st->misc.minor,
		 mpu6050_rate_hz(st), mpu6050_range_g(st), st->bus_hz);
	return 0;
}

static void mpu6050_remove(struct i2c_client *client)
{
	struct mpu6050 *st = i2c_get_clientdata(client);
	int ret;

	/*
	 * Gỡ /dev và thuộc tính trước: từ đây không ai mở mới, không còn store nào chạy.
	 * Bình thường nguồn đã tắt ở release (rmmod bị chặn khi còn fd); chỉ unbind khi
	 * fd còn mở mới tới đây với nguồn đang chạy, dừng nó trước khi chip ngủ. Ngắt
	 * do devm trả sau hàm này.
	 */
	misc_deregister(&st->misc);
	mutex_lock(&st->cfg_lock);
	if (st->streaming)
		mpu6050_stop(st);
	mutex_unlock(&st->cfg_lock);

	/* Ngủ: lần probe sau vẫn reset nên không phụ thuộc trạng thái này. */
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1,
					MPU6050_PWR_MGMT_1_SLEEP |
					MPU6050_PWR_MGMT_1_CLKSEL_PLL);
	if (ret < 0)
		dev_warn(&client->dev, "cannot put chip to sleep: %d\n", ret);
	else
		dev_info(&client->dev, "removed, chip asleep\n");
}

/*
 * compatible riêng của project (overlay mpu6050.dts), không phải "invensense,mpu6050",
 * để driver mainline inv_mpu6050 không bao giờ nhận thiết bị này.
 * MODULE_DEVICE_TABLE sinh alias of:N*T*Cprojectb,mpu6050 cho udev tự modprobe.
 */
static const struct of_device_id mpu6050_of_match[] = {
	{ .compatible = "projectb,mpu6050" },
	{ }
};
MODULE_DEVICE_TABLE(of, mpu6050_of_match);

static struct i2c_driver mpu6050_driver = {
	.driver = {
		.name = "mpu6050",
		.of_match_table = mpu6050_of_match,
	},
	.probe = mpu6050_probe,
	.remove = mpu6050_remove,
};
module_i2c_driver(mpu6050_driver);

MODULE_DESCRIPTION("MPU6050/MPU-6500 accelerometer driver (project B)");
MODULE_AUTHOR("nmatu0810");
MODULE_LICENSE("GPL");
