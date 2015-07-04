include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_kazoo

if HAVE_ERLANG

mod_LTLIBRARIES = mod_kazoo.la
mod_kazoo_la_SOURCES  = mod_kazoo.c kazoo_utils.c kazoo_node.c kazoo_event_stream.c kazoo_fetch_agent.c kazoo_commands.c kazoo_dptools.c
mod_kazoo_la_CFLAGS   = $(AM_CFLAGS) @ERLANG_CFLAGS@ -D_REENTRANT
mod_kazoo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_kazoo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared @ERLANG_LDFLAGS@

else
install: error
all: error
error:
	$(error You must install erlang to build this module)
endif
