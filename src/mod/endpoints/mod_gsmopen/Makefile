MODNAME=mod_gsmopen
SVNDEF := -D'GSMOPEN_SVN_VERSION="$(shell svnversion -n .)"'
LOCAL_CFLAGS += $(SVNDEF) -I../../../../libs/spandsp/src -I../../../..//libs/tiff-3.8.2/libtiff
LOCAL_LDFLAGS=-L../../../../libs/spandsp/src -lspandsp -lctb-0.16
LOCAL_OBJS=gsmopen_protocol.o
include ../../../../build/modmake.rules
