/*
 * gcs.h for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2014, Grasshopper
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
 * The Original Code is gcs.h for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Baroukh Ovadia
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Baroukh Ovadia <ovadbar@gmail.com>
 *
 * gcs.h - Some Google Cloud Storage  helper functions
 *
 */
#ifndef GCS_H
#define GCS_H

#include <switch.h>
#include <switch_curl.h>
#include <common.h>

/* (SHA1_LENGTH * 1.37 base64 bytes per byte * 3 url-encoded bytes per byte) */
#define GCS_SIGNATURE_LENGTH_MAX 1024

SWITCH_MOD_DECLARE(switch_curl_slist_t*) gcs_append_headers(http_profile_t *profile, switch_curl_slist_t *headers,
	const char *verb, unsigned int content_length, const char *content_type, const char *url, const unsigned int block_num, char **query_string);
SWITCH_MOD_DECLARE(switch_status_t) gcs_config_profile(switch_xml_t xml, http_profile_t *profile,switch_memory_pool_t *pool);

#endif
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
