FSLD = $(top_builddir)/libfreeswitch.la $(top_builddir)/libs/apr/libapr-1.la $(top_builddir)/libs/apr-util/libaprutil-1.la


if HAVE_TAP

TESTS = switch_event
check_PROGRAMS = switch_event

switch_event_SOURCES = switch_event.c
switch_event_CFLAGS = $(SWITCH_AM_CFLAGS)
switch_event_LDADD = $(FSLD)
switch_event_LDFLAGS = $(SWITCH_AM_LDFLAGS) -ltap

event_create_SOURCES = event_create.c
event_create_CFLAGS = $(SWITCH_AM_CFLAGS)
event_create_LDADD = $(FSLD)
event_create_LDFLAGS = $(SWITCH_AM_LDFLAGS) -ltap


else
check:
error:
	$(error You must install libtap-dev to build these unit tests)
endif

