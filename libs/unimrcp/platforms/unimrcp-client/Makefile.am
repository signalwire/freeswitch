MAINTAINERCLEANFILES   = Makefile.in

INCLUDES               = -Iinclude \
                         -I$(top_srcdir)/platforms/libunimrcp-client/include \
                         -I$(top_srcdir)/libs/mrcp-client/include \
                         -I$(top_srcdir)/libs/mrcp-signaling/include \
                         -I$(top_srcdir)/libs/mrcpv2-transport/include \
                         -I$(top_srcdir)/libs/mrcp/include \
                         -I$(top_srcdir)/libs/mrcp/message/include \
                         -I$(top_srcdir)/libs/mrcp/control/include \
                         -I$(top_srcdir)/libs/mrcp/resources/include \
                         -I$(top_srcdir)/libs/mpf/include \
                         -I$(top_srcdir)/libs/apr-toolkit/include \
                         $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

bin_PROGRAMS           = unimrcpclient
unimrcpclient_SOURCES  = src/main.c \
                         src/demo_framework.c \
                         src/demo_synth_application.c \
                         src/demo_recog_application.c \
                         src/demo_bypass_application.c \
                         src/demo_discover_application.c \
                         src/demo_util.c
unimrcpclient_LDADD    = $(top_builddir)/platforms/libunimrcp-client/libunimrcpclient.la

if ISMAC
unimrcpclient_LDFLAGS  = -framework CoreFoundation -framework SystemConfiguration
endif
