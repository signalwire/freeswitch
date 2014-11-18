MAINTAINERCLEANFILES        = Makefile.in

AM_CPPFLAGS                 = -I$(top_srcdir)/libs/mrcp-client/include \
                              -I$(top_srcdir)/libs/mrcp-signaling/include \
                              -I$(top_srcdir)/libs/mrcpv2-transport/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                              $(UNIMRCP_APR_INCLUDES)

noinst_LTLIBRARIES          = libmrcpclient.la

include_HEADERS             = include/mrcp_client_types.h \
                              include/mrcp_client.h \
                              include/mrcp_client_session.h \
                              include/mrcp_application.h

libmrcpclient_la_SOURCES    = src/mrcp_client.c \
                              src/mrcp_client_session.c \
                              src/mrcp_application.c
