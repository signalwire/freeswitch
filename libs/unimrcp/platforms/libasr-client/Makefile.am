AM_CPPFLAGS                 = -I$(top_srcdir)/platforms/libasr-client/include \
                              -I$(top_srcdir)/platforms/libunimrcp-client/include \
                              $(UNIMRCP_CLIENTLIB_INCLUDES)

lib_LTLIBRARIES             = libasrclient.la

include_HEADERS             = include/asr_engine.h
libasrclient_la_SOURCES     = src/asr_engine.c
libasrclient_la_LIBADD      = $(top_builddir)/platforms/libunimrcp-client/libunimrcpclient.la
libasrclient_la_LDFLAGS     = $(UNIMRCP_CLIENTLIB_OPTS)

include $(top_srcdir)/build/rules/uniclientlib.am
