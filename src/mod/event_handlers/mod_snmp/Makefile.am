include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_snmp

mod_LTLIBRARIES = mod_snmp.la
mod_snmp_la_SOURCES  = mod_snmp.c subagent.c
mod_snmp_la_CFLAGS   = $(AM_CFLAGS)
mod_snmp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la -lnetsnmpmibs -lnetsnmpagent -lnetsnmp
mod_snmp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
