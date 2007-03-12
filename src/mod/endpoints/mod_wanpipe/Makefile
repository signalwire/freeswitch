# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
LOCAL_CFLAGS  =-I/usr/local/include -I/usr/src/libpri -I/usr/src/linux/include -I. -I/usr/include 
LOCAL_CFLAGS +=-D__LINUX__ -D_REENTRANT -D_GNU_SOURCE -DAFT_A104 -DWANPIPE_TDM_API
LOCAL_LDFLAGS=-lsangoma
LOCAL_OBJS=ss7boost_client.o
include $(BASE)/build/modmake.rules

local_depend:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libsangoma --prefix=$(PREFIX) --with-libpri=/usr/src/libpri
