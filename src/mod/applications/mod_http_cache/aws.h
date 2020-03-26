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
 * aws.h - Some Amazon Web Services helper functions
 *
 */
#ifndef AWS_H
#define AWS_H

#include <switch.h>
#include <switch_curl.h>
#include "common.h"

#define DATE_STAMP_LENGTH 9			// 20190729
#define TIME_STAMP_LENGTH 17		// 20190729T083832Z
#define DEFAULT_BASE_DOMAIN "s3.%s.amazonaws.com"
#define DEFAULT_EXPIRATION_TIME 604800

SWITCH_MOD_DECLARE(switch_status_t) aws_s3_config_profile(switch_xml_t xml, http_profile_t *profile);

struct aws_s3_profile {
	const char* base_domain;
	char* bucket;
	char* object;
	char time_stamp[TIME_STAMP_LENGTH];
	char date_stamp[DATE_STAMP_LENGTH];
	const char* verb;
	const char* access_key_id;
	const char* access_key_secret;
	const char* region;
	switch_time_t expires;
};

typedef struct aws_s3_profile switch_aws_s3_profile;

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
