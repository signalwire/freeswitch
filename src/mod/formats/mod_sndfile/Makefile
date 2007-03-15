# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
MODNAME=mod_sndfile

BASE=../../../..

SNDFILELA=$(BASE)/libs/libsndfile/src/libsndfile.la

LOCAL_CFLAGS=-I$(BASE)/libs/libsndfile/src
LOCAL_LDFLAGS=$(SNDFILELA)
LOCAL_OBJS=

include $(BASE)/build/modmake.rules

local_depend: $(SNDFILELA)

$(SNDFILELA):
	cd $(BASE)/libs/libsndfile && $(MAKE)
