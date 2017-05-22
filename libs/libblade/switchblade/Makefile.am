AM_CFLAGS += -I$(abs_top_srcdir)/src/include -g -ggdb -O0
TEST_LDADD = $(abs_top_builddir)/libblade.la -lconfig -lm -lpthread
check_PROGRAMS =


check_PROGRAMS += switchblade
switchblade_SOURCES = switchblade.c
switchblade_CFLAGS = $(AM_CFLAGS)
switchblade_LDADD = $(TEST_LDADD)

sb: $(check_PROGRAMS)
