MODNAME=mod_gsmopen
SVNDEF := -D'GSMOPEN_SVN_VERSION="$(shell svnversion -n .)"'
LOCAL_CFLAGS += $(SVNDEF) -DNO_ALSA  -I../../../../../../libs/spandsp/src -I../../../../../..//libs/tiff-3.8.2/libtiff
LOCAL_LDFLAGS=-lgsmme -L../../../../../../libs/spandsp/src -avoid-version -module -no-undefined -shared -lspandsp
LOCAL_OBJS=gsmopen_protocol.o 
include ../../../../../../build/modmake.rules
