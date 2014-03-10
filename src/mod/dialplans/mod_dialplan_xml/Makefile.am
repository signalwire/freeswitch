include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_dialplan_xml

mod_LTLIBRARIES = mod_dialplan_xml.la
mod_dialplan_xml_la_SOURCES  = mod_dialplan_xml.c
mod_dialplan_xml_la_CFLAGS   = $(AM_CFLAGS)
mod_dialplan_xml_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_dialplan_xml_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
