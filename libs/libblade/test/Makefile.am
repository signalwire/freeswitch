AM_CFLAGS += -I$(abs_top_srcdir)/src/include -g -ggdb -O0 -I$(top_srcdir)/../civetweb-1.9.1/include
TEST_LDADD = $(abs_top_builddir)/libblade.la  -L$(top_srcdir)/../civetweb-1.9.1 -lconfig -lm -lpthread -lcivetweb
check_PROGRAMS =


check_PROGRAMS += testbuild
testbuild_SOURCES = testbuild.c tap.c
testbuild_CFLAGS = $(AM_CFLAGS)
testbuild_LDADD = $(TEST_LDADD)

check_PROGRAMS += bladec
bladec_SOURCES = bladec.c tap.c
bladec_CFLAGS = $(AM_CFLAGS)
bladec_LDADD = $(TEST_LDADD)

check_PROGRAMS += blades
blades_SOURCES = blades.c tap.c
blades_CFLAGS = $(AM_CFLAGS)
blades_LDADD = $(TEST_LDADD)

check_PROGRAMS += testcli
testcli_SOURCES = testcli.c tap.c
testcli_CFLAGS = $(AM_CFLAGS)
testcli_LDADD = $(TEST_LDADD)

check_PROGRAMS += testcon
testcon_SOURCES = testcon.c tap.c
testcon_CFLAGS = $(AM_CFLAGS)
testcon_LDADD = $(TEST_LDADD)

#check_PROGRAMS += testdht2
#testdht2_SOURCES = testdht2.c tap.c
#testdht2_CFLAGS = $(AM_CFLAGS)
#testdht2_LDADD = $(TEST_LDADD)

#check_PROGRAMS += testbuckets
#testbuckets_SOURCES = testbuckets.c tap.c
#testbuckets_CFLAGS = $(AM_CFLAGS)
#testbuckets_LDADD = $(TEST_LDADD)

#check_PROGRAMS += nodeidgen
#nodeidgen_SOURCES = nodeidgen.c tap.c
#nodeidgen_CFLAGS = $(AM_CFLAGS)
#nodeidgen_LDADD = $(TEST_LDADD)


TESTS=$(check_PROGRAMS)

tests: $(check_PROGRAMS)
