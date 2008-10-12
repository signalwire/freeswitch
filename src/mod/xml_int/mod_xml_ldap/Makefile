switch_srcdir=../../../..

LDAP=openldap-2.4.11
LDAP_DIR=$(switch_srcdir)/libs/$(LDAP)

LDAPLA=$(LDAP_DIR)/libraries/libldap_r/libldap_r.la
LIBLBERLA=$(LDAP_DIR)/libraries/liblber/liblber.la
LIBLUTILA=$(LDAP_DIR)/libraries/liblutil/liblutil.a

LOCAL_CFLAGS=-DWITH_OPENLDAP -DLDAP_DEPRECATED -I$(LDAP_DIR)/include

DEFAULT_ARGS= --prefix=$(PREFIX) --disable-shared --with-pic --disable-slapd
DEFAULT_ARGS+= --disable-slurpd --disable-relay --disable-bdb --disable-hdb

LOCAL_LIBADD=$(LDAPLA) $(LIBLBERLA) $(LIBLUTILA)

include $(switch_srcdir)/build/modmake.rules

$(LDAP_DIR):
	$(GETLIB) $(LDAP).tgz
	cd $(LDAP_DIR) && CFLAGS="`echo $(CFLAGS) | sed -e 's:-std=c99::'`" ./configure $(DEFAULT_ARGS)

$(LIBLUTILA) $(LDAPLA) $(LIBLBERLA): $(LDAP_DIR)
	cd $(LDAP_DIR) && $(MAKE)
	$(TOUCH_TARGET)
