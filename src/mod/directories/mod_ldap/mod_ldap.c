/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * mod_ldap.c -- LDAP
 *
 */
#include <switch.h>
#ifdef MSLDAP
#include <windows.h>
#include <winldap.h>
#include <winber.h>
#define LDAP_OPT_SUCCESS LDAP_SUCCESS
#else
#include <lber.h>
#include <ldap.h>
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_ldap_load);
SWITCH_MODULE_DEFINITION(mod_ldap, mod_ldap_load, NULL, NULL);

struct ldap_context {
	LDAP *ld;
	LDAPMessage *msg;
	LDAPMessage *entry;
	BerElement *ber;
	char *attr;
	char *var;
	char *val;
	char **vals;
	int itt;
	int vitt;
	int vi;
};


static switch_status_t mod_ldap_open(switch_directory_handle_t *dh, char *source, char *dsn, char *passwd)
{
	struct ldap_context *context;
	int auth_method = LDAP_AUTH_SIMPLE;
	int desired_version = LDAP_VERSION3;

	if ((context = switch_core_alloc(dh->memory_pool, sizeof(*context))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	if ((context->ld = ldap_init(source, LDAP_PORT)) == NULL) {
		return SWITCH_STATUS_FALSE;
	}

	/* set the LDAP version to be 3 */
	if (ldap_set_option(context->ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version) != LDAP_OPT_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	if (ldap_bind_s(context->ld, dsn, passwd, auth_method) != LDAP_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	dh->private_info = context;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t mod_ldap_close(switch_directory_handle_t *dh)
{
	struct ldap_context *context;

	context = dh->private_info;
	switch_assert(context != NULL);

	ldap_unbind_s(context->ld);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t mod_ldap_query(switch_directory_handle_t *dh, char *base, char *query)
{
	struct ldap_context *context;
	char **attrs = NULL;

	context = dh->private_info;
	switch_assert(context != NULL);
#if _MSC_VER >= 1500
	/* Silence warning from incorrect code analysis signature.  attrs == NULL indicates to return all attrs */
	/* http://msdn2.microsoft.com/en-us/library/aa908101.aspx */
	__analysis_assume(attrs);
#endif

	if (ldap_search_s(context->ld, base, LDAP_SCOPE_SUBTREE, query, attrs, 0, &context->msg) != LDAP_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	if (ldap_count_entries(context->ld, context->msg) <= 0) {
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t mod_ldap_next(switch_directory_handle_t *dh)
{
	struct ldap_context *context;

	context = dh->private_info;
	switch_assert(context != NULL);

	context->vitt = 0;

	if (!context->itt) {
		context->entry = ldap_first_entry(context->ld, context->msg);
		context->itt++;
	} else if (context->entry) {
		context->entry = ldap_next_entry(context->ld, context->entry);
		context->itt++;
	}

	if (!context->entry) {
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t mod_ldap_next_pair(switch_directory_handle_t *dh, char **var, char **val)
{
	struct ldap_context *context;

	context = dh->private_info;
	switch_assert(context != NULL);

	*var = *val = NULL;

	if (!context->entry) {
		return SWITCH_STATUS_FALSE;
	}

	if (!context->vitt) {
		context->var = "dn";
		context->val = ldap_get_dn(context->ld, context->entry);
		context->vitt++;
		*var = context->var;
		*val = context->val;
		return SWITCH_STATUS_SUCCESS;
	} else {
	  itter:
		if (context->attr && context->vals) {
			if ((*val = context->vals[context->vi++])) {
				*var = context->attr;
				return SWITCH_STATUS_SUCCESS;
			} else {
				ldap_value_free(context->vals);
				context->vals = NULL;
				context->vi = 0;
			}
		}

		if (context->vitt == 1) {
			ldap_memfree(context->val);
			context->val = NULL;
			if (context->ber) {
				ber_free(context->ber, 0);
				context->ber = NULL;
			}
			context->attr = ldap_first_attribute(context->ld, context->entry, &context->ber);
		} else {
			if (context->attr) {
				ldap_memfree(context->attr);
			}
			context->attr = ldap_next_attribute(context->ld, context->entry, context->ber);
		}
		context->vitt++;
		if (context->entry && context->attr && (context->vals = ldap_get_values(context->ld, context->entry, context->attr)) != 0) {
			goto itter;
		}
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_ldap_load)
{
	switch_directory_interface_t *dir_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	dir_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_DIRECTORY_INTERFACE);
	dir_interface->interface_name = "ldap";
	dir_interface->directory_open = mod_ldap_open;
	dir_interface->directory_close = mod_ldap_close;
	dir_interface->directory_query = mod_ldap_query;
	dir_interface->directory_next = mod_ldap_next;
	dir_interface->directory_next_pair = mod_ldap_next_pair;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
