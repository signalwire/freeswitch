MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS          = -I$(top_srcdir)/libs/apr-toolkit/include \
                       $(UNIMRCP_APR_INCLUDES)

noinst_PROGRAMS      = strtablegen
strtablegen_LDADD    = $(top_builddir)/libs/apr-toolkit/libaprtoolkit.la \
                       $(UNIMRCP_APR_LIBS)
strtablegen_SOURCES  = src/main.c
