FSLD = $(top_builddir)/libfreeswitch.la $(top_builddir)/libs/apr/libapr-1.la $(top_builddir)/libs/apr-util/libaprutil-1.la

TESTS = 
check_PROGRAMS = 


if HAVE_TAP
TESTS += switch_event
check_PROGRAMS += switch_event

switch_event_SOURCES = switch_event.c
switch_event_CFLAGS = $(SWITCH_AM_CFLAGS)
switch_event_LDADD = $(FSLD)
switch_event_LDFLAGS = $(SWITCH_AM_LDFLAGS) -ltap

TESTS += switch_hash
check_PROGRAMS += switch_hash

switch_hash_SOURCES = switch_hash.c
switch_hash_CFLAGS = $(SWITCH_AM_CFLAGS)
switch_hash_LDADD = $(FSLD)
switch_hash_LDFLAGS = $(SWITCH_AM_LDFLAGS) -ltap

else
check:
error:
	$(error You must install libtap-dev to build these unit tests)
endif

