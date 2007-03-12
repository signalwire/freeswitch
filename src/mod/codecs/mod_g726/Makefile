# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to imapact your build
LOCAL_CFLAGS=-I$(BASE)/libs/codec/g726/src
LOCAL_LDFLAGS=
LOCAL_OBJS=$(BASE)/libs/codec/g726/libg726.la
include $(BASE)/build/modmake.rules

local_depend:
	cd $(BASE)/libs/codec/g726 && $(MAKE)

