MAINTAINERCLEANFILES 	    = Makefile.in

INCLUDES                    = -Iinclude \
                              -I$(top_srcdir)/platforms/libunimrcp-client/include \
                              -I$(top_srcdir)/libs/mrcp-client/include \
                              -I$(top_srcdir)/libs/mrcp-signaling/include \
                              -I$(top_srcdir)/libs/mrcpv2-transport/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                              $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

lib_LTLIBRARIES             = libasrclient.la

include_HEADERS             = include/asr_engine.h

libasrclient_la_SOURCES     = src/asr_engine.c

libasrclient_la_LIBADD      = $(top_builddir)/platforms/libunimrcp-client/libunimrcpclient.la
                              
libasrclient_la_LDFLAGS     = $(UNI_LT_VERSION)
