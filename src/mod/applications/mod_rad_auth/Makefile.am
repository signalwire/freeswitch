include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_rad_auth

mod_LTLIBRARIES = mod_rad_auth.la
mod_rad_auth_la_SOURCES  = mod_rad_auth.c
mod_rad_auth_la_CFLAGS   = $(AM_CFLAGS)
mod_rad_auth_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_rad_auth_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
