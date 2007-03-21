BASE=../../../..
LOCAL_INSERT_CFLAGS=if test -d $(BASE)/libs/g729; then echo "-I$(PREFIX)/include/g729" ; else echo "-DG729_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -d $(BASE)/libs/g729 || echo "-L/usr/local/freeswitchlib -lg729" 
include $(BASE)/build/modmake.rules
