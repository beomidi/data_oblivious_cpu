#KERNEL ?= $(shell uname -r)
obj-m += pteditor.o 
#ccflags-y += -Wno-unused-result -I/usr/src/linux-headers-${KERNEL}/include/linux/ -DCC_USING_FENTRY
ccflags-y += -Wno-unused-result -I/home/behnam/Projects/chipyard/software/keystone/linux/arch/riscv/ -DCC_USING_FENTRY
all:
	#make -C /lib/modules/${KERNEL}/build M=$(PWD) modules
	make -C /home/behnam/Projects/chipyard/software/keystone/build/linux.build/ CROSS_COMPILE=riscv64-unknown-linux-gnu- ARCH=riscv M=$(PWD) modules
clean:
	#make -C /lib/modules/${KERNEL}/build M=$(PWD) clean
	rm -rf pteditor.mod* pteditor.o pteditor.ko Module.symvers modules.order
