MAINTAINERCLEANFILES   = Makefile.in

INCLUDES               = -Iinclude \
                         -I$(top_srcdir)/platforms/libunimrcp-server/include \
                         -I$(top_srcdir)/libs/mrcp-server/include \
                         -I$(top_srcdir)/libs/mrcp-engine/include \
                         -I$(top_srcdir)/libs/mrcp-signaling/include \
                         -I$(top_srcdir)/libs/mrcpv2-transport/include \
                         -I$(top_srcdir)/libs/mrcp/include \
                         -I$(top_srcdir)/libs/mrcp/message/include \
                         -I$(top_srcdir)/libs/mrcp/control/include \
                         -I$(top_srcdir)/libs/mrcp/resources/include \
                         -I$(top_srcdir)/libs/mpf/include \
                         -I$(top_srcdir)/libs/apr-toolkit/include \
                         $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

bin_PROGRAMS           = unimrcpserver
unimrcpserver_SOURCES  = src/main.c src/uni_cmdline.c src/uni_daemon.c
unimrcpserver_LDADD    = $(top_builddir)/platforms/libunimrcp-server/libunimrcpserver.la

if ISMAC
unimrcpserver_LDFLAGS  = -framework CoreFoundation -framework SystemConfiguration
endif
