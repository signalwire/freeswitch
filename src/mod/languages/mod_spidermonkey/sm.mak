CFLAGS += -I$(PREFIX)/include/js -I$(PREFIX)/include/nspr -DXP_UNIX -I../mod_spidermonkey
LDFLAGS += -lnspr4 -ljs -lcurl
