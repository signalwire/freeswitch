BASE=../../../..
DIR=$(BASE)/libs/libg729
A=$(DIR)/.libs/libg729.a

LOCAL_INSERT_CFLAGS=if test -f $(A); then echo "-I$(DIR)/src/include" ; else echo "-DG729_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -f $(A) || echo $(A) 
include $(BASE)/build/modmake.rules
