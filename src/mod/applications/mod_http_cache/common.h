/*
 * aws.h for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * The Original Code is aws.h for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 * Quoc-Bao Nguyen <baonq5@vng.com.vn>
 *
 * common.h - Functions common to the store provider
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <switch.h>

/**
 * An http profile. Defines optional credentials
 * for access to Amazon S3 and Azure Blob Service
 */
struct http_profile {
	const char *name;
	char *aws_s3_access_key_id;
	char *secret_access_key;
	char *base_domain;
	char *region;            // AWS region. Used by AWS S3
	switch_time_t expires;   // Expiration time in seconds for URL signature. Default is 604800 seconds. Used by AWS S3
	switch_size_t bytes_per_block;
	int header_count;
	char** header_names;
	char** header_values;
	// function to be called to add the profile specific headers to the GET/PUT requests
	switch_curl_slist_t *(*append_headers_ptr)(struct http_profile *profile, switch_curl_slist_t *headers,
		const char *verb, unsigned int content_length, const char *content_type, const char *url, const unsigned int block_num, char **query_string);
	// function to be called to perform the profile-specific actions at the end of the PUT operation
	switch_status_t (*finalise_put_ptr)(struct http_profile *profile, const char *url, const unsigned int num_blocks);
};
typedef struct http_profile http_profile_t;


SWITCH_MOD_DECLARE(void) parse_url(char *url, const char *base_domain, const char *default_base_domain, char **bucket, char **object);

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
