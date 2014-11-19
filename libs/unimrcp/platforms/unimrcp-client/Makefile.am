AM_CPPFLAGS            = -I$(top_srcdir)/platforms/unimrcp-client/include \
                         $(UNIMRCP_CLIENTAPP_INCLUDES)

bin_PROGRAMS           = unimrcpclient

unimrcpclient_SOURCES  = src/main.c \
                         src/demo_framework.c \
                         src/demo_synth_application.c \
                         src/demo_recog_application.c \
                         src/demo_bypass_application.c \
                         src/demo_discover_application.c \
                         src/demo_util.c
unimrcpclient_LDADD    = $(UNIMRCP_CLIENTAPP_LIBS)
unimrcpclient_LDFLAGS  = $(UNIMRCP_CLIENTAPP_OPTS)

include $(top_srcdir)/build/rules/uniclientapp.am
