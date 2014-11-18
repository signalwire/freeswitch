AM_CPPFLAGS                 = -I$(top_srcdir)/platforms/libunimrcp-client/include \
                              $(UNIMRCP_CLIENTLIB_INCLUDES)

lib_LTLIBRARIES             = libunimrcpclient.la

include_HEADERS             = include/unimrcp_client.h
libunimrcpclient_la_SOURCES = src/unimrcp_client.c
libunimrcpclient_la_LIBADD  = $(UNIMRCP_CLIENTLIB_LIBS)
libunimrcpclient_la_LDFLAGS = $(UNIMRCP_CLIENTLIB_OPTS)

include $(top_srcdir)/build/rules/uniclientlib.am
