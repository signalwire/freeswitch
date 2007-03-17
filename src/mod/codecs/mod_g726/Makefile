
BASE=../../../..
G726LA=$(BASE)/libs/codec/g726/libg726.la
LOCAL_CFLAGS=-I$(BASE)/libs/codec/g726/src
LOCAL_LIBADD=$(G726LA)
include $(BASE)/build/modmake.rules

$(G726LA):
	cd $(BASE)/libs/codec/g726 && $(MAKE)

