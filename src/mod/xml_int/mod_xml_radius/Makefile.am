include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_xml_radius

mod_LTLIBRARIES = mod_xml_radius.la
mod_xml_radius_la_SOURCES  = mod_xml_radius.c
mod_xml_radius_la_CFLAGS   = $(AM_CFLAGS)
mod_xml_radius_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_xml_radius_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
