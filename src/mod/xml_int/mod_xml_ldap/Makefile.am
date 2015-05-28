include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_xml_ldap

if HAVE_LDAP

mod_LTLIBRARIES = mod_xml_ldap.la
mod_xml_ldap_la_SOURCES  = mod_xml_ldap.c
mod_xml_ldap_la_CFLAGS   = $(AM_CFLAGS) -DWITH_OPENLDAP -DLDAP_DEPRECATED
mod_xml_ldap_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(OPENLDAP_LIBS)
mod_xml_ldap_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libldap2-dev or similar package to build mod_xml_ldap)
endif
