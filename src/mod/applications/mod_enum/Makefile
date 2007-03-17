
BASE=../../../..
UDNSA=$(BASE)/libs/udns/libudns.a
LOCAL_CFLAGS=-I$(BASE)/libs/udns
LOCAL_LIBADD=$(UDNSA)
include $(BASE)/build/modmake.rules

$(UDNSA):
	cd $(BASE)/libs/udns && $(MAKE) staticlib


