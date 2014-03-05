# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y
#LDDINC=

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSKYPOPEN_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2 -Wall
endif

EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I$(LDDINC)

ifneq ($(KERNELRELEASE),)
# call from kernel build system

option-objs := main.o

obj-m	:= option.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINC=$(PWD)/../include modules

endif



clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif
