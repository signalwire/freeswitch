AUTOMAKE_OPTIONS = foreign subdir-objects

mod_LTLIBRARIES = mod_skel.la
mod_skel_la_SOURCES  = mod_skel.c
mod_skel_la_CFLAGS   = $(FREESWITCH_CFLAGS)
mod_skel_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(FREESWITCH_LDFLAGS)
