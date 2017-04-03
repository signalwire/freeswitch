AM_CFLAGS += -I$(abs_top_srcdir)/src/include -g -ggdb -O0 $(openssl_CFLAGS)
TEST_LDADD = $(abs_top_builddir)/libks.la $(openssl_LIBS)
check_PROGRAMS =

EXTRA_DIST = tap.h

check_PROGRAMS += testpools
testpools_SOURCES = testpools.c tap.c
testpools_CFLAGS = $(AM_CFLAGS)
testpools_LDADD = $(TEST_LDADD)

check_PROGRAMS += testrealloc
testrealloc_SOURCES = testrealloc.c tap.c
testrealloc_CFLAGS = $(AM_CFLAGS)
testrealloc_LDADD = $(TEST_LDADD)

check_PROGRAMS += testacl
testacl_SOURCES = testacl.c tap.c
testacl_CFLAGS = $(AM_CFLAGS)
testacl_LDADD = $(TEST_LDADD)

check_PROGRAMS += test_thread_pools
test_thread_pools_SOURCES = test_thread_pools.c tap.c
test_thread_pools_CFLAGS = $(AM_CFLAGS)
test_thread_pools_LDADD = $(TEST_LDADD)

check_PROGRAMS += testthreadmutex
testthreadmutex_SOURCES = testthreadmutex.c tap.c
testthreadmutex_CFLAGS = $(AM_CFLAGS)
testthreadmutex_LDADD = $(TEST_LDADD)

check_PROGRAMS += testtime
testtime_SOURCES = testtime.c tap.c
testtime_CFLAGS = $(AM_CFLAGS)
testtime_LDADD = $(TEST_LDADD)

check_PROGRAMS += testq
testq_SOURCES = testq.c tap.c
testq_CFLAGS = $(AM_CFLAGS)
testq_LDADD = $(TEST_LDADD)

check_PROGRAMS += testhash
testhash_SOURCES = testhash.c tap.c
testhash_CFLAGS = $(AM_CFLAGS)
testhash_LDADD = $(TEST_LDADD)

check_PROGRAMS += testsock
testsock_SOURCES = testsock.c tap.c
testsock_CFLAGS = $(AM_CFLAGS)
testsock_LDADD = $(TEST_LDADD)

check_PROGRAMS += testsock2
testsock2_SOURCES = testsock2.c tap.c
testsock2_CFLAGS = $(AM_CFLAGS)
testsock2_LDADD = $(TEST_LDADD)

check_PROGRAMS += testwebsock
testwebsock_SOURCES = testwebsock.c tap.c
testwebsock_CFLAGS = $(AM_CFLAGS)
testwebsock_LDADD = $(TEST_LDADD)


TESTS=$(check_PROGRAMS)

tests: $(check_PROGRAMS)
