# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
MODNAME=mod_ilbc
LOCAL_CFLAGS=-I$(BASE)/libs/codec/ilbc/src
LOCAL_LDFLAGS=$(BASE)/libs/codec/ilbc/libilbc.la
LOCAL_OBJS=
include ../../../../build/modmake.rules

local_depend:
	cd $(BASE)/libs/codec/ilbc && $(MAKE)
