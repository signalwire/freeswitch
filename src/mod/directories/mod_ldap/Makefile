BASE=../../../..

LDAP=openldap-2.4.19
LDAP_DIR=$(switch_srcdir)/libs/$(LDAP)
LDAP_BUILDDIR=$(switch_builddir)/libs/$(LDAP)

LDAPLA=$(LDAP_BUILDDIR)/libraries/libldap_r/libldap_r.la
LIBLBERLA=$(LDAP_BUILDDIR)/libraries/liblber/liblber.la


LOCAL_CFLAGS=-DWITH_OPENLDAP -DLDAP_DEPRECATED -I$(LDAP_DIR)/include

LOCAL_LIBADD=$(LDAPLA) $(LIBLBERLA)

include $(BASE)/build/modmake.rules

DEFAULT_ARGS+= --disable-slapd --disable-bdb --disable-hdb

$(LDAP_DIR):
	$(GETLIB) $(LDAP).tar.gz

$(LDAP_BUILDDIR)/Makefile: $(LDAP_DIR)
	mkdir -p $(LDAP_BUILDDIR)
	cd $(LDAP_BUILDDIR) && $(DEFAULT_VARS) $(LDAP_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(LDAP_DIR)
	$(TOUCH_TARGET)

$(LDAPLA) $(LIBLBERLA): $(LDAP_BUILDDIR)/Makefile
	cd $(LDAP_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)
