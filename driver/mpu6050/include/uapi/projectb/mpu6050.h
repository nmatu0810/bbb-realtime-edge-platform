/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Hợp đồng /dev/mpu6050 giữa driver và userspace (project B). Chốt ở GĐ1, GĐ2 chỉ
 * đọc, không sửa: đổi struct hay mã ioctl là phải sửa cả driver, app, sensord.
 *
 * read() trả một hoặc nhiều struct mpu6050_sample trọn vẹn, không bao giờ nửa bản
 * ghi. Cấu hình (sample_rate, accel_range) ghi qua /sys/class/misc/mpu6050/;
 * userspace đọc lại cấu hình và bộ đếm bằng ioctl, không cần đọc sysfs.
 *
 * Kernel include bằng -I.../include/uapi, userspace cài vào /usr/include/projectb/.
 * Thư mục projectb/ để không trùng với header nào của kernel hay libc.
 */
#ifndef _UAPI_PROJECTB_MPU6050_H
#define _UAPI_PROJECTB_MPU6050_H

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * Một mẫu, 16 byte trên mọi ABI: bốn __s16 chiếm 8 byte đầu nên ts_ns luôn ở offset
 * 8, không phụ thuộc __u64 căn 4 (i386) hay 8 (ARM EABI, x86_64).
 * ts_ns là CLOCK_MONOTONIC (ktime_get_ns), so được với clock_gettime(CLOCK_MONOTONIC).
 */
struct mpu6050_sample {
	__s16 ax, ay, az;	/* LSB; chia accel_lsb_per_g ra g */
	__s16 temp;		/* thô; m°C = temp * 100000 / temp_sens_x100 + temp_offset_mc */
	__u64 ts_ns;
};

/* Ảnh chụp cấu hình đang áp, kèm hằng số đổi đơn vị để userspace khỏi biết chip. */
struct mpu6050_config {
	__u32 whoami;		/* 0x70: họ MPU-6500, 0x68: MPU-6050 */
	__u32 sample_rate_hz;	/* tốc độ thật sau làm tròn, như sysfs sample_rate */
	__u32 accel_range_g;	/* 2, 4, 8, 16 */
	__u32 accel_lsb_per_g;	/* 16384 >> ACCEL_FS_SEL */
	__u32 accel_bw_hz;	/* băng thông DLPF accel đang chọn */
	__s32 temp_sens_x100;	/* LSB/°C × 100 */
	__s32 temp_offset_mc;	/* m°C khi temp = 0 */
};

struct mpu6050_stats {
	__u64 dropped;		/* mẫu mất từ lúc probe (fifo đầy, nguồn lỡ nhịp), như sysfs dropped */
};

#define MPU6050_IOC_MAGIC	'x'	/* không có trong ioctl-number.rst của 6.12 */

#define MPU6050_IOC_GET_CONFIG	_IOR(MPU6050_IOC_MAGIC, 1, struct mpu6050_config)
#define MPU6050_IOC_GET_STATS	_IOR(MPU6050_IOC_MAGIC, 2, struct mpu6050_stats)
#define MPU6050_IOC_FLUSH	_IO(MPU6050_IOC_MAGIC, 3)	/* bỏ mẫu đang chờ đọc */

/* Kiểm ABI ở mọi nơi include: kernel (gnu11), app C, sensord C++17. g++ không có
 * _Static_assert nên phải tách nhánh. */
#ifdef __cplusplus
#define MPU6050_ABI_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define MPU6050_ABI_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

MPU6050_ABI_ASSERT(sizeof(struct mpu6050_sample) == 16, "mpu6050_sample ABI");
MPU6050_ABI_ASSERT(sizeof(struct mpu6050_config) == 28, "mpu6050_config ABI");
MPU6050_ABI_ASSERT(sizeof(struct mpu6050_stats) == 8, "mpu6050_stats ABI");

#undef MPU6050_ABI_ASSERT

#endif /* _UAPI_PROJECTB_MPU6050_H */
