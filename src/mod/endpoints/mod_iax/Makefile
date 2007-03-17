# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build

BASE=../../../..
IAXLA=$(BASE)/libs/iax/src/libiax.la
LOCAL_CFLAGS=-I$(BASE)/libs/iax/src
LOCAL_LDFLAGS=
LOCAL_LIBADD=$(IAXLA)
include $(BASE)/build/modmake.rules

$(IAXLA):
	cd $(BASE)/libs/iax && $(MAKE)

