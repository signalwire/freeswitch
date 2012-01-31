BASE=../../../..

ESL_DIR = $(switch_srcdir)/libs/esl

LOCAL_LDFLAGS=-L$(ESL_DIR) -lesl
LOCAL_OBJS=$(ESL_DIR)/libesl.so
LOCAL_LIBADD=$(ESL_DIR)/libesl.so
LOCAL_CFLAGS=-I$(ESL_DIR)/src/include

include $(BASE)/build/modmake.rules

$(ESL_DIR)/libesl.so: $(ESL_DIR)/Makefile
	cd $(ESL_DIR) && $(MAKE) -j1
