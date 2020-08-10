PREFIX = /usr
CC = gcc
CC_W32 = i686-mingw32-gcc
OBJS = bfcp_server.o bfcp_link_list.o bfcp_floor_list.o bfcp_user_list.o
OBJS_W32 = bfcp_server.obj bfcp_link_list.obj bfcp_floor_list.obj bfcp_user_list.obj
OPTS = -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -O2
LIBS = -lbfcpmsg -lpthread -lssl -lcrypto
LIBS_W32 = -lbfcpmsg -lws2_32 -lpthreadGC2 -lssleay32 -llibeay32 #-s
DEPS_W32 = -L./
WIN32 = -DWIN32
BASE = ../../../..
INC = -I$(BASE)/src/include -I$(BASE)/libs/libteletone/src
so : $(OBJS)
	$(CC) -ggdb -shared -Wl,-soname,libbfcpsrv.so.0 -o libbfcpsrv.so.0.0.7 $(OBJS) $(OPTS) $(LIBS)
%.o: %.c
	$(CC) -fPIC -ggdb -c $(INC) $< -o $@ $(OPTS)

dll : $(OBJS_W32)
	$(CC_W32) $(WIN32) -ggdb -shared -o libbfcpsrv.dll $(OBJS_W32) $(OPTS) $(DEPS_W32) $(LIBS_W32)

%.obj: %.c
	$(CC_W32) $(WIN32) -ggdb -c $< -o $@ $(OPTS)

linux : so main.o $(OBJS)
	$(CC) -ggdb -o bfcp_server main.o $(OBJS) $(OPTS) $(LIBS) -lbfcpsrv 

stress : stress_server.o $(OBJS)
	$(CC) -ggdb -o stress_server stress_server.o $(OBJS) $(OPTS) $(LIBS)

win32 : main.obj $(OBJS_W32)
	$(CC_W32) $(WIN32) -ggdb -o bfcp_server.exe main.obj $(OBJS_W32) $(OPTS) $(DEPS_W32) $(LIBS_W32)

clean :
	rm -f *.o *.obj libbfcpsrv* bfcp_server bfcp_server.exe

all: clean so dll linux win32

install:
	@echo Installing BFCP server library to $(PREFIX)/lib/:
	install -m 755 libbfcpsrv.so.0.0.7 $(PREFIX)/lib/
	ln -sf $(PREFIX)/lib/libbfcpsrv.so.0.0.7 $(PREFIX)/lib/libbfcpsrv.so.0
	ln -sf $(PREFIX)/lib/libbfcpsrv.so.0 $(PREFIX)/lib/libbfcpsrv.so
	@echo Installing BFCP server headers to $(PREFIX)/include/:
	install -m 755 bfcp_server.h $(PREFIX)/include/
	install -m 755 bfcp_threads.h $(PREFIX)/include/
	install -m 755 bfcp_user_list.h $(PREFIX)/include/
	install -m 755 bfcp_link_list.h $(PREFIX)/include/
	install -m 755 bfcp_floor_list.h $(PREFIX)/include/

uninstall:
	@echo Uninstalling BFCP server library from $(PREFIX)/lib/:
	rm -f $(PREFIX)/lib/libbfcpsrv*
	@echo Uninstalling BFCP server headers from $(PREFIX)/include/:
	rm -f $(PREFIX)/include/bfcp_server.h
	rm -f $(PREFIX)/include/bfcp_threads.h
	rm -f $(PREFIX)/include/bfcp_user_list.h
	rm -f $(PREFIX)/include/bfcp_link_list.h
	rm -f $(PREFIX)/include/bfcp_floor_list.h
