MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/mrcp/include \
                           -I$(top_srcdir)/libs/mrcp/message/include \
                           -I$(top_srcdir)/libs/mrcp/control/include \
                           -I$(top_srcdir)/libs/mrcp/resources/include \
                           -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libmrcp.la

include_HEADERS          = include/mrcp.h \
                           include/mrcp_types.h \
                           message/include/mrcp_start_line.h \
                           message/include/mrcp_header_accessor.h \
                           message/include/mrcp_generic_header.h \
                           message/include/mrcp_message.h \
                           control/include/mrcp_resource.h \
                           control/include/mrcp_resource_factory.h \
                           control/include/mrcp_resource_loader.h \
                           control/include/mrcp_stream.h \
                           resources/include/mrcp_synth_header.h \
                           resources/include/mrcp_synth_resource.h \
                           resources/include/mrcp_recog_header.h \
                           resources/include/mrcp_recog_resource.h \
                           resources/include/mrcp_recorder_header.h \
                           resources/include/mrcp_recorder_resource.h

libmrcp_la_SOURCES       = message/src/mrcp_header_accessor.c \
                           message/src/mrcp_generic_header.c \
                           message/src/mrcp_start_line.c \
                           message/src/mrcp_message.c \
                           control/src/mrcp_resource_factory.c \
                           control/src/mrcp_resource_loader.c \
                           control/src/mrcp_stream.c \
                           resources/src/mrcp_synth_header.c \
                           resources/src/mrcp_synth_resource.c \
                           resources/src/mrcp_recog_header.c \
                           resources/src/mrcp_recog_resource.c \
                           resources/src/mrcp_recorder_header.c \
                           resources/src/mrcp_recorder_resource.c
