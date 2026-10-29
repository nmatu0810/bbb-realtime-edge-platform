# Kbuild cho module ngoài cây. Yocto (module.bbclass) gọi với KERNEL_SRC trỏ vào
# kernel đã cấu hình (STAGING_KERNEL_DIR); module phải build lại mỗi khi đổi kernel.
obj-m := mpu6050.o

# Header uapi dùng chung với userspace: #include <projectb/mpu6050.h>. Với M=, kernel
# để VPATH rỗng nên $(src) là đường dẫn tuyệt đối tới thư mục này.
ccflags-y += -I$(src)/include/uapi

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions
