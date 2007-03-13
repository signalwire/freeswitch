# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
MODNAME=mod_ldap
LOCAL_CFLAGS=-DWITH_OPENLDAP -DLDAP_DEPRECATED
LOCAL_LDFLAGS=-lldap_r -llber
LOCAL_OBJS=
include ../../../../build/modmake.rules

local_depend:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install openldap-2.3.19.tgz --prefix=$(PREFIX) --disable-bdb --disable-hdb
