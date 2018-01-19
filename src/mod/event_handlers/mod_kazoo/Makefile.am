include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_kazoo

if HAVE_ERLANG

mod_LTLIBRARIES = mod_kazoo.la
mod_kazoo_la_SOURCES  = mod_kazoo.c kazoo_utils.c kazoo_dptools.c kazoo_tweaks.c
mod_kazoo_la_SOURCES  += kazoo_api.c kazoo_commands.c kazoo_config.c
mod_kazoo_la_SOURCES  += kazoo_message.c
mod_kazoo_la_SOURCES  += kazoo_ei_config.c kazoo_ei_utils.c kazoo_event_stream.c
mod_kazoo_la_SOURCES  += kazoo_fetch_agent.c kazoo_node.c

mod_kazoo_la_CFLAGS   = $(AM_CFLAGS) @ERLANG_CFLAGS@ -D_REENTRANT
mod_kazoo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_kazoo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared @ERLANG_LDFLAGS@

mod_kazoo.la: kazoo_definitions.h $(mod_kazoo_la_OBJECTS) $(mod_kazoo_la_DEPENDENCIES) $(EXTRA_mod_kazoo_la_DEPENDENCIES)
	$(AM_V_CCLD)$(mod_kazoo_la_LINK) $(am_mod_kazoo_la_rpath) $(mod_kazoo_la_OBJECTS) $(mod_kazoo_la_LIBADD) $(LIBS)


kazoo_definitions.h:
	xxd -i kazoo.conf.xml > kazoo_definitions.h

else
install: error
all: error
error:
	$(error You must install erlang to build this module)
endif
