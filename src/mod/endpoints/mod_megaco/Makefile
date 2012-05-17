ifndef ARCH
        ARCH=$(shell uname -m)
endif

ifeq ($(ARCH),x86_64)
        LOCAL_CFLAGS+=-DBIT_64 -DALIGN_64BIT
endif

BASE=../../../..
LOCAL_OBJS=megaco.o megaco_stack.o megaco_xml.o
LOCAL_LDFLAGS=-lsng_megaco
include $(BASE)/build/modmake.rules
