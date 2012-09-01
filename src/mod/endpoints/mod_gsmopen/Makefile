MODNAME=mod_gsmopen
LOCAL_CFLAGS += -I../../../../libs/spandsp/src -I../../../..//libs/tiff-4.0.2/libtiff -DGSMOPEN_C_VER=\"`git log -1 --format="%h" gsmopen_protocol.cpp`\" -DMODGSMOPEN_C_VER=\"`git log -1 --format="%h" mod_gsmopen.cpp`\"
LOCAL_LDFLAGS=-L../../../../libs/spandsp/src -lspandsp -lctb-0.16 -lgsmme
LOCAL_OBJS=gsmopen_protocol.o
include ../../../../build/modmake.rules
