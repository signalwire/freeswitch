BASE=../../../..

SNDFILE_DIR=$(BASE)/libs/libsndfile
SNDFILELA=$(SNDFILE_DIR)/src/libsndfile.la

LOCAL_CFLAGS=-I$(SNDFILE_DIR)/src
LOCAL_LIBADD=$(SNDFILELA)
LOCAL_OBJS=

include $(BASE)/build/modmake.rules

$(SNDFILELA): $(SNDFILE_DIR) $(SNDFILE_DIR)/.update
	cd $(SNDFILE_DIR) && $(MAKE)
	$(TOUCH_TARGET)
