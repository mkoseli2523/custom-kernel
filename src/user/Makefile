# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

ULIB_OBJS = \
	string.o \
	io.o \
	syscall.o \
	start.o

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I.

all: bin/trek bin/init0 bin/init1 bin/init2 bin/init_cat bin/init_ioctl bin/test_kernel_access bin/test_kernel_write bin/test_execute_noexec bin/test_nonreadable bin/test_nonwritable bin/test_extra_credit

bin/init0: $(ULIB_OBJS) init0.o
	$(LD) -T user.ld -o $@ $^

bin/init1: $(ULIB_OBJS) init1.o
	$(LD) -T user.ld -o $@ $^

bin/init2: $(ULIB_OBJS) init2.o
	$(LD) -T user.ld -o $@ $^

bin/init_cat: $(ULIB_OBJS) init_cat.o
	$(LD) -T user.ld -o $@ $^

bin/init_ioctl: $(ULIB_OBJS) init_ioctl.o
	$(LD) -T user.ld -o $@ $^

bin/test_kernel_access: $(ULIB_OBJS) test_kernel_access.o
	$(LD) -T user.ld -o $@ $^

bin/test_kernel_write: $(ULIB_OBJS) test_kernel_write.o
	$(LD) -T user.ld -o $@ $^

bin/test_execute_noexec: $(ULIB_OBJS) test_execute_noexec.o
	$(LD) -T user.ld -o $@ $^

bin/test_nonreadable: $(ULIB_OBJS) test_nonreadable.o
	$(LD) -T user.ld -o $@ $^

bin/test_nonwritable: $(ULIB_OBJS) test_nonwritable.o
	$(LD) -T user.ld -o $@ $^

bin/test_extra_credit: $(ULIB_OBJS) test_extra_credit.o
	$(LD) -T user.ld -o $@ $^

clean:
	rm -rf *.o *.elf *.asm bin/init0 bin/init1 bin/init2
