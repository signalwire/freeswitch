MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS          = -I$(top_srcdir)/libs/mrcp/include \
                       -I$(top_srcdir)/libs/mrcp/message/include \
                       -I$(top_srcdir)/libs/mrcp/control/include \
                       -I$(top_srcdir)/libs/mrcp/resources/include \
                       -I$(top_srcdir)/libs/apr-toolkit/include \
                       $(UNIMRCP_APR_INCLUDES)

noinst_PROGRAMS      = mrcptest
mrcptest_LDADD       = $(top_builddir)/libs/mrcp/libmrcp.la \
                       $(top_builddir)/libs/apr-toolkit/libaprtoolkit.la \
                       $(UNIMRCP_APR_LIBS)
mrcptest_SOURCES     = src/main.c \
                       src/parse_gen_suite.c \
                       src/set_get_suite.c \
                       src/transparent_set_get_suite.c
