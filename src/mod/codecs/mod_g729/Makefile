BASE=../../../..
DIR=./g729abc
A=$(DIR)/libg729ab.a

LOCAL_INSERT_CFLAGS=if test -f $(A); then echo "-I$(DIR)" ; else echo "-DG729_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -f $(A) || echo $(A) 
include $(BASE)/build/modmake.rules
