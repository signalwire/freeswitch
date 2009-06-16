MAINTAINERCLEANFILES = Makefile.in

INCLUDES             = -I$(top_srcdir)/libs/uni-rtsp/include \
                       -I$(top_srcdir)/libs/apr-toolkit/include \
                       $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_PROGRAMS      = rtsptest
rtsptest_LDADD       = $(top_builddir)/libs/uni-rtsp/libunirtsp.la \
                       $(top_builddir)/libs/apr-toolkit/libaprtoolkit.la \
                       $(UNIMRCP_APR_LIBS) $(UNIMRCP_APU_LIBS)
rtsptest_SOURCES     = src/main.c \
                       src/parse_gen_suite.c
