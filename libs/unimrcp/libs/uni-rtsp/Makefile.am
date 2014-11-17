MAINTAINERCLEANFILES     = Makefile.in

AM_CPPFLAGS              = -I$(top_srcdir)/libs/uni-rtsp/include \
                           -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES)

noinst_LTLIBRARIES       = libunirtsp.la

include_HEADERS          = include/rtsp.h \
                           include/rtsp_header.h \
                           include/rtsp_start_line.h \
                           include/rtsp_message.h \
                           include/rtsp_stream.h \
                           include/rtsp_server.h \
                           include/rtsp_client.h

libunirtsp_la_SOURCES    = src/rtsp_header.c \
                           src/rtsp_start_line.c \
                           src/rtsp_message.c \
                           src/rtsp_stream.c \
                           src/rtsp_server.c \
                           src/rtsp_client.c
