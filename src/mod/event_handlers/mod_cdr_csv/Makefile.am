include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_cdr_csv

mod_LTLIBRARIES = mod_cdr_csv.la
mod_cdr_csv_la_SOURCES  = mod_cdr_csv.c
mod_cdr_csv_la_CFLAGS   = $(AM_CFLAGS)
mod_cdr_csv_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_cdr_csv_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
