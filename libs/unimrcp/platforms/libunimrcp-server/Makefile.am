MAINTAINERCLEANFILES 	    = Makefile.in

INCLUDES                    = -I$(top_srcdir)/platforms/libunimrcp-server/include \
                              -I$(top_srcdir)/modules/mrcp-sofiasip/include \
                              -I$(top_srcdir)/modules/mrcp-unirtsp/include \
                              -I$(top_srcdir)/libs/mrcp-server/include \
                              -I$(top_srcdir)/libs/mrcp-engine/include \
                              -I$(top_srcdir)/libs/mrcp-signaling/include \
                              -I$(top_srcdir)/libs/mrcpv2-transport/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                              -I$(top_srcdir)/build \
                              $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

lib_LTLIBRARIES             = libunimrcpserver.la

include_HEADERS             = include/unimrcp_server.h

libunimrcpserver_la_SOURCES = src/unimrcp_server.c

libunimrcpserver_la_LIBADD  = $(top_builddir)/modules/mrcp-sofiasip/libmrcpsofiasip.la \
                              $(top_builddir)/modules/mrcp-unirtsp/libmrcpunirtsp.la \
                              $(top_builddir)/libs/mrcp-server/libmrcpserver.la \
                              $(top_builddir)/libs/mrcp-signaling/libmrcpsignaling.la \
                              $(top_builddir)/libs/mrcpv2-transport/libmrcpv2transport.la \
                              $(top_builddir)/libs/mrcp-engine/libmrcpengine.la \
                              $(top_builddir)/libs/mrcp/libmrcp.la \
                              $(top_builddir)/libs/mpf/libmpf.la \
                              $(top_builddir)/libs/apr-toolkit/libaprtoolkit.la \
                              $(UNIMRCP_APR_LIBS) $(UNIMRCP_APU_LIBS) $(UNIMRCP_SOFIA_LIBS) -lm

libunimrcpserver_la_LDFLAGS = $(UNI_LT_VERSION)
