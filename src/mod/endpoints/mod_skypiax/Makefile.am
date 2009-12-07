include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_skypiax
mod_LTLIBRARIES = mod_skypiax.la
mod_skypiax_la_SOURCES  = mod_skypiax.c skypiax_protocol.c
mod_skypiax_la_CFLAGS   = $(AM_CFLAGS) -DSKYPIAX_SVN_VERSION=\"`cat $(switch_builddir)/.version`\"
mod_skypiax_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_skypiax_la_LDFLAGS  = -avoid-version -module -no-undefined -lX11
