PREFIX = /usr
CC = gcc
CC_W32 = i686-pc-mingw32-gcc
OBJS = bfcp_messages.o bfcp_messages_build.o bfcp_messages_parse.o bfcp_strings.o
OBJS_W32 = bfcp_messages.obj bfcp_messages_build.obj bfcp_messages_parse.obj bfcp_strings.obj
OPTS = -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -O2 -Werror -Wunused
LIBS = -lpthread
LIBS_W32 = -lws2_32 #-s
DEPS_W32 = -L./
WIN32 = -DWIN32
BASE = ../../..
INC = -I$(BASE)/src/include -I$(BASE)/libs/libteletone/src

so : $(OBJS)
	$(CC) -ggdb -shared -Wl,-soname,libbfcpmsg.so.0 -o libbfcpmsg.so.0.0.7 $(OBJS) $(OPTS) $(LIBS)

%.o: %.c
	$(CC) -fPIC -ggdb -c $(INC) $< -o $@ $(OPTS)

dll : $(OBJS_W32)
	$(CC_W32) $(WIN32) -ggdb -shared -o libbfcpmsg.dll $(OBJS_W32) $(OPTS) $(DEPS_W32) $(LIBS_W32)

%.obj: %.c
	$(CC_W32) $(WIN32) -ggdb -c $< -o $@ $(OPTS)

linux : testcode.o $(OBJS)
	$(CC) -ggdb -o testcode testcode.o $(OBJS) $(OPTS) $(LIBS) -lbfcpmsg

win32 : testcode.obj $(OBJS_W32)
	$(CC_W32) $(WIN32) -ggdb -o testcode.exe testcode.obj $(OBJS_W32) $(OPTS) $(DEPS_W32) $(LIBS_W32)

clean :
	rm -f *.o *.obj libbfcp* testcode testcode.exe

install:
	@echo Installing BFCP messages library to $(PREFIX)/lib/:
	install -m 755 libbfcpmsg.so.0.0.7 $(PREFIX)/lib/
	ln -sf $(PREFIX)/lib/libbfcpmsg.so.0.0.7 $(PREFIX)/lib/libbfcpmsg.so.0
	ln -sf $(PREFIX)/lib/libbfcpmsg.so.0 $(PREFIX)/lib/libbfcpmsg.so
	@echo Installing BFCP messages headers to $(PREFIX)/include/:
	install -m 755 bfcp_messages.h $(PREFIX)/include/
	install -m 755 bfcp_strings.h $(PREFIX)/include/

uninstall:
	@echo Uninstalling BFCP messages library from $(PREFIX)/lib/:
	rm -f $(PREFIX)/lib/libbfcpmsg*
	@echo Uninstalling BFCP messages headers from $(PREFIX)/include/:
	rm -f $(PREFIX)/include/bfcp_messages.h
	rm -f $(PREFIX)/include/bfcp_strings.h
