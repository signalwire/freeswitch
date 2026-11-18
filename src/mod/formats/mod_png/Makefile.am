include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_png

mod_LTLIBRARIES = mod_png.la
mod_png_la_SOURCES  = mod_png.c
mod_png_la_CFLAGS   = $(AM_CFLAGS)
mod_png_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_png_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
