MODNAME=mod_skypiax
SVNDEF := -D'SKYPIAX_SVN_VERSION="$(shell svnversion -n .)"'
LOCAL_CFLAGS += $(SVNDEF)
LOCAL_LDFLAGS=-lX11
LOCAL_OBJS=skypiax_protocol.o
include ../../../../build/modmake.rules
