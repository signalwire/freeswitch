BASE=../../../..
DIR=$(BASE)/libs/broadvoice
A=$(DIR)/src/.libs/libbroadvoice.a

LOCAL_INSERT_CFLAGS=if test -f $(A); then echo "-I$(DIR)/src" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -f $(A) || echo $(A) 
include $(BASE)/build/modmake.rules
