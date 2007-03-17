# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build


BASE=../../../..

SPEEXLA=$(BASE)/libs/speex/libspeex/libspeex.la

LOCAL_CFLAGS=-I$(BASE)/libs/speex/include
LOCAL_LDFLAGS=$(SPEEXLA)
LOCAL_OBJS=
include $(BASE)/build/modmake.rules

local_depend: $(SPEEXLA)

$(SPEEXLA): $(BASE)/libs/speex/stamp-h1
	cd $(BASE)/libs/speex && $(MAKE)
