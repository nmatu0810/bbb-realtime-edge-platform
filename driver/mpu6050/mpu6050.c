// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver MPU6050 / MPU-6500 trên I2C cho project B (BeagleBone Black).
 *
 * Chip thật trên module là họ MPU-6500 (WHO_AM_I = 0x70), tên mpu6050 giữ cho cả
 * project. Thanh ghi theo MPU-6500 Register Map rev 2.1 (RM-MPU-6500A-00); thông
 * số MPU-6050 lấy theo inv_mpu6050 mainline, chưa kiểm trên chip thật.
 *
 * Tuần 4, bước 2: probe reset chip, ghi tường minh mọi thanh ghi cấu hình rồi đọc
 * lại để kiểm, đọc burst một mẫu để kiểm dữ liệu; remove cho chip ngủ. Chưa có
 * char device (tuần 5), chưa ngắt (tuần 6), chưa FIFO (tuần 7).
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/math.h>
#include <linux/math64.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

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

/* Chờ sau mỗi bước reset, như inv_mpu6050 (INV_MPU6050_POWER_UP_TIME). */
#define MPU6050_RESET_DELAY_MS		100

/* Burst ACCEL_XOUT_H..GYRO_ZOUT_L: ax ay az temp gx gy gz, big-endian s16. */
#define MPU6050_BURST_LEN		14

/* ±2 g (ACCEL_CONFIG = 0) */
#define MPU6050_ACCEL_LSB_PER_G		16384

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

/* Ghi mọi thanh ghi cấu hình, kể cả những cái trùng giá trị reset. */
static int mpu6050_configure(struct i2c_client *client,
			     const struct mpu6050_chip *chip)
{
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

	/* FIFO_MODE 0, DLPF_CFG 3 (41 Hz, Fs 1 kHz). DLPF_CFG phải trong 1..6 thì
	 * SMPLRT_DIV mới có tác dụng; giá trị reset 0 làm SMPLRT_DIV vô hiệu. */
	ret = mpu6050_write_check(client, MPU6050_REG_CONFIG, 0x03);
	if (ret)
		return ret;
	/* 1 kHz / (1 + 9) = 100 Hz */
	ret = mpu6050_write_check(client, MPU6050_REG_SMPLRT_DIV, 9);
	if (ret)
		return ret;
	/* ±250 °/s, FCHOICE_B 00 (đi qua DLPF) */
	ret = mpu6050_write_check(client, MPU6050_REG_GYRO_CONFIG, 0x00);
	if (ret)
		return ret;
	/* ±2 g */
	ret = mpu6050_write_check(client, MPU6050_REG_ACCEL_CONFIG, 0x00);
	if (ret)
		return ret;
	/* ACCEL_FCHOICE_B 0, A_DLPF_CFG 3 (41 Hz): dưới Nyquist 50 Hz của 100 Hz */
	if (chip->has_accel_config2) {
		ret = mpu6050_write_check(client, MPU6050_REG_ACCEL_CONFIG2, 0x03);
		if (ret)
			return ret;
	}

	/* INT active-high, push-pull, xung 50 µs, xóa khi đọc INT_STATUS: khớp
	 * IRQ_TYPE_EDGE_RISING trong overlay. Ngắt bật ở tuần 6, FIFO ở tuần 7. */
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

/*
 * Đọc 14 byte trong một giao dịch (S, addr+W, 0x3B, Sr, addr+R, 14 byte, P) để mọi
 * trục thuộc cùng một lần lấy mẫu. Tạm thời: in một mẫu để kiểm, bỏ khi có char
 * device ở tuần 5.
 */
static int mpu6050_dump_sample(struct i2c_client *client,
			       const struct mpu6050_chip *chip)
{
	__be16 raw[MPU6050_BURST_LEN / 2];
	s16 ax, ay, az, temp, gx, gy, gz;
	u32 mag;
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, MPU6050_REG_ACCEL_XOUT_H,
					    MPU6050_BURST_LEN, (u8 *)raw);
	if (ret < 0)
		return ret;
	if (ret != MPU6050_BURST_LEN)
		return -EIO;

	ax = be16_to_cpu(raw[0]);
	ay = be16_to_cpu(raw[1]);
	az = be16_to_cpu(raw[2]);
	temp = be16_to_cpu(raw[3]);
	gx = be16_to_cpu(raw[4]);
	gy = be16_to_cpu(raw[5]);
	gz = be16_to_cpu(raw[6]);

	mag = int_sqrt64((s64)ax * ax + (s64)ay * ay + (s64)az * az);

	dev_info(&client->dev,
		 "accel %d %d %d (|a| %u mg), temp %lld mC, gyro %d %d %d\n",
		 ax, ay, az, mag * 1000 / MPU6050_ACCEL_LSB_PER_G,
		 div_s64((s64)temp * 100000, chip->temp_sens_x100) +
			 chip->temp_offset_mc,
		 gx, gy, gz);
	return 0;
}

static int mpu6050_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	const struct mpu6050_chip *chip;
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

	ret = mpu6050_reset(client, chip);
	if (ret)
		return dev_err_probe(dev, ret, "reset failed\n");

	ret = mpu6050_configure(client, chip);
	if (ret)
		return dev_err_probe(dev, ret, "configure failed\n");

	/* Accel khởi động 30 ms (Product Spec 6.2); chờ thêm vài chu kỳ 10 ms. */
	msleep(50);
	ret = mpu6050_dump_sample(client, chip);
	if (ret)
		return dev_err_probe(dev, ret, "burst read failed\n");

	dev_info(dev, "%s (WHO_AM_I 0x%02x), irq %d, 100 Hz, +-2 g\n",
		 chip->name, chip->whoami, client->irq);
	return 0;
}

static void mpu6050_remove(struct i2c_client *client)
{
	int ret;

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
