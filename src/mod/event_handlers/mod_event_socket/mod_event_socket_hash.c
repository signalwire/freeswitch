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
#include "mod_event_socket_hash.h"

typedef struct {
	wchar_t * alg;
	char * salt;
	char * hash;
} auth_data_t;



char *generate_salt() {
	UINT random_factor = 0;
	char salt_buffer[40];
	char num_buffer[15];

	salt_buffer[0] = '\0';	

	while (strlen(salt_buffer) < 22)
	{
		if (SUCCEEDED(BCryptGenRandom(NULL, (BYTE*)&random_factor, sizeof(UINT), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
			sprintf_s(num_buffer, 15, "%u", random_factor);
			strcat(salt_buffer, num_buffer);
		}
		else
			return NULL;
	}

	return strdup(salt_buffer);
}

char *generate_hash(wchar_t* alg, char* salt, const char* password) {

	unsigned char		b64_key[512];

	b64_key[0] = '\0';

	BCRYPT_ALG_HANDLE	alg_handle = NULL;
	NTSTATUS			status = BCryptOpenAlgorithmProvider(&alg_handle, alg, NULL, 0);

	if (SUCCEEDED(status)) 	{
		DWORD data_size = 0;
		DWORD hash_object_size = 0;

		status = BCryptGetProperty(alg_handle, BCRYPT_OBJECT_LENGTH, (PBYTE)&hash_object_size, sizeof(DWORD), &data_size, 0);

		if (SUCCEEDED(status)) {
			//allocate the hash object on the heap
			PBYTE pbHashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, hash_object_size);

			if (NULL != pbHashObject) {
				DWORD hash_size = 0;

				//calculate the length of the hash
				status = BCryptGetProperty(alg_handle, BCRYPT_HASH_LENGTH, (PBYTE)&hash_size, sizeof(DWORD), &data_size, 0);

				if (SUCCEEDED(status)) {

					//allocate the hash buffer on the heap
					PBYTE	hash = (PBYTE)HeapAlloc(GetProcessHeap(), 0, hash_size);

					if (NULL != hash) {

						BCRYPT_HASH_HANDLE	hash_handle = NULL;

						status = BCryptCreateHash(alg_handle, &hash_handle, pbHashObject, hash_object_size, NULL, 0, 0);

						if (SUCCEEDED(status)) {

							size_t salted_data_len = strlen(salt) + strlen(password);
							char* salted_data = malloc(salted_data_len + 1);

							if (salted_data) {

								strcpy_s(salted_data, salted_data_len + 1, salt);
								strcat_s(salted_data, salted_data_len + 1, password);

								status = BCryptHashData(hash_handle, (PBYTE)salted_data, (ULONG)salted_data_len, 0);

								if (SUCCEEDED(status)) {

									status = BCryptFinishHash(hash_handle, hash, hash_size, 0);

									if (SUCCEEDED(status)) {
										switch_b64_encode(hash, hash_size, b64_key, sizeof(b64_key));
									}
								}
							}
						}

						HeapFree(GetProcessHeap(), 0, hash);
					}
				}

				HeapFree(GetProcessHeap(), 0, pbHashObject);
			}
		}

		BCryptCloseAlgorithmProvider(alg_handle, 0);
	}

	return strdup((const char *)b64_key);
}

void free_hash_data(auth_data_t** hash_data) {
	if (hash_data)
	{
		if (*hash_data)
		{
			(*hash_data)->alg = NULL;
			switch_safe_free((*hash_data)->salt);
			switch_safe_free((*hash_data)->hash);
			switch_safe_free(*hash_data);
		}
	}
}

auth_data_t* parse_hash(const char *auth_hash) {

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
						strncpy(component, next, length);
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
			wchar_t* alg = NULL;
			if (strlen(components[0]) == 1)
			{
				switch (components[0][0]) {
					case '1': {
						alg = BCRYPT_MD5_ALGORITHM;
						break;
					}
					case '5': {
						alg = BCRYPT_SHA256_ALGORITHM;
						break;
					}
					case '6': {
						alg = BCRYPT_SHA512_ALGORITHM;
						break;
					}
				}
			}

			if (alg) {
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
				switch_bool_t valid_b64 = SWITCH_TRUE;
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

						data->alg = alg;
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
		char *hash = generate_hash(data->alg, data->salt, password);

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
	char *salt = generate_salt();

	if (salt) {
		char *hash = generate_hash(BCRYPT_SHA512_ALGORITHM, salt, password);

		if (hash)
		{
			size_t auth_hash_len = 5 + strlen(salt) + strlen(hash);

			auth_hash = malloc(auth_hash_len);
			sprintf_s(auth_hash, auth_hash_len, "$6$%s$%s", salt, hash);
		}

		switch_safe_free(salt);
	}

	return auth_hash;
}