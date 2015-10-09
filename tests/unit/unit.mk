AUTOMAKE_OPTIONS = foreign
FSLD = $(top_builddir)/libfreeswitch.la $(top_builddir)/libs/apr/libapr-1.la $(top_builddir)/libs/apr-util/libaprutil-1.la

check_PROGRAMS += tests/unit/switch_event

tests_unit_switch_event_SOURCES = tests/unit/switch_event.c
tests_unit_switch_event_CFLAGS = $(SWITCH_AM_CFLAGS)
tests_unit_switch_event_LDADD = $(FSLD)
tests_unit_switch_event_LDFLAGS = $(SWITCH_AM_LDFLAGS) -ltap

check_PROGRAMS += tests/unit/switch_hash

tests_unit_switch_hash_SOURCES = tests/unit/switch_hash.c
tests_unit_switch_hash_CFLAGS = $(SWITCH_AM_CFLAGS)
tests_unit_switch_hash_LDADD = $(FSLD)
tests_unit_switch_hash_LDFLAGS = $(SWITCH_AM_LDFLAGS) -ltap

