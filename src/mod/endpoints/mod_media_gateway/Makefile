ifndef ARCH
        ARCH=$(shell uname -m)
endif

ifeq ($(ARCH),x86_64)
        LOCAL_CFLAGS+=-DBIT_64 -DALIGN_64BIT
endif

BASE=../../../..
LOCAL_OBJS=media_gateway.o media_gateway_stack.o media_gateway_xml.o media_gateway_cli.o media_gateway_stack_alarms.o
LOCAL_LDFLAGS=-lsng_mg
include $(BASE)/build/modmake.rules
