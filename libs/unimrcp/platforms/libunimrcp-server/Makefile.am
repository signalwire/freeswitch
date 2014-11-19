AM_CPPFLAGS                 = -I$(top_srcdir)/platforms/libunimrcp-server/include \
                              $(UNIMRCP_SERVERLIB_INCLUDES)

lib_LTLIBRARIES             = libunimrcpserver.la

include_HEADERS             = include/unimrcp_server.h
libunimrcpserver_la_SOURCES = src/unimrcp_server.c
libunimrcpserver_la_LIBADD  = $(UNIMRCP_SERVERLIB_LIBS)
libunimrcpserver_la_LDFLAGS = $(UNIMRCP_SERVERLIB_OPTS)

include $(top_srcdir)/build/rules/uniserverlib.am
