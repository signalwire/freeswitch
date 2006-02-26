CFLAGS += -I$(BASE)/libs/mozilla/js/src -Wall -Wno-format -g -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R -DX86_LINUX  -DDEBUG -DDEBUG_root -DJS_THREADSAFE -I$(BASE)/libs/mozilla/js/src -I$(BASE)/libs/mozilla/js/src/Linux_All_DBG.OBJ -Wall -Wno-format -g -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R -DX86_LINUX  -DDEBUG -DDEBUG_root -DJS_THREADSAFE -I$(BASE)/libs/mozilla/nsprpub/dist/include/nspr -ILinux_All_DBG.OBJ
LDFLAGS +=-DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R -DX86_LINUX  -DDEBUG -DDEBUG_root -DJS_THREADSAFE -I$(BASE)/libs/mozilla/nsprpub/dist/include/nspr -Wall -Wno-format -g -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DHAVE_LOCALTIME_R -DX86_LINUX  -DDEBUG -DDEBUG_root -DJS_THREADSAFE -I$(BASE)/libs/mozilla/nsprpub/dist/include/nspr
OBJS=$(BASE)/libs/mozilla/js/src/Linux_All_DBG.OBJ/libjs.a $(BASE)/libs/mozilla/nsprpub/dist/lib/libnspr4.a
LINKER=$(CC)


all:	depends $(MODNAME).so

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) mozilla --prefix=$(PREFIX)

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).so  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod
