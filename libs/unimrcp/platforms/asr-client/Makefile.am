AM_CPPFLAGS            = -I$(top_srcdir)/platforms/libasr-client/include \
                         $(UNIMRCP_CLIENTAPP_INCLUDES)

bin_PROGRAMS           = asrclient

asrclient_SOURCES      = src/main.c
asrclient_LDADD        = $(top_builddir)/platforms/libasr-client/libasrclient.la
asrclient_LDFLAGS      = $(UNIMRCP_CLIENTAPP_OPTS)

include $(top_srcdir)/build/rules/uniclientapp.am
