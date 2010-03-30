MODNAME=mod_gsmopen
SVNDEF := -D'GSMOPEN_SVN_VERSION="$(shell svnversion -n .)"'
#LOCAL_CFLAGS += $(SVNDEF) -I/usr/src/gsmlib-1.10
LOCAL_CFLAGS += $(SVNDEF) -I../../../../libs/spandsp/src -I../../../..//libs/tiff-3.8.2/libtiff
#LOCAL_LDFLAGS=-lasound -L/usr/src/gsmlib-1.10/gsmlib/.libs -lgsmme
LOCAL_LDFLAGS=-L../../../../libs/spandsp/src -lasound -lgsmme -lspandsp
LOCAL_OBJS=gsmopen_protocol.o 
#OUR_OBJS += /usr/src/gsmlib-1.10/gsmlib/libgsmme.la
include ../../../../build/modmake.rules
