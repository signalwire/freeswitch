MAINTAINERCLEANFILES 	   = Makefile.in

INCLUDES                   = -Iinclude \
                             -I$(top_srcdir)/libs/mrcp-engine/include \
                             -I$(top_srcdir)/libs/mrcp/include \
                             -I$(top_srcdir)/libs/mrcp/message/include \
                             -I$(top_srcdir)/libs/mrcp/control/include \
                             -I$(top_srcdir)/libs/mrcp/resources/include \
                             -I$(top_srcdir)/libs/mpf/include \
                             -I$(top_srcdir)/libs/apr-toolkit/include \
                             $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

plugin_LTLIBRARIES         = mrcprecorder.la

mrcprecorder_la_SOURCES    = src/mrcp_recorder_engine.c
mrcprecorder_la_LDFLAGS    = -module $(PLUGIN_LT_VERSION)
