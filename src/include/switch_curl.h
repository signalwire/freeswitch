/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
#include "curl/curl.h"


typedef void switch_CURL;
typedef struct curl_slist switch_curl_slist_t;
typedef int switch_CURLINFO;
typedef int switch_CURLcode;
typedef int switch_CURLoption;

SWITCH_DECLARE(switch_CURL *) switch_curl_easy_init(void);
SWITCH_DECLARE(switch_CURLcode) switch_curl_easy_perform(switch_CURL *handle);
SWITCH_DECLARE(switch_CURLcode) switch_curl_easy_getinfo(switch_CURL *curl, switch_CURLINFO info, ... );
SWITCH_DECLARE(void) switch_curl_easy_cleanup(switch_CURL *handle);
SWITCH_DECLARE(switch_curl_slist_t *) switch_curl_slist_append(switch_curl_slist_t * list, const char * string );
SWITCH_DECLARE(void) switch_curl_slist_free_all(switch_curl_slist_t * list);
SWITCH_DECLARE(switch_CURLcode) switch_curl_easy_setopt(CURL *handle, switch_CURLoption option, ...);
SWITCH_DECLARE(const char *) switch_curl_easy_strerror(switch_CURLcode errornum );
SWITCH_DECLARE(void) switch_curl_init(void);
SWITCH_DECLARE(void) switch_curl_destroy(void);
SWITCH_DECLARE(switch_status_t) switch_curl_process_form_post_params(switch_event_t *event, switch_CURL *curl_handle, struct curl_httppost **formpostp);
																
#endif


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

