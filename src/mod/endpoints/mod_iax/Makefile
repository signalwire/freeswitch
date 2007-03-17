BASE=../../../..

IAX_DIR=$(BASE)/libs/iax
IAXLA=$(IAX_DIR)/src/libiax.la
LOCAL_CFLAGS=-I$(IAX_DIR)/src
LOCAL_LIBADD=$(IAXLA)
include $(BASE)/build/modmake.rules

$(IAXLA): $(IAX_DIR) $(IAX_DIR)/.update
	cd $(IAX_DIR) && $(MAKE)

