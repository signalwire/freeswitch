BASE=../../../..

export PTLIBDIR = $(shell /usr/bin/ptlib-config --ptlibdir)
LOCAL_CFLAGS+=-g -I$PTLIBDIR -I/usr/include/openh323  -I. -DPTRACING=1 -D_REENTRANT -fno-exceptions
LOCAL_LDFLAGS= -L/usr/lib -lopenh323 -lpt -lrt 

ifeq ($(shell uname -m),x86_64)
LOCAL_CFLAGS+=-DP_64BIT
endif

include $(BASE)/build/modmake.rules
