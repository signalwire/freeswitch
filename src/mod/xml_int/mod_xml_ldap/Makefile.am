include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_xml_ldap

LDAP=openldap-2.4.19
LDAP_DIR=$(switch_srcdir)/libs/$(LDAP)
LDAP_BUILDDIR=$(switch_builddir)/libs/$(LDAP)

LDAPLA=$(LDAP_BUILDDIR)/libraries/libldap_r/libldap_r.la
LIBLBERLA=$(LDAP_BUILDDIR)/libraries/liblber/liblber.la
LIBLUTILA=$(LDAP_BUILDDIR)/libraries/liblutil/liblutil.a

mod_LTLIBRARIES = mod_xml_ldap.la
mod_xml_ldap_la_SOURCES  = mod_xml_ldap.c
mod_xml_ldap_la_CFLAGS   = $(AM_CFLAGS) -DWITH_OPENLDAP -DLDAP_DEPRECATED -I$(LDAP_DIR)/include
mod_xml_ldap_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(LDAPLA) $(LIBLBERLA) $(LIBLUTILA)
mod_xml_ldap_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(LDAPLA) $(LIBLBERLA) $(LIBLUTILA)

MY_DEFAULT_ARGS= --build=$(build) --host=$(host) --target=$(target) --prefix="$(prefix)" --exec_prefix="$(exec_prefix)" --libdir="$(libdir)" --disable-slapd --disable-slurpd --disable-relay --disable-bdb --disable-hdb

$(LDAP_DIR):
	$(GETLIB) $(LDAP).tar.gz

$(LDAP_BUILDDIR)/Makefile: $(LDAP_DIR)
	mkdir -p $(LDAP_BUILDDIR)
	cd $(LDAP_BUILDDIR) && $(MY_DEFAULT_VARS) $(LDAP_DIR)/configure $(MY_DEFAULT_ARGS) --srcdir=$(LDAP_DIR)
	$(TOUCH_TARGET)

$(LDAPLA) $(LIBLBERLA): $(LDAP_BUILDDIR)/Makefile
	cd $(LDAP_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

