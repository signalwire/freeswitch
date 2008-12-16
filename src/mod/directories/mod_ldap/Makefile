switch_srcdir=../../../..

LDAP=openldap-2.4.11
LDAP_DIR=$(switch_srcdir)/libs/$(LDAP)

LDAPLA=$(LDAP_DIR)/libraries/libldap_r/libldap_r.la
LIBLBERLA=$(LDAP_DIR)/libraries/liblber/liblber.la

LOCAL_CFLAGS=-DWITH_OPENLDAP -DLDAP_DEPRECATED -I$(LDAP_DIR)/include

DEFAULT_ARGS=--prefix=$(PREFIX) --disable-shared --with-pic

LOCAL_LIBADD=$(LDAPLA) $(LIBLBERLA)

include $(switch_srcdir)/build/modmake.rules

$(LDAP_DIR):
	$(GETLIB) $(LDAP).tar.gz
	cd $(LDAP_DIR) && ./configure $(DEFAULT_ARGS) --disable-bdb --disable-hdb --disable-slapd

$(LDAPLA) $(LIBLBERLA): $(LDAP_DIR)
	cd $(LDAP_DIR) && $(MAKE)
	$(TOUCH_TARGET)
