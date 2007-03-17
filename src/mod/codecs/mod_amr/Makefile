BASE=../../../..

LOCAL_INSERT_CFLAGS=if test -d $(BASE)/libs/amr; then echo "-I$(PREFIX)/include/amr" ; else echo "-DAMR_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -d $(BASE)/libs/amp || echo "-lamr"

include $(BASE)/build/modmake.rules
