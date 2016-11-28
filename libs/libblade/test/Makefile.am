AM_CFLAGS += -I$(abs_top_srcdir)/src/include -g -ggdb -O0 
TEST_LDADD = $(abs_top_builddir)/libblade.la
check_PROGRAMS =

check_PROGRAMS += testbuild
testbuild_SOURCES = testbuild.c tap.c
testbuild_CFLAGS = $(AM_CFLAGS)
testbuild_LDADD = $(TEST_LDADD)


TESTS=$(check_PROGRAMS)

tests: $(check_PROGRAMS)
