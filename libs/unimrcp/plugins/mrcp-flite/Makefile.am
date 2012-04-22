MAINTAINERCLEANFILES 	   = Makefile.in

INCLUDES                   = -Iinclude \
                             -I$(top_srcdir)/libs/mrcp-engine/include \
                             -I$(top_srcdir)/libs/mrcp/include \
                             -I$(top_srcdir)/libs/mrcp/message/include \
                             -I$(top_srcdir)/libs/mrcp/control/include \
                             -I$(top_srcdir)/libs/mrcp/resources/include \
                             -I$(top_srcdir)/libs/mpf/include \
                             -I$(top_srcdir)/libs/apr-toolkit/include \
                             $(UNIMRCP_APR_INCLUDES) \
                             $(UNIMRCP_APU_INCLUDES) \
                             $(UNIMRCP_FLITE_INCLUDES)

plugin_LTLIBRARIES         = mrcpflite.la

mrcpflite_la_SOURCES       = src/mrcp_flite.c src/flite_voices.c
mrcpflite_la_LDFLAGS       = -module $(PLUGIN_LT_VERSION)
mrcpflite_la_LIBADD        = $(UNIMRCP_FLITE_LIBS) -lm
