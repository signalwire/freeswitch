AUTOMAKE_OPTIONS = foreign

noinst_PROGRAMS = test_recv_event
TESTS = $(noinst_PROGRAMS)

test_recv_event_SOURCES = test_recv_event.c
test_recv_event_CFLAGS  = $(AM_CFLAGS) -I$(switch_srcdir)/libs/esl/src/include
test_recv_event_LDADD   = $(top_builddir)/libs/esl/libesl.la
test_recv_event_LDFLAGS = $(AM_LDFLAGS) -lpthread -lm
