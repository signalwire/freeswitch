MODNAME=mod_gsm
BASE=../../../..
GSMLA=$(BASE)/libs/codec/gsm/libgsm.la
LOCAL_CFLAGS=-I$(BASE)/libs/codec/gsm/inc
LOCAL_LIBADD=$(GSMLA)
include $(BASE)/build/modmake.rules

$(GSMLA):
	cd $(BASE)/libs/codec/gsm && $(MAKE)
