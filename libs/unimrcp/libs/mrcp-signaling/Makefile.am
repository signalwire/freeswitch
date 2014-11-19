MAINTAINERCLEANFILES        = Makefile.in

AM_CPPFLAGS                 = -I$(top_srcdir)/libs/mrcp-signaling/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                              $(UNIMRCP_APR_INCLUDES)

noinst_LTLIBRARIES          = libmrcpsignaling.la

include_HEADERS             = include/mrcp_sig_types.h \
                              include/mrcp_sig_agent.h \
                              include/mrcp_session.h \
                              include/mrcp_session_descriptor.h

libmrcpsignaling_la_SOURCES = src/mrcp_sig_agent.c \
                              src/mrcp_session_descriptor.c