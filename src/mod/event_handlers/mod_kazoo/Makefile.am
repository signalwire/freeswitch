include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_kazoo

if HAVE_ERLANG

KAZOO_DEFS=kazoo_definitions.o

mod_LTLIBRARIES = mod_kazoo.la
mod_kazoo_la_SOURCES  = mod_kazoo.c kazoo_utils.c kazoo_dptools.c kazoo_tweaks.c
mod_kazoo_la_SOURCES  += kazoo_api.c kazoo_commands.c kazoo_config.c
mod_kazoo_la_SOURCES  += kazoo_message.c
mod_kazoo_la_SOURCES  += kazoo_ei_config.c kazoo_ei_utils.c kazoo_event_stream.c
mod_kazoo_la_SOURCES  += kazoo_fetch_agent.c kazoo_node.c
mod_kazoo_la_SOURCES  += kazoo_endpoints.c

mod_kazoo_la_CFLAGS   = $(AM_CFLAGS) @ERLANG_CFLAGS@ -D_REENTRANT -DERLANG_VERSION=@ERLANG_VERSION@ -DERLANG_MAJOR=@ERLANG_MAJOR@ -DERLANG_MINOR=@ERLANG_MINOR@
mod_kazoo_la_LIBADD   = $(KAZOO_DEFS) $(switch_builddir)/libfreeswitch.la
mod_kazoo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared @ERLANG_LDFLAGS@

BUILT_SOURCES = $(KAZOO_DEFS)

$(KAZOO_DEFS): kazoo.conf.xml

.S.o: $<
	@$(CC) $(CFLAGS) -o $@ -c $<

install-exec-am:
	@install `which epmd` $(DESTDIR)$(bindir)/fs_epmd

else
install: error
all: error
error:
	$(error You must install erlang to build this module)
endif
