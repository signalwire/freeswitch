AM_CFLAGS += -I$(abs_top_srcdir)/src/include -g -ggdb -O0 -I$(top_srcdir)/../civetweb-1.9.1/include -I$(top_srcdir)/../libks/src/include
TEST_LDADD = $(abs_top_builddir)/libblade.la -L$(top_srcdir)/../civetweb-1.9.1 -L$(top_srcdir)/../libks -lconfig -lm -lpthread -lcivetweb -lks
check_PROGRAMS =


check_PROGRAMS += switchblade
switchblade_SOURCES = switchblade.c
switchblade_CFLAGS = $(AM_CFLAGS)
switchblade_LDADD = $(TEST_LDADD)

sb: $(check_PROGRAMS)
