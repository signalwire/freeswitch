include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_skypiax
mod_LTLIBRARIES = mod_skypiax.la
mod_skypiax_la_SOURCES  = mod_skypiax.c skypiax_protocol.c
mod_skypiax_la_CFLAGS   = $(AM_CFLAGS) -DSKYPIAX_SVN_VERSION=\"`cat $(switch_builddir)/.version`\" -I../../../../libs/spandsp/src -I../../../..//libs/tiff-3.8.2/libtiff
mod_skypiax_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_skypiax_la_LDFLAGS  = -L../../../../libs/spandsp/src -avoid-version -module -no-undefined -shared -lX11 -lspandsp
