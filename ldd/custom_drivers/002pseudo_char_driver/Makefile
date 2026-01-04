obj-m := pcd.o
ARCH=arm
# Detect platform and set paths accordingly
MACHINE := $(shell uname -m)
ifeq ($(MACHINE),aarch64)
    # Jetson (ARM64/aarch64)
    CROSS_COMPILE=arm-none-linux-gnueabihf-
    KERN_DIR=/home/nvidia/beaglebone-linux/ldd/source/linux/
else
    # x86_64
    CROSS_COMPILE=arm-linux-gnueabihf-
    KERN_DIR=/home/karthik/workspace/ldd/source/linux/
endif
HOST_KERN_DIR=/lib/modules/$(shell uname -r)/build/

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) modules
clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) clean
help:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) help
host:
	make -C $(HOST_KERN_DIR) M=$(PWD) modules
host_clean:
	make -C $(HOST_KERN_DIR) M=$(PWD) clean
host_help:
	make -C $(HOST_KERN_DIR) M=$(PWD) help

