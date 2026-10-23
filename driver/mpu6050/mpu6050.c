// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver MPU6050 / MPU-6500 trên I2C cho project B (BeagleBone Black).
 *
 * Chip thật trên module là họ MPU-6500 (WHO_AM_I = 0x70), tên mpu6050 giữ cho cả
 * project. Thanh ghi theo MPU-6500 Register Map rev 2.1 (RM-MPU-6500A-00).
 *
 * Tuần 4, bước 1: khung i2c_driver. Probe chỉ đọc WHO_AM_I để biết đã nói chuyện
 * được với chip; chưa reset, chưa cấu hình, chưa char device.
 */
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

#define MPU6050_REG_WHO_AM_I	0x75

#define MPU6050_WHOAMI_6050	0x68
#define MPU6050_WHOAMI_6500	0x70

static int mpu6050_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	int id;

	id = i2c_smbus_read_byte_data(client, MPU6050_REG_WHO_AM_I);
	if (id < 0)
		return dev_err_probe(dev, id, "cannot read WHO_AM_I\n");

	switch (id) {
	case MPU6050_WHOAMI_6500:
		dev_info(dev, "MPU-6500 family (WHO_AM_I 0x%02x), irq %d\n",
			 id, client->irq);
		break;
	case MPU6050_WHOAMI_6050:
		dev_info(dev, "MPU-6050 (WHO_AM_I 0x%02x), irq %d\n",
			 id, client->irq);
		break;
	default:
		dev_err(dev, "unknown WHO_AM_I 0x%02x\n", id);
		return -ENODEV;
	}

	return 0;
}

static void mpu6050_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "removed\n");
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
