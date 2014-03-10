include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_xml_scgi
mod_LTLIBRARIES = mod_xml_scgi.la
mod_xml_scgi_la_SOURCES  = mod_xml_scgi.c ../../../../libs/libscgi/src/scgi.c
mod_xml_scgi_la_CFLAGS   = $(AM_CFLAGS)  -I. -I$(switch_srcdir)/libs/libscgi/src/include -D_GNU_SOURCE
mod_xml_scgi_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_xml_scgi_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
