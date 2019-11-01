include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_radius_cdr

mod_LTLIBRARIES = mod_radius_cdr.la
mod_radius_cdr_la_SOURCES  = mod_radius_cdr.c
mod_radius_cdr_la_CFLAGS   = $(AM_CFLAGS) -I$(RADCLIENT_DIR)/include
mod_radius_cdr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_radius_cdr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
