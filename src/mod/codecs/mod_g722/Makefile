MODNAME=mod_g722
BASE=../../../..
G7XXLA=$(BASE)/libs/codec/g7xx/libg7xx.la
LOCAL_CFLAGS=-I$(BASE)/libs/codec/g7xx/src/include/
LOCAL_LIBADD=$(G7XXLA)
include $(BASE)/build/modmake.rules

$(G7XXLA):
	cd $(BASE)/libs/codec/g7xx && $(MAKE)

