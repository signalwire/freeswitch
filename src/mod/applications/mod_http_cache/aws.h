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
 *
 * aws.h - Some Amazon Web Services helper functions
 *
 */
#ifndef AWS_H
#define AWS_H

#include <switch.h>

/* (SHA1_LENGTH * 1.37 base64 bytes per byte * 3 url-encoded bytes per byte) */
#define S3_SIGNATURE_LENGTH_MAX 83

int aws_s3_is_s3_url(const char *url, const char *base_domain);
void aws_s3_parse_url(char *url, const char *base_domain, char **bucket, char **object);
char *aws_s3_string_to_sign(const char *verb, const char *bucket, const char *object, const char *content_type, const char *content_md5, const char *date);
char *aws_s3_signature(char *signature, int signature_length, const char *string_to_sign, const char *aws_secret_access_key);
char *aws_s3_presigned_url_create(const char *verb, const char *url, const char *base_domain, const char *content_type, const char *content_md5, const char *aws_access_key_id, const char *aws_secret_access_key, const char *expires);
char *aws_s3_authentication_create(const char *verb, const char *url, const char *base_domain, const char *content_type, const char *content_md5, const char *aws_access_key_id, const char *aws_secret_access_key, const char *date);

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
