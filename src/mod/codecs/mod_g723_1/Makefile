BASE=../../../..

LOCAL_INSERT_CFLAGS=if test -d $(BASE)/libs/libg723_1; then echo "-I$(PREFIX)/include/g723" ; else echo "-DG723_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -d $(BASE)/libs/libg723_1 || echo "-lg723"

include $(BASE)/build/modmake.rules
