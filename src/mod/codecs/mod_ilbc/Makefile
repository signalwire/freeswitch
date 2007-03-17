
BASE=../../../..
ILBCLA=$(BASE)/libs/codec/ilbc/libilbc.la
LOCAL_CFLAGS=-I$(BASE)/libs/codec/ilbc/src
LOCAL_LIBADD=$(ILBCLA)
include $(BASE)/build/modmake.rules

$(ILBCLA):
	cd $(BASE)/libs/codec/ilbc && $(MAKE)
