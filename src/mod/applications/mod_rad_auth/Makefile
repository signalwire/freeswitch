switch_srcdir=../../../..

RADCLIENT_VERSION=1.1.6
RADCLIENT=freeradius-client-$(RADCLIENT_VERSION)
RADCLIENT_DIR=$(switch_srcdir)/libs/$(RADCLIENT)
RADCLIENT_BUILDDIR=$(switch_builddir)/libs/$(RADCLIENT)
RADCLIENT_LIBDIR=$(RADCLIENT_BUILDDIR)/lib
RADCLIENT_LA=${RADCLIENT_LIBDIR}/libfreeradius-client.la

LOCAL_CFLAGS=-I$(RADCLIENT_DIR)/include
LOCAL_LIBADD=$(RADCLIENT_LA)

include $(switch_srcdir)/build/modmake.rules

