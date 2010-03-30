MODNAME=mod_gsmopen
SVNDEF := -D'GSMOPEN_SVN_VERSION="$(shell svnversion -n .)"'
LOCAL_CFLAGS += $(SVNDEF) -Wno-error=address -DNO_GSMLIB -I../../../../../../libs/spandsp/src -I../../../../../..//libs/tiff-3.8.2/libtiff
LOCAL_LDFLAGS=-lasound  -L../../../../../../libs/spandsp/src -lspandsp
LOCAL_OBJS=gsmopen_protocol.o 
include ../../../../../../build/modmake.rules
