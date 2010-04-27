/* $OpenLDAP: pkg/ldap/include/lutil_ldap.h,v 1.9.2.4 2008/02/11 23:24:10 kurt Exp $ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2008 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#ifndef LUTIL_LDAP_H
#define LUTIL_LDAP_H

#include <ldap_cdefs.h>
#include <lber_types.h>

/*
 * Include file for lutil LDAP routines
 */

LDAP_BEGIN_DECL LDAP_LUTIL_F(void)
	 lutil_sasl_freedefs LDAP_P((void *defaults));

LDAP_LUTIL_F(void *)
	 lutil_sasl_defaults LDAP_P((LDAP * ld, char *mech, char *realm, char *authcid, char *passwd, char *authzid));

LDAP_LUTIL_F(int)
	 lutil_sasl_interact LDAP_P((LDAP * ld, unsigned flags, void *defaults, void *p));

LDAP_END_DECL
#endif /* _LUTIL_LDAP_H */
	 typedef struct lutil_sasl_defaults_s {
		 char *mech;
		 char *realm;
		 char *authcid;
		 char *passwd;
		 char *authzid;
		 char **resps;
		 int nresps;
	 } lutilSASLdefaults;
