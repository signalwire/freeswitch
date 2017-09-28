AM_CFLAGS += -I$(abs_top_srcdir)/src/include -g -ggdb -O0 -I$(top_srcdir)/../civetweb-1.9.1/include
TEST_LDADD = $(abs_top_builddir)/libblade.la -L$(top_srcdir)/../civetweb-1.9.1 -lconfig -lm -lpthread -lcivetweb
check_PROGRAMS =


check_PROGRAMS += switchblade
switchblade_SOURCES = switchblade.c
switchblade_CFLAGS = $(AM_CFLAGS)
switchblade_LDADD = $(TEST_LDADD)

sb: $(check_PROGRAMS)
