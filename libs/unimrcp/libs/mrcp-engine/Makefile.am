MAINTAINERCLEANFILES 	    = Makefile.in

INCLUDES                    = -I$(top_srcdir)/libs/mrcp-engine/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                              $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES          = libmrcpengine.la

include_HEADERS             = include/mrcp_resource_plugin.h \
                              include/mrcp_resource_engine.h

libmrcpengine_la_SOURCES    = src/mrcp_resource_engine.c
