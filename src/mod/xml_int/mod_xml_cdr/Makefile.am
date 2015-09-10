include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_xml_cdr

mod_LTLIBRARIES = mod_xml_cdr.la
mod_xml_cdr_la_SOURCES  = mod_xml_cdr.c
mod_xml_cdr_la_CFLAGS   = $(CURL_CFLAGS) $(AM_CFLAGS)
mod_xml_cdr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_xml_cdr_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared
