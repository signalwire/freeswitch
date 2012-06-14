switch_srcdir=../../../..

ifdef FREERADIUSDIR
FREERADIUSLA=$(FREERADIUSDIR)/libfreeradius-client.so
else
FREERADIUSLA=/usr/local/lib/libfreeradius-client.so
endif


LOCAL_LIBADD=$(FREERADIUSLA)

include $(switch_srcdir)/build/modmake.rules

