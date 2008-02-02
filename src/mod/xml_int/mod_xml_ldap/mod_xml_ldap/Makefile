WANT_LDAP=yes
LDAP=openldap-2.3.19
LDAP_DIR=$(switch_srcdir)/libs/$(LDAP)
LDAPLA=$(LDAP_DIR)/libraries/libldap_r/libldap_r.la
LIBLBERLA=$(LDAP_DIR)/libraries/liblber/liblber.la
LOCAL_LIBADD=$(LDAPLA) $(LIBLBERLA)
switch_srcdir=../../../..

LOCAL_CFLAGS=-DWITH_OPENLDAP -DLDAP_DEPRECATED -I$(LDAP_DIR)/include
include ../../../../build/modmake.rules
