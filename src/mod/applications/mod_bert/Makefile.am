include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_bert

mod_LTLIBRARIES = mod_bert.la
mod_bert_la_SOURCES  = mod_bert.c
mod_bert_la_CFLAGS   = $(AM_CFLAGS)
mod_bert_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_bert_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
