CFLAGS += -I$(PREFIX)/include/js -I$(PREFIX)/include/nspr -DXP_UNIX -I../mod_spidermonkey -DJS_THREADSAFE
LDFLAGS += -lnspr4 -ljs -lcurl
