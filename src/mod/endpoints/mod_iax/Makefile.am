include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_iax

mod_LTLIBRARIES = mod_iax.la
mod_iax_la_SOURCES  = mod_iax.c iax2-parser.c iax.c md5.c jitterbuf.c iax-mutex.c
mod_iax_la_CFLAGS   = $(AM_CFLAGS) -I. -DNEWJB -DLIBIAX -DDEBUG_SUPPORT
mod_iax_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_iax_la_LDFLAGS  = -avoid-version -module -no-undefined
