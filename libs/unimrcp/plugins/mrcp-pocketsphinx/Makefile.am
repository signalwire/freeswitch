MAINTAINERCLEANFILES 	    = Makefile.in

INCLUDES                    = -Iinclude \
                              -I$(top_srcdir)/libs/mrcp-engine/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                             $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

plugin_LTLIBRARIES          = mrcppocketsphinx.la

mrcppocketsphinx_la_SOURCES = src/mrcp_pocketsphinx.c.c
mrcppocketsphinx_la_LDFLAGS = -module $(PLUGIN_LT_VERSION)
