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
	char *region;					// AWS region. Used by AWS S3
	switch_time_t expires;			// Expiration time in seconds for URL signature. Default is 604800 seconds. Used by AWS S3
	switch_size_t bytes_per_block;
	char* backup_folder;			// backup folder used when failed to upload file by http_put

	// function to be called to add the profile specific headers to the GET/PUT requests
	switch_curl_slist_t *(*append_headers_ptr)(struct http_profile *profile, switch_curl_slist_t *headers,
		const char *verb, unsigned int content_length, const char *content_type, const char *url, const unsigned int block_num, char **query_string);
	// function to be called to perform the profile-specific actions at the end of the PUT operation
	switch_status_t (*finalise_put_ptr)(struct http_profile *profile, const char *url, const unsigned int num_blocks);
};
typedef struct http_profile http_profile_t;


SWITCH_MOD_DECLARE(void) parse_url(char *url, const char *base_domain, const char *default_base_domain, char **bucket, char **object);


/**
 * Get current time_stamp. Example: 20190724T110316Z
 * @param format format of the time in strftime format
 * @param buffer buffer to store the result
 * @param buffer_length length of buffer
 * @return current time stamp
 */
SWITCH_MOD_DECLARE(char*) get_time(char* format, char* buffer, unsigned int buffer_length);


/**
 * Calculate HMAC-SHA256 hash of a message
 * @param buffer buffer to store the HMAC-SHA256 version of message as byte array
 * @param buffer_length length of buffer
 * @param key buffer that store the key to run HMAC-SHA256
 * @param key_length length of the key
 * @param message message that will be hashed
 * @return byte array, equals to buffer
 */
SWITCH_MOD_DECLARE(char*) hmac256(char* buffer, unsigned int buffer_length, const char* key, unsigned int key_length, const char* message);

/**
 * Calculate HMAC-SHA256 hash of a message
 * @param buffer buffer to store the HMAC-SHA256 version of the message as hex string
 * @param key buffer that store the key to run HMAC-SHA256
 * @param key_length length of the key
 * @param message message that will be hashed
 * @return hex string that store the HMAC-SHA256 version of the message
 */
SWITCH_MOD_DECLARE(char*) hmac256_hex(char* buffer, const char* key, unsigned int key_length, const char* message);


/**
 * Calculate SHA256 hash of a message
 * @param buffer buffer to store the SHA256 version of the message as hex string
 * @param string string to be hashed
 * @return hex string that store the SHA256 version of the message
 */
SWITCH_MOD_DECLARE(char*) sha256_hex(char* buffer, const char* string);



/**
 * Backup file to local storage. Used with http_put in case of failed
 * @param source
 * @param dest
 * @return
 */
SWITCH_MOD_DECLARE(int) backup_file(const char* source, const char* dest);


/**
 * Create a folder if it is not exists. Equivalent to 'mkdir -p'
 * @param path
 */
SWITCH_MOD_DECLARE(void) recursive_mkdir(char *path);

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
