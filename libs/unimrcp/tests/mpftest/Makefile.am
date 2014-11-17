MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS          = -I$(top_srcdir)/libs/mpf/include \
                       -I$(top_srcdir)/libs/apr-toolkit/include \
                       $(UNIMRCP_APR_INCLUDES)

noinst_PROGRAMS      = mpftest
mpftest_LDADD        = $(top_builddir)/libs/mpf/libmpf.la \
                       $(top_builddir)/libs/apr-toolkit/libaprtoolkit.la \
                       $(UNIMRCP_APR_LIBS)
mpftest_SOURCES      = src/main.c \
                       src/mpf_suite.c
