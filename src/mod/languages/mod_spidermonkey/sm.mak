CFLAGS += -I$(PREFIX)/include/js -I$(PREFIX)/include/nspr -DXP_UNIX -I../mod_spidermonkey -DJS_THREADSAFE -DJS_HAS_FILE_OBJECT=1
LDFLAGS += -lnspr4 -ljs -lcurl
