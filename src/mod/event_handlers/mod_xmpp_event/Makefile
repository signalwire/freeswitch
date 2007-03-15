# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
MODNAME=mod_xmpp_event
BASE=../../../..

IKSEMELLA=$(BASE)/libs/iksemel/src/libiksemel.la

LOCAL_CFLAGS=-I$(BASE)/libs/iksemel/include
LOCAL_LDFLAGS=$(IKSEMELA)
LOCAL_OBJS=
include $(BASE)/build/modmake.rules

local_depend: $(IKSEMELLA)

$(IKSEMELLA):
	cd $(BASE)/libs/iksemel && $(MAKE)
