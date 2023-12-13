obj-m += matfs_nvme_kernel.o
# CFLAGS = -O0 -g3 -ggdb 

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean