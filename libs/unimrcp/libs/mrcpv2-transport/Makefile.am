MAINTAINERCLEANFILES 	      = Makefile.in

INCLUDES                      = -I$(top_srcdir)/libs/mrcpv2-transport/include \
                                -I$(top_srcdir)/libs/mrcp/include \
                                -I$(top_srcdir)/libs/mrcp/message/include \
                                -I$(top_srcdir)/libs/mrcp/control/include \
                                -I$(top_srcdir)/libs/apr-toolkit/include \
                                $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES            = libmrcpv2transport.la

include_HEADERS               = include/mrcp_connection_types.h \
                                include/mrcp_control_descriptor.h \
                                include/mrcp_connection.h \
                                include/mrcp_client_connection.h \
                                include/mrcp_server_connection.h

libmrcpv2transport_la_SOURCES = src/mrcp_control_descriptor.c \
                                src/mrcp_connection.c \
                                src/mrcp_client_connection.c \
                                src/mrcp_server_connection.c
