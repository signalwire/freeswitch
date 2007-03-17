
BASE=../../../..

topdir=$(shell cd $(BASE) && pwd)
ifeq ($(shell test -d $(topdir)/libs/amr && cd $(topdir)/libs/amr && pwd),$(topdir)/libs/amr)

LOCAL_CFLAGS = -I$(PREFIX)/include/amr
LOCAL_LDFLAGS =-lamr

else

LOCAL_CFLAGS = -DAMR_PASSTHROUGH

endif

include $(BASE)/build/modmake.rules
