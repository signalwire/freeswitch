BASE=../../../../..

LOCAL_CFLAGS += -I../ -I./
LOCAL_OBJS= main.o ../aws.o
LOCAL_SOURCES= main.c
include $(BASE)/build/modmake.rules

local_all:
	libtool --mode=link gcc main.o ../aws.o -o test test_aws.la

local_clean:
	-rm test
