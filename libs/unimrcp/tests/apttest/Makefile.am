MAINTAINERCLEANFILES = Makefile.in

INCLUDES             = -I$(top_srcdir)/libs/apr-toolkit/include \
                       $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_PROGRAMS      = apttest
apttest_LDADD        = $(top_builddir)/libs/apr-toolkit/libaprtoolkit.la \
                       $(UNIMRCP_APR_LIBS) $(UNIMRCP_APU_LIBS)
apttest_SOURCES      = src/main.c \
                       src/task_suite.c \
                       src/consumer_task_suite.c
