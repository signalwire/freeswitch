BASE=../../../..
DIR=$(switch_srcdir)/libs/libg723_1
BUILDDIR=$(switch_bulddir)/libs/libg723_1
A=$(BUILDDIR)/.libs/libg723.a

LOCAL_INSERT_CFLAGS=if test -f $(A); then echo "-I$(DIR)/src/include" ; else echo "-DG723_PASSTHROUGH" ; fi ;
LOCAL_INSERT_LDFLAGS=test ! -f $(A) || echo $(A) 

include $(BASE)/build/modmake.rules
