# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to imapact your build
LOCAL_CFLAGS=
LOCAL_LDFLAGS=-lshout -lmp3lame
LOCAL_OBJS=
include $(BASE)/build/modmake.rules

local_depend:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install lame-3.97.tar.gz --prefix=$(PREFIX) --disable-shared --with-pic
	MOD_CFLAGS="${ALL_CFLAGS}" MOD_LDFLAGS="${LD_CFLAGS}" MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libshout-2.2.2.tar.gz --disable-shared --with-pic --prefix=$(PREFIX)
