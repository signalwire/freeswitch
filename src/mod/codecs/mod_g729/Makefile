BASE=../../../..
LOCAL_INSERT_CFLAGS=if test -d $(BASE)/libs/libg729; then echo "-I$(PREFIX)/include/libg729" ; else echo "-DG729_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -d $(BASE)/libs/libg729 || echo "-lg729" 
include $(BASE)/build/modmake.rules
