AM_CPPFLAGS            = -I$(top_srcdir)/platforms/unimrcp-server/include \
                         $(UNIMRCP_SERVERAPP_INCLUDES)

bin_PROGRAMS           = unimrcpserver
unimrcpserver_SOURCES  = src/main.c src/uni_cmdline.c src/uni_daemon.c
unimrcpserver_LDADD    = $(UNIMRCP_SERVERAPP_LIBS)
unimrcpserver_LDFLAGS  = $(UNIMRCP_SERVERAPP_OPTS)

include $(top_srcdir)/build/rules/uniserverapp.am
