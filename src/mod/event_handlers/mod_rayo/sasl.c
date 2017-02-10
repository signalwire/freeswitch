/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013, Grasshopper
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
 * The Original Code is mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * sasl.c -- SASL functions
 *
 */
#include <switch.h>
#include <iksemel.h>
#include "sasl.h"

/**
 * Parse authzid, authcid, and password tokens from base64 PLAIN auth message.
 * @param message the base-64 encoded authentication message
 * @param authzid the authorization id in the message - free this string when done with parsed message
 * @param authcid the authentication id in the message
 * @param password the password in the message
 */
void parse_plain_auth_message(const char *message, char **authzid, char **authcid, char **password)
{
	char *decoded = iks_base64_decode(message);
	int maxlen = strlen(message) * 6 / 8 + 1;
	int pos = 0;
	*authzid = NULL;
	*authcid = NULL;
	*password = NULL;
	if (decoded == NULL) {
		goto end;
	}
	pos = strlen(decoded) + 1;
	if (pos >= maxlen) {
		goto end;
	}
	*authcid = strdup(decoded + pos);
	pos += strlen(*authcid) + 1;
	if (pos >= maxlen) {
		goto end;
	}
	*password = strdup(decoded + pos);
	if (zstr(decoded)) {
		*authzid = strdup(*authcid);
	} else {
		*authzid = strdup(decoded);
	}

 end:
	switch_safe_free(decoded);
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
