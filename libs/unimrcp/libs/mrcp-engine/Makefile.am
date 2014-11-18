MAINTAINERCLEANFILES        = Makefile.in

AM_CPPFLAGS                 = -I$(top_srcdir)/libs/mrcp-engine/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                              $(UNIMRCP_APR_INCLUDES)

noinst_LTLIBRARIES          = libmrcpengine.la

include_HEADERS             = include/mrcp_engine_types.h \
                              include/mrcp_engine_plugin.h \
                              include/mrcp_engine_iface.h \
                              include/mrcp_engine_impl.h \
                              include/mrcp_synth_engine.h \
                              include/mrcp_recog_engine.h \
                              include/mrcp_recorder_engine.h \
                              include/mrcp_verifier_engine.h \
                              include/mrcp_resource_engine.h \
                              include/mrcp_engine_factory.h \
                              include/mrcp_engine_loader.h \
                              include/mrcp_state_machine.h \
                              include/mrcp_synth_state_machine.h \
                              include/mrcp_recog_state_machine.h \
                              include/mrcp_recorder_state_machine.h \
                              include/mrcp_verifier_state_machine.h

libmrcpengine_la_SOURCES    = src/mrcp_engine_iface.c \
                              src/mrcp_engine_impl.c \
                              src/mrcp_engine_factory.c \
                              src/mrcp_engine_loader.c \
                              src/mrcp_synth_state_machine.c \
                              src/mrcp_recog_state_machine.c \
                              src/mrcp_recorder_state_machine.c \
                              src/mrcp_verifier_state_machine.c
