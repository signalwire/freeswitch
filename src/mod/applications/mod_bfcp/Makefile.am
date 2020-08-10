include $(top_srcdir)/build/modmake.rulesam
MODNAME = mod_bfcp

BFCP_CFLAGS = -I$(switch_srcdir)/libs/libbfcp/bfcpmsg -I$(switch_srcdir)/libs/libbfcp/bfcpsrvclt/bfcpsrv -I$(switch_srcdir)/libs/libbfcp/bfcpsrvclt/bfcpclt -I$(switch_srcdir)/src/mod/applications/mod_bfcp -I$(switch_srcdir)/src/include  -I$(switch_srcdir)/libs/libbfcp -lbfcpmsg -I$(switch_srcdir)/libs/libbfcp -lbfcpsrv -I$(switch_srcdir)/libs/libbfcp -lbfcpclt
BFCP_LDFLAGS = -L$(switch_srcdir)/libs/libbfcp/bfcpmsg -L$(switch_srcdir)/libs/libbfcp/bfcpsrvclt/bfcpsrv -L$(switch_srcdir)/libs/libbfcp/bfcpsrvclt/bfcpclt

mod_LTLIBRARIES = mod_bfcp.la

mod_bfcp_la_SOURCES  = mod_bfcp.c mod_bfcp.h bfcp_interface.c bfcp_interface.h bfcp_media.c bfcp_thread.h
mod_bfcp_la_CFLAGS   = $(AM_CFLAGS) $(BFCP_CFLAGS)
mod_bfcp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(BFCP_LDFLAGS)
mod_bfcp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -bfcprel



