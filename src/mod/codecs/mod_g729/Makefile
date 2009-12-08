BASE=../../../..

DIR=$(switch_srcdir)/libs/libg729
BUILDDIR=$(switch_builddir)/libs/libg729
A=$(BUILDDIR)/.libs/libg729.a

LOCAL_INSERT_CFLAGS=if test -f $(A); then echo "-I$(DIR)/src/include" ; else echo "-DG729_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -f $(A) || echo $(A) 
include $(BASE)/build/modmake.rules
