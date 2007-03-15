# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
MODNAME=mod_dingaling
BASE=../../../..

IKSEMELLA=$(BASE)/libs/iksemel/src/libiksemel.la
DINGLA=$(BASE)/libs/libdingaling/libdingaling.la

LOCAL_CFLAGS=-I$(BASE)/libs/libdingaling/src
LOCAL_LDFLAGS=
LOCAL_OBJS=
LOCAL_LIBADD=$(DINGLA)

include $(BASE)/build/modmake.rules

INST_DINGLA=$(PREFIX)/lib/libdingaling.la

$(IKSEMELLA):
	cd $(BASE)/libs/iksemel && $(MAKE)

$(DINGLA): $(IKSEMELLA)
	cd $(BASE)/libs/libdingaling && $(MAKE)

depend_install: $(INST_DINGLA)

$(INST_DINGLA): $(DINGLA)
	cd $(BASE)/libs/libdingaling && $(MAKE) install

