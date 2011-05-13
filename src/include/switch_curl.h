/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * switch_curl.h
 *
 */

#ifndef __SWITCH_CURL_H
#define __SWITCH_CURL_H

#include <curl/curl.h>
#include "switch_ssl.h"

static inline void switch_curl_init(void)
{
	int curl_count = switch_core_curl_count(NULL);

	if (curl_count == 0) {
		curl_global_init(CURL_GLOBAL_ALL);
#if defined(HAVE_OPENSSL)
		switch_curl_init_ssl_locks();
#endif
	}

	curl_count++;
	switch_core_curl_count(&curl_count);
}

static inline void switch_curl_destroy()
{
	int curl_count = switch_core_curl_count(NULL);
	
	curl_count--;

	if (curl_count == 0) {

#if defined(HAVE_OPENSSL)
		switch_curl_destroy_ssl_locks();
#endif
		curl_global_cleanup();
	}
	switch_core_curl_count(&curl_count);
}

#endif


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

