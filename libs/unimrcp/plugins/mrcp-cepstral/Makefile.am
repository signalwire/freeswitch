MAINTAINERCLEANFILES 	   = Makefile.in

INCLUDES                   = -Iinclude \
                             -I$(top_srcdir)/libs/mrcp-engine/include \
                             -I$(top_srcdir)/libs/mrcp/include \
                             -I$(top_srcdir)/libs/mrcp/message/include \
                             -I$(top_srcdir)/libs/mrcp/control/include \
                             -I$(top_srcdir)/libs/mrcp/resources/include \
                             -I$(top_srcdir)/libs/mpf/include \
                             -I$(top_srcdir)/libs/apr-toolkit/include \
                             $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES) $(UNIMRCP_SWIFT_INCLUDES)

plugin_LTLIBRARIES         = mrcpcepstral.la

mrcpcepstral_la_SOURCES    = src/mrcp_swift.c
mrcpcepstral_la_LDFLAGS    = -module $(PLUGIN_LT_VERSION) $(UNIMRCP_SWIFT_LDFLAGS)
mrcpcepstral_la_LIBADD     = $(UNIMRCP_SWIFT_LIBS) 
