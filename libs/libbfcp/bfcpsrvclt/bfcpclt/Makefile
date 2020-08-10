PREFIX = /usr
CC = gcc
CC_W32 = i686-pc-mingw32-gcc
OBJS = bfcp_participant.o
OBJS_W32 = bfcp_participant.obj
OPTS = -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -O2
LIBS = -lbfcpmsg -lpthread -lssl -lcrypto
LIBS_W32 = -lbfcpmsg -lws2_32 -lpthreadGC2 -lssleay32 -llibeay32 #-s
DEPS_W32 = -L./
WIN32 = -DWIN32

so : $(OBJS)
	$(CC) -ggdb -shared -Wl,-soname,libbfcpclt.so.0 -o libbfcpclt.so.0.0.7 $(OBJS) $(OPTS) $(LIBS)

%.o: %.c
	$(CC) -fPIC -ggdb -c $< -o $@ $(OPTS)

dll : $(OBJS_W32)
	$(CC_W32) $(WIN32) -ggdb -shared -o libbfcpclt.dll $(OBJS_W32) $(OPTS) $(DEPS_W32) $(LIBS_W32)

%.obj: %.c
	$(CC_W32) $(WIN32) -ggdb -c $< -o $@ $(OPTS)

linux : main.o $(OBJS)
	$(CC) -ggdb -o bfcp_participant main.o $(OBJS) $(OPTS) $(LIBS)

win32 : dll main.obj
	$(CC_W32) $(WIN32) -ggdb -o bfcp_participant.exe main.obj $(OPTS) $(DEPS_W32) -lbfcpclt $(LIBS_W32)

clean :
	rm -f *.o *.obj libbfcpclt* bfcp_participant bfcp_participant.exe

all: clean so dll linux win32

install:
	@echo Installing BFCP client library to $(PREFIX)/lib/:
	install -m 755 libbfcpclt.so.0.0.7 $(PREFIX)/lib/
	ln -sf $(PREFIX)/lib/libbfcpclt.so.0.0.7 $(PREFIX)/lib/libbfcpclt.so.0
	ln -sf $(PREFIX)/lib/libbfcpclt.so.0 $(PREFIX)/lib/libbfcpclt.so
	@echo Installing BFCP client headers to $(PREFIX)/include/:
	install -m 755 bfcp_participant.h $(PREFIX)/include/

uninstall:
	@echo Uninstalling BFCP client library from $(PREFIX)/lib/:
	rm -f $(PREFIX)/lib/libbfcpclt*
	@echo Uninstalling BFCP client headers from $(PREFIX)/include/:
	rm -f $(PREFIX)/include/bfcp_participant.h
