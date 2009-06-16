MAINTAINERCLEANFILES 	    = Makefile.in

INCLUDES                    = -I$(top_srcdir)/libs/mrcp-server/include \
                              -I$(top_srcdir)/libs/mrcp-engine/include \
                              -I$(top_srcdir)/libs/mrcp-signaling/include \
                              -I$(top_srcdir)/libs/mrcpv2-transport/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                              $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES          = libmrcpserver.la

include_HEADERS             = include/mrcp_server_types.h \
                              include/mrcp_server.h \
                              include/mrcp_server_session.h

libmrcpserver_la_SOURCES    = src/mrcp_server.c \
                              src/mrcp_server_session.c
