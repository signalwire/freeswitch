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
 * Paul Mateer <paul.mateer@jci.com>
 *
 *
 * mod_event_socket_hash.c -- Authentication hashing for the Socket Controlled event handler
 *
 */
#include <switch.h>
#include <switch_utils.h>
#include <switch_stun.h>
#include <openssl/ssl.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include "mod_event_socket_hash.h"

typedef enum {
	HASH_INVALID = 0,
	HASH_MD5 = 1,
	HASH_SHA256 = 5,
	HASH_SHA512 = 6
} hash_algorithm_t;

typedef struct {
	hash_algorithm_t algId;
	char * salt;
	char * hash;
} auth_data_t;

/*
	Concatenates the provided salt and password values and returns memory holding the result.
	This memory should be freed by the caller using switch_safe_free
*/
char * generate_hash_data(char * salt, const char * password) {
	
	if ((password) && (salt)) {
		char * data_to_hash = malloc(strlen(salt) + strlen(password) + 1);

		if (data_to_hash)
		{
			strcpy(data_to_hash, salt);
			strcat(data_to_hash, password);

			return data_to_hash;
		}
	}

	return NULL;
}

/*
	Generate an MD5 hash from the provided data
	This memory should be freed by the caller using switch_safe_free
*/
uint32_t generate_md5_hash(char * data_to_hash, unsigned char ** hash) {
	uint32_t hash_length = SWITCH_MD5_DIGESTSIZE;
	MD5_CTX context;

	*hash = malloc(hash_length);

	if (*hash) {
		MD5_Init(&context);
		MD5_Update(&context, data_to_hash, strlen(data_to_hash));
		MD5_Final(*hash, &context);
	}
	else
		hash_length = 0;

	return hash_length;
}

/*
	Generate a SHA-256 hash from the provided data
	This memory should be freed by the caller using switch_safe_free
*/
uint32_t generate_sha256_hash(char * data_to_hash, unsigned char ** hash) {
	uint32_t hash_length = SHA256_DIGEST_LENGTH;
	SHA256_CTX context;

	*hash = malloc(hash_length);

	if (*hash) {
		SHA256_Init(&context);
		SHA256_Update(&context, data_to_hash, strlen(data_to_hash));
		SHA256_Final(*hash, &context);
	}
	else
		hash_length = 0;

	return hash_length;
}

/*
	Generate a SHA-512 hash from the provided data
	This memory should be freed by the caller using switch_safe_free
*/
uint32_t generate_sha512_hash(char * data_to_hash, unsigned char ** hash) {
	uint32_t hash_length = SHA512_DIGEST_LENGTH;
	SHA512_CTX context;

	*hash = malloc(hash_length);

	if (*hash) {
		SHA512_Init(&context);
		SHA512_Update(&context, data_to_hash, strlen(data_to_hash));
		SHA512_Final(*hash, &context);
	}
	else
		hash_length = 0;

	return hash_length;
}

char *generate_hash(hash_algorithm_t algId, char* salt, const char* password) {

	uint32_t(*hash_func)(char *, unsigned char **);
	unsigned char		b64_key[512];

	b64_key[0] = '\0';
	hash_func = NULL;

	/* Select a hashing function based upon the specified algorithm Id*/
	switch (algId) {
		case HASH_MD5: {
			hash_func = generate_md5_hash;
			break;
		}
		case HASH_SHA256: {
			hash_func = generate_sha256_hash;
			break;
		}
		case HASH_SHA512: {
			hash_func = generate_sha512_hash;
			break;
		}
		default:
			break;
	}

	if (hash_func) {
		/* Merge the salt with the password */
		char * data_to_hash = generate_hash_data(salt, password);

		unsigned char * hash = NULL;
		uint32_t hash_size = 0;

		/* If we have any data to hash then generate a hash */
		if (data_to_hash) {
			hash_size = hash_func(data_to_hash, &hash);
		}

		/* If we have a valid hash then base64 encode it */
		if (hash_size > 0) {
			switch_b64_encode(hash, hash_size, b64_key, sizeof(b64_key));

			/* Free the hash data */
			switch_safe_free(hash);
		}

		/* Free the data to hash */
		switch_safe_free(data_to_hash);
	}

	return strdup((const char *)b64_key);
}

void free_hash_data(auth_data_t** hash_data) {
	if (hash_data)
	{
		if (*hash_data)
		{
			switch_safe_free((*hash_data)->salt);
			switch_safe_free((*hash_data)->hash);
			switch_safe_free(*hash_data);
		}
	}
}

auth_data_t* parse_hash(const char *auth_hash) {

	switch_bool_t valid_b64 = SWITCH_TRUE;
	auth_data_t* data = NULL;

	if (auth_hash) {

		const char *next = auth_hash;
		uint16_t index = 0;
		char *components[3];
		size_t component_size = sizeof(components) / sizeof(char*);

		memset(components, 0, sizeof(components));

		do
		{
			const char *delim = strchr(next, '$');
			if (delim == next)
				next++;
			else {
				if (delim) {
					size_t length = strlen(next) - strlen(delim);
					char *component = malloc(length + 1);
					if (component) {
						memcpy(component, next, length);
						component[length] = '\0';
					}
					else
						break;
					components[index++] = component;
					next = delim++;
				}
				else {
					components[index++] = strdup(next);
					break;
				}
			}
		} while (index < component_size);

		if (component_size == index) {
			hash_algorithm_t algId = HASH_INVALID;

			switch (switch_safe_atoi(components[0], 0)) {
				case 1: {
					algId = HASH_MD5;
					break;
				}
				case 5: {
					algId = HASH_SHA256;
					break;
				}
				case 6: {
					algId = HASH_SHA512;
					break;
				}
			}

			if (algId != HASH_INVALID) {
				size_t b64_length = strlen(components[2]);

				/* Account for any padding characters at the end of the string */
				for (size_t idx = 0; idx < 2; idx++) {
					if ('=' == components[2][b64_length - 1]) {
						b64_length--;
					}
					else
						break;
				}

				/* Confirm that the component contains only legitimate b64 characters */
				for (size_t idx = 0; idx < b64_length; idx++) {
					if (switch_isalnum(components[2][idx]))
						continue;
					switch (components[2][idx]) {
					case '/':
					case '+':
						continue;
					}

					valid_b64 = SWITCH_FALSE;
					break;
				}

				if (valid_b64) {
					/* Create a authentication data structure to hold the details*/
					data = malloc(sizeof(auth_data_t));

					if (data)
					{
						switch_safe_free(components[0]);

						data->algId = algId;
						data->salt = strdup(components[1]);
						data->hash = strdup(components[2]);
					}
				}
			}
		}

		/* Free the memory allocated for the component array */
		for (size_t idx = 0; idx < component_size; idx++)
		{
			switch_safe_free(components[0]);
		}
	}
	return data;
}

switch_bool_t validate_hash(const char *auth_hash) {

	auth_data_t* data = parse_hash(auth_hash);

	if (data)
	{
		free_hash_data(&data);
		return SWITCH_TRUE;
	}
	return SWITCH_FALSE;
}

switch_bool_t validate_password(const char *auth_hash, const char *password) {

	switch_bool_t is_valid = SWITCH_FALSE;
	auth_data_t* data = parse_hash(auth_hash);

	if (data)
	{
		char *hash = generate_hash(data->algId, data->salt, password);

		if ((hash) && (!strcmp(hash, data->hash)))
		{
			is_valid = SWITCH_TRUE;
		}
		switch_safe_free(hash);
		free_hash_data(&data);
	}
	return is_valid;
}

const char * create_auth_hash(const char *password) {

	char *auth_hash = NULL;
	char *hash = NULL;
	char salt[30];

	switch_stun_random_string(salt, 20, NULL);

	hash = generate_hash(HASH_SHA512, salt, password);

	if (hash)
	{
		size_t auth_hash_len = 5 + strlen(salt) + strlen(hash);

		auth_hash = malloc(auth_hash_len);
		sprintf(auth_hash, "$6$%s$%s", salt, hash);
	}

	return auth_hash;
}
