# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
LOCAL_CFLAGS=-I$(BASE)/libs/udns
LOCAL_LDFLAGS=
LOCAL_OBJS=$(BASE)/libs/udns/libudns.a
include $(BASE)/build/modmake.rules

local_depend:
	cd $(BASE)/libs/udns && $(MAKE) staticlib


