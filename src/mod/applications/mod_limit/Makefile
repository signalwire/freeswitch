BASE=../../../..
include $(BASE)/build/modmake.rules
LOCAL_CFLAGS += `if test -f $(BASE)/.libs/libfreeswitch_la-switch_odbc.o ; then echo -DSWITCH_HAVE_ODBC; fi ;`
