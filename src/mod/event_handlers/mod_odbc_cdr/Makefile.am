include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_odbc_cdr

mod_LTLIBRARIES = mod_odbc_cdr.la
mod_odbc_cdr_la_SOURCES  = mod_odbc_cdr.c
mod_odbc_cdr_la_CFLAGS   = $(AM_CFLAGS)
mod_odbc_cdr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_odbc_cdr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

