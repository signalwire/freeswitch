MAINTAINERCLEANFILES       = Makefile.in

AM_CPPFLAGS                = -I$(top_srcdir)/modules/mrcp-sofiasip/include \
                             -I$(top_srcdir)/libs/mrcpv2-transport/include \
                             -I$(top_srcdir)/libs/mrcp-signaling/include \
                             -I$(top_srcdir)/libs/mrcp/include \
                             -I$(top_srcdir)/libs/mrcp/message/include \
                             -I$(top_srcdir)/libs/mrcp/control/include \
                             -I$(top_srcdir)/libs/mrcp/resources/include \
                             -I$(top_srcdir)/libs/mpf/include \
                             -I$(top_srcdir)/libs/apr-toolkit/include \
                             $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_SOFIA_INCLUDES)

noinst_LTLIBRARIES         = libmrcpsofiasip.la

include_HEADERS            = include/mrcp_sdp.h \
                             include/mrcp_sofiasip_task.h \
                             include/mrcp_sofiasip_logger.h \
                             include/mrcp_sofiasip_server_agent.h \
                             include/mrcp_sofiasip_client_agent.h
libmrcpsofiasip_la_SOURCES = src/mrcp_sdp.c \
                             src/mrcp_sofiasip_task.c \
                             src/mrcp_sofiasip_server_agent.c \
                             src/mrcp_sofiasip_client_agent.c \
                             src/mrcp_sofiasip_logger.c
