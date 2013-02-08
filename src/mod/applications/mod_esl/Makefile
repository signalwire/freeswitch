BASE=../../../..

ESL_DIR = $(switch_srcdir)/libs/esl

LOCAL_OBJS=$(ESL_DIR)/libesl.a
LOCAL_CFLAGS=-I$(ESL_DIR)/src/include

include $(BASE)/build/modmake.rules

$(ESL_DIR)/libesl.a: $(ESL_DIR)/Makefile
	cd $(ESL_DIR) && $(MAKE) -j1
