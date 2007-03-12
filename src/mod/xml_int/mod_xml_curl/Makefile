# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to imapact your build
LOCAL_CFLAGS=-I$(BASE)/libs/curl/include
LOCAL_LDFLAGS=$(BASE)/libs/curl/lib/libcurl.la
LOCAL_OBJS=
include $(BASE)/build/modmake.rules

local_depend:
	cd $(BASE)/libs/curl && $(MAKE)
