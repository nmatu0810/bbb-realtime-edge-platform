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
 * IRQ đọc burst từng mẫu (thay hrtimer + work của tuần 5). Chưa FIFO của chip
 * (tuần 7).
 */
#include <linux/atomic.h>
#include <linux/bitops.h>
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
#define MPU6050_REG_WHO_AM_I		0x75

#define MPU6050_PWR_MGMT_1_DEVICE_RESET	BIT(7)
#define MPU6050_PWR_MGMT_1_SLEEP	BIT(6)
#define MPU6050_PWR_MGMT_1_CLKSEL_PLL	0x01	/* PLL khi sẵn sàng, không thì dao động trong */
#define MPU6050_SIGNAL_PATH_RESET_ALL	0x07	/* GYRO_RST | ACCEL_RST | TEMP_RST */
#define MPU6050_INT_ENABLE_RAW_RDY	BIT(0)

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
	unsigned int fifo_size;		/* byte; dùng ở tuần 7 */
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
	 * Nguồn dữ liệu: ngắt DATA_RDY. irq_ts là ts_ns nửa trên lấy cho mẫu chưa đọc,
	 * 0 khi thread đã lấy (ktime_get_ns không bao giờ ra 0 sau boot).
	 */
	int irq;
	atomic64_t irq_ts;

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

/*
 * Nửa trên: chỉ lấy thời điểm. Trên RT hàm này chạy trong thread irq/26 của
 * gpio-omap (IRQF_ONESHOT giữ nó khỏi bị ép thành thread riêng, manage.c), nên ts
 * đã gồm độ trễ đánh thức thread đó.
 *
 * gpio-omap dùng handle_simple_irq cho ngắt cạnh, không mask: cạnh tới lúc thread
 * còn chạy vẫn vào đây. irq_ts khác 0 nghĩa là thread chưa lấy mẫu trước, chip đã
 * ghi đè thanh ghi dữ liệu: mất một mẫu, tính vào dropped. Các lần đánh thức dồn
 * lại chỉ cho thread chạy thêm một lần (__irq_wake_thread), đọc mẫu mới nhất với ts
 * mới nhất.
 */
static irqreturn_t mpu6050_irq(int irq, void *data)
{
	struct mpu6050 *st = data;

	if (atomic64_xchg(&st->irq_ts, ktime_get_ns()))
		atomic64_inc(&st->dropped);
	return IRQ_WAKE_THREAD;
}

/*
 * Nửa dưới (thread irq/N-mpu6050, SCHED_FIFO 50): đọc burst 8 byte rồi đẩy vào
 * fifo. Lấy ts trước khi đọc: nếu thread kẹt hơn một chu kỳ giữa hai việc này, burst
 * đọc mẫu sau với ts của mẫu trước và lần chạy kế đọc lại đúng mẫu đó (bản ghi
 * trùng). Ở 100 Hz phải kẹt ~9 ms; FIFO của chip (tuần 7) bỏ được giới hạn này.
 */
static irqreturn_t mpu6050_irq_thread(int irq, void *data)
{
	struct mpu6050 *st = data;
	__be16 raw[MPU6050_SAMPLE_REGS];
	struct mpu6050_sample s;
	int ret;

	s.ts_ns = atomic64_xchg(&st->irq_ts, 0);
	if (!s.ts_ns)
		return IRQ_HANDLED;

	ret = i2c_smbus_read_i2c_block_data(st->client, MPU6050_REG_ACCEL_XOUT_H,
					    sizeof(raw), (u8 *)raw);
	if (ret == sizeof(raw)) {
		s.ax = be16_to_cpu(raw[0]);
		s.ay = be16_to_cpu(raw[1]);
		s.az = be16_to_cpu(raw[2]);
		s.temp = be16_to_cpu(raw[3]);
		/* Đầy thì bỏ mẫu mới: bỏ mẫu cũ là thao tác của phía đọc, cần khóa. */
		if (kfifo_put(&st->fifo, s))
			wake_up_interruptible(&st->wq);
		else
			atomic64_inc(&st->dropped);
	} else {
		/* Mẫu của ngắt này không đọc được: cũng là mẫu mất. Đọc từng mẫu ở
		 * 1000 Hz, chip thỉnh thoảng NACK và omap_i2c treo 1 s sau đó
		 * (2026-10-30); 500 Hz chưa gặp. */
		atomic64_inc(&st->dropped);
		dev_err_ratelimited(&st->client->dev, "burst read failed: %d\n",
				    ret < 0 ? ret : -EIO);
	}
	return IRQ_HANDLED;
}

/*
 * Gọi trong cfg_lock. Bật chip trước rồi mới mở đường GPIO: ghi lỗi thì chưa có gì
 * phải đóng lại. Lúc ngắt còn mask, gpio-omap tắt dò cạnh (trigger NONE) nên
 * enable_irq không phát lại cạnh cũ. Mẫu đầu đến sau tối đa một chu kỳ, nên read
 * ngay sau open là -EAGAIN.
 */
static int mpu6050_start(struct mpu6050 *st)
{
	int ret;

	atomic64_set(&st->irq_ts, 0);
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
 * producer, kể cả khi bus I2C hỏng. Tắt INT_ENABLE sau, chỉ để chip thôi phát xung.
 */
static void mpu6050_stop(struct mpu6050 *st)
{
	int ret;

	st->streaming = false;
	disable_irq(st->irq);
	ret = i2c_smbus_write_byte_data(st->client, MPU6050_REG_INT_ENABLE, 0);
	if (ret < 0)
		dev_warn(&st->client->dev, "cannot disable DATA_RDY: %d\n", ret);
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
	init_waitqueue_head(&st->wq);
	st->irq = client->irq;
	i2c_set_clientdata(client, st);

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

	dev_info(dev, "%s (WHO_AM_I 0x%02x), irq %d, /dev/%s minor %d, %u Hz, +-%u g\n",
		 chip->name, chip->whoami, client->irq, st->misc.name, st->misc.minor,
		 mpu6050_rate_hz(st), mpu6050_range_g(st));
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
