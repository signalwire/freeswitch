AM_CFLAGS += -I$(abs_top_srcdir)/src/include -g -ggdb -O0 $(openssl_CFLAGS)
TEST_LDADD = $(abs_top_builddir)/libks.la $(openssl_LIBS)
check_PROGRAMS = 

EXTRA_DIST = tap.h

check_PROGRAMS += testpools
testpools_SOURCES = testpools.c tap.c
testpools_CFLAGS = $(AM_CFLAGS)
testpools_LDADD = $(TEST_LDADD)

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

check_PROGRAMS += testdht
testdht_SOURCES = testdht.c tap.c
testdht_CFLAGS = $(AM_CFLAGS)
testdht_LDADD = $(TEST_LDADD)

check_PROGRAMS += testdht_net
testdht_net_SOURCES = testdht-net.c tap.c
testdht_net_CFLAGS = $(AM_CFLAGS)
testdht_net_LDADD = $(TEST_LDADD)

check_PROGRAMS += testdht_msg
testdht_msg_SOURCES = testdht-msg.c tap.c
testdht_msg_CFLAGS = $(AM_CFLAGS)
testdht_msg_LDADD = $(TEST_LDADD)

check_PROGRAMS += dht_example
dht_example_SOURCES = dht-example.c
dht_example_CFLAGS = $(AM_CFLAGS)
dht_example_LDADD = $(abs_top_builddir)/libks.la $(openssl_LIBS) -ledit -lpthread

#check_PROGRAMS += libtorrent_example
#libtorrent_example_SOURCES = libtorrent-example.c
#libtorrent_example_CFLAGS = $(AM_CFLAGS) 
#libtorrent_example_LDADD = $(abs_top_builddir)/libks.la $(abs_top_builddir)/test/libtorrent.so /usr/lib/x86_64-linux-gnu/libboost_system.a $(openssl_LIBS) -ledit -lpthread -ltorrent-rasterbar -lstdc++

TESTS=$(check_PROGRAMS)

tests: $(check_PROGRAMS)

$(abs_top_builddir)/test/libtorrent.so: $(abs_top_builddir)/test/libtorrent.o
	g++ -shared -o $(abs_top_builddir)/test/libtorrent.so $(abs_top_builddir)/test/libtorrent.o

$(abs_top_builddir)/test/libtorrent.o: $(abs_top_builddir)/test/libtorrent.cpp
	g++ -c -fPIC -o $(abs_top_builddir)/test/libtorrent.o -I$(abs_top_builddir)/test/ $(abs_top_builddir)/test/libtorrent.cpp

