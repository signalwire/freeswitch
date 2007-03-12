include ../mod_spidermonkey/sm.mak
# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to imapact your build
#LOCAL_CFLAGS=
LOCAL_LDFLAGS+=-lodbc
#LOCAL_OBJS=
include $(BASE)/build/modmake.rules

local_depend:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install unixODBC-2.2.12.tar.gz --prefix=$(PREFIX) --disable-gui --without-x --with-pic

