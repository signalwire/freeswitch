include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_erlang_event
mod_LTLIBRARIES = mod_erlang_event.la
mod_erlang_event_la_SOURCES  = mod_erlang_event.c handle_msg.c ei_helpers.c
mod_erlang_event_la_CFLAGS   = $(AM_CFLAGS) @ERLANG_CFLAGS@ -D_REENTRANT
mod_erlang_event_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_erlang_event_la_LDFLAGS  = -avoid-version -module -no-undefined -shared @ERLANG_LDFLAGS@
