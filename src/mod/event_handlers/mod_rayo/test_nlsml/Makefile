BASE=../../../../..

IKS_DIR=$(BASE)/libs/iksemel
IKS_LA=$(IKS_DIR)/src/libiksemel.la
LOCAL_CFLAGS += -I../ -I$(BASE)/libs/iksemel/include
LOCAL_OBJS= $(PCRE_LA) $(IKS_LA) main.o ../nlsml.o ../iks_helpers.o
LOCAL_SOURCES= main.c
include $(BASE)/build/modmake.rules

$(IKS_LA): $(IKS_DIR) $(IKS_DIR)/.update
	@cd $(IKS_DIR) && $(MAKE)
	@$(TOUCH_TARGET)

local_all:
	libtool --mode=link gcc main.o ../nlsml.o ../iks_helpers.o  test_nlsml.la ../../../../../.libs/libfreeswitch.la ../../../../../libs/iksemel/src/.libs/libiksemel.a -lpcre -lssl -lcrypto -g -ggdb -O2 -pthread -o test
 
local_clean:
	-rm test
