switch_srcdir=../../../..

LDAP=openldap-2.4.11
LDAP_DIR=$(switch_srcdir)/libs/$(LDAP)

LDAPLA=$(LDAP_DIR)/libraries/libldap_r/libldap_r.la
LIBLBERLA=$(LDAP_DIR)/libraries/liblber/liblber.la

LOCAL_CFLAGS=-DWITH_OPENLDAP -DLDAP_DEPRECATED -I$(LDAP_DIR)/include

LOCAL_LIBADD=$(LDAPLA) $(LIBLBERLA)

include $(switch_srcdir)/build/modmake.rules

DEFAULT_ARGS+= --disable-slapd --disable-bdb --disable-hdb

$(LDAP_DIR):
	$(GETLIB) $(LDAP).tar.gz
	cd $(LDAP_DIR) && $(DEFAULT_VARS) CFLAGS="`echo $(CFLAGS) | sed -e 's:-std=c99::'`" ./configure $(DEFAULT_ARGS)

$(LDAPLA) $(LIBLBERLA): $(LDAP_DIR)
	cd $(LDAP_DIR) && $(MAKE)
	$(TOUCH_TARGET)
