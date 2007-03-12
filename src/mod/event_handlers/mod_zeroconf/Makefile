# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
LOCAL_CFLAGS=-I$(PREFIX)/include/howl
LOCAL_LDFLAGS=-lhowl
LOCAL_OBJS=
include $(BASE)/build/modmake.rules

local_depend:
	$(BASE)/build/buildlib.sh $(BASE) install howl-1.0.0.tar.gz --prefix=$(PREFIX)
