include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_commands

mod_LTLIBRARIES = mod_commands.la
mod_commands_la_SOURCES  = mod_commands.c
mod_commands_la_CFLAGS   = $(AM_CFLAGS)
mod_commands_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_commands_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

noinst_LTLIBRARIES = libmodcommands.la
libmodcommands_la_SOURCES = $(mod_commands_la_SOURCES)
libmodcommands_la_CFLAGS = $(mod_commands_la_CFLAGS)

noinst_PROGRAMS = test/test_mod_commands
test_test_mod_commands_CFLAGS = $(SWITCH_AM_CFLAGS) -I../ -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_mod_commands_LDFLAGS = -avoid-version -no-undefined $(SWITCH_AM_LDFLAGS)
test_test_mod_commands_LDADD = libmodcommands.la $(switch_builddir)/libfreeswitch.la

TESTS = $(noinst_PROGRAMS)
