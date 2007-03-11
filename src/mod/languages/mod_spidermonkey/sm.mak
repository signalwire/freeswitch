CFLAGS += -I$(BASE)/libs/js/src -I$(BASE)/libs/js/nsprpub/dist/include/nspr  -DXP_UNIX -I../mod_spidermonkey  -DJS_THREADSAFE -DJS_HAS_FILE_OBJECT=1
LDFLAGS += -L$(BASE)/libs/js/nsprpub/pr/src -L$(BASE)/libs/js/nsprpub/dist/lib -lnspr4 $(BASE)/libs/js/libjs.la
