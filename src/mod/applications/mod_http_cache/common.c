/*
 * common.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * The Original Code is common.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 * Quoc-Bao Nguyen <baonq5@vng.com.vn>
 *
 * common.c - Functions common to the store provider
 *
 */


#include <switch.h>

/**
* Reverse string substring search
*/
static char *my_strrstr(const char *haystack, const char *needle)
{
	char *s;
	size_t needle_len;
	size_t haystack_len;

	if (zstr(haystack)) {
		return NULL;
	}

	if (zstr(needle)) {
		return (char *)haystack;
	}

	needle_len = strlen(needle);
	haystack_len = strlen(haystack);
	if (needle_len > haystack_len) {
		return NULL;
	}

	s = (char *)(haystack + haystack_len - needle_len);
	do {
		if (!strncmp(s, needle, needle_len)) {
			return s;
		}
	} while (s-- != haystack);

	return NULL;
}

SWITCH_MOD_DECLARE(void) parse_url(char *url, const char *base_domain, const char *default_base_domain, char **bucket, char **object)
{
	char *bucket_start = NULL;
	char *bucket_end;
	char *object_start;
	char *p;
	char base_domain_match[1024];

	*bucket = NULL;
	*object = NULL;

	if (zstr(url)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "url is empty\n");
		return;
	}

	/* expect: http(s)://bucket.foo-bar.s3.amazonaws.com/object */
	if (!strncasecmp(url, "https://", 8)) {
		bucket_start = url + 8;
	} else if (!strncasecmp(url, "http://", 7)) {
		bucket_start = url + 7;
	}

	if (zstr(bucket_start)) { /* invalid URL */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid url\n");
		return;
	}

	if (zstr(base_domain)) {
		base_domain = default_base_domain;
	}
	switch_snprintf(base_domain_match, 1024, ".%s", base_domain);
	bucket_end = my_strrstr(bucket_start, base_domain_match);

	if (!bucket_end) { /* invalid URL */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid url\n");
		return;
	}

	*bucket_end = '\0';

	object_start = strchr(bucket_end + 1, '/');

	if (!object_start) { /* invalid URL */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid url\n");
		return;
	}

	object_start++;

	if (zstr(bucket_start) || zstr(object_start)) { /* invalid URL */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid url\n");
		return;
	}

	/* ignore the query string from the end of the URL */
	if ((p = strchr(object_start, '&'))) {
		*p = '\0';
	}

	*bucket = bucket_start;
	*object = object_start;
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
