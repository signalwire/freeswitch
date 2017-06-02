/*
 * azure.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * The Original Code is aws.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 * Richard Screene <richard.screene@thisisdrum.com>
 *
 * azure.c -- Some Azure Blob Service helper functions
 *
 */
#include "azure.h"
#include <switch.h>
#include <switch_curl.h>

#if defined(HAVE_OPENSSL)
#include <openssl/hmac.h>
#include <openssl/sha.h>
#endif

#define SHA256_LENGTH 32

#define MS_VERSION "2015-12-11"

#define BLOCK_STR_LENGTH 17
#define BLOCK_ID_LENGTH 25

struct curl_memory_read {
	char *read_ptr;
	size_t size_left;
};
typedef struct curl_memory_read curl_memory_read_t;

#if defined(_WIN32) || defined(_WIN64)
# define strtok_r strtok_s
#endif

/**
 * Convert query string parameters into string to be appended to
 * Azure authentication header
 * @param query_string The string string to convert
 * @return the canonicalised resources (must be freed)
 */
static char *canonicalise_query_string(const char *query_string) {
	char *saveptr = NULL;
	char out_str[1024] = "";
	char *p = out_str;
	char *query_string_dup = switch_safe_strdup(query_string);
	char *in_str = (char *) query_string_dup;
	char *kvp;

	while ((kvp = strtok_r(in_str, "&", &saveptr)) != NULL) {
		char *value = strchr(kvp, '=');
		if (value) {
			*value = '\0';
			value ++;
			p += switch_snprintf(p, &out_str[sizeof(out_str)] - p, "\n%s:%s", kvp, value);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "canonicalise_query_string - badly formatted query string parameter=%s\n", kvp);
		}
		in_str = NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "canonicalise_query_string - out_str=%s\n", out_str);
	switch_safe_free(query_string_dup);
	return strdup(out_str);
}

/**
 * Create the string to sign for a Azure Blob Service signature calculation
 * @param verb (PUT/GET)
 * @param account account blob is stored in
 * @param blob to access (filename.ext)
 * @param content_length content length
 * @param content_type optional content type
 * @param content_md5 optional content MD5 checksum
 * @param date header
 * @param resources the canonicalised resources
 * @return the string_to_sign (must be freed)
 */
static char *azure_blob_string_to_sign(const char *verb, const char *account, const char *blob, unsigned int content_length, const char *content_type, const char *content_md5, const char *date, const char *resources)
{
	char *content_length_str = NULL;

	if (content_length > 0) {
		content_length_str = switch_mprintf("%d", content_length);
	}

	return switch_mprintf("%s\n\n\n%s\n%s\n%s\n%s\n\n\n\n\n\nx-ms-version:" MS_VERSION "\n/%s/%s%s",
		verb, content_length_str ? content_length_str : "", content_md5 ? content_md5 : "", content_type ? content_type : "",
		date, account, blob, resources);
}

/**
 * Create the Azure Blob Service signature
 * @param signature buffer to store the signature
 * @param signature_length length of signature buffer
 * @param string_to_sign
 * @param secret_access_key secret access key
 * @return the signature buffer or NULL if missing input
 */
static char *azure_blob_signature(char *signature, int signature_length, const char *string_to_sign, const char *secret_access_key)
{
#if defined(HAVE_OPENSSL)
	unsigned int signature_raw_length = SHA256_LENGTH;
	char signature_raw[SHA256_LENGTH];
#endif

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "azure_blob_signature to '%s'\n", string_to_sign);

#if defined(HAVE_OPENSSL)
	signature_raw[0] = '\0';
	if (!signature || signature_length <= 0) {
		return NULL;
	}
	if (zstr(secret_access_key)) {
		return NULL;
	}
	if (!string_to_sign) {
		string_to_sign = "";
	}

	HMAC(EVP_sha256(),
		secret_access_key, strlen(secret_access_key),
		 (const unsigned char *)string_to_sign,
		 strlen(string_to_sign),
		 (unsigned char *)signature_raw,
		 &signature_raw_length);

	/* convert result to base64 */
	switch_b64_encode((unsigned char *)signature_raw, signature_raw_length, (unsigned char *)signature, signature_length);

#endif
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "azure_blob_signature result %s\n", signature);
	return signature;
}

/**
 * Create an authentication signature for Azure Blob Service
 * @param verb (PUT/GET)
 * @param url address (virtual-host-style)
 * @param base_domain (optional - Azure Blob assumed if not specified)
 * @param content_length content length
 * @param content_type optional content type
 * @param content_md5 optional content MD5 checksum
 * @param key secret access key
 * @param date header
 * @param query_string extra parameters for the URL
 * @return signature for Authorization header (must be freed)
 */
static char *azure_blob_authentication_create(const char *verb, const char *url, const char *base_domain, unsigned int content_length, const char *content_type, const char *content_md5, const char *key, const char *date, const char *query_string)
{
	char signature[AZURE_SIGNATURE_LENGTH_MAX] = "";
	char *string_to_sign;
	char *url_dup = strdup(url);
	char *account;
	char *blob;
	char *resources;
	char *result;

	resources = canonicalise_query_string(query_string);

	/* create base64 encoded signature */
	parse_url(url_dup, base_domain, "blob", &account, &blob);
	string_to_sign = azure_blob_string_to_sign(verb, account, blob, content_length, content_type, content_md5, date, resources);
	azure_blob_signature(signature, AZURE_SIGNATURE_LENGTH_MAX, string_to_sign, key);

	result = switch_mprintf("SharedKey %s:%s", account, signature);

	free(string_to_sign);
	free(url_dup);
	free(resources);

	return result;
}

/**
 * Read callback for libcurl - reads data from memory.  Same function signature as fread(3)
 */
static size_t curl_memory_read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	curl_memory_read_t *info = (curl_memory_read_t *) userp;
	size_t bytes_requested = size * nmemb;
	size_t items;

	if (info->read_ptr == NULL) {
		return 0;
	} else if (bytes_requested <= info->size_left) {
		memcpy(ptr, info->read_ptr, bytes_requested);
		info->read_ptr += bytes_requested;
		info->size_left -= bytes_requested;
		return nmemb;
	} else {
		memcpy(ptr, info->read_ptr, info->size_left);
		info->read_ptr = NULL;
		items = info->size_left / size;
		info->size_left = 0;
		return items;
	}
}

/** Convert the block number to a base64 encoded string
 * @param num the number to encode
 * @result the base64 string (must be freed)
 */
static char *azure_blob_block_num_to_id(const unsigned int num)
{
	char num_str[BLOCK_STR_LENGTH], num_len;
	char *out_str;

	num_len = switch_snprintf(num_str, sizeof(num_str), "%016d", num);

	switch_malloc(out_str, BLOCK_ID_LENGTH);

	switch_b64_encode((unsigned char *) num_str, num_len, (unsigned char *) out_str, BLOCK_ID_LENGTH);

	return out_str;
}

/**
 * Send blocklist message once we have uploaded all of the blob blocks.
 * @param url the url to send the request to
 * @param base_domain (optional - Azure Blob assumed if not specified)
 * @param key secret access key
 * @param num_blocks the number of blocks that the file was sent in
 * @return SWITCH_STATUS_SUCCESS on success
 */
switch_status_t azure_blob_finalise_put(http_profile_t *profile, const char *url, const unsigned int num_blocks) {
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_curl_slist_t *headers = NULL;
	CURL *curl_handle = NULL;
	long httpRes = 0;
	char xmlDoc[2048] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<BlockList>\n";
	char *p = &xmlDoc[strlen(xmlDoc)];
	char *query_string = NULL;
	char *full_url = NULL;
	curl_memory_read_t upload_info;

	for (int i = 1; i < num_blocks; i ++) {
		char *block_id = azure_blob_block_num_to_id(i);
		p += switch_snprintf(p, &xmlDoc[sizeof(xmlDoc)] - p, "  <Uncommitted>%s</Uncommitted>\n", block_id);
		switch_safe_free(block_id);
	}
	strncpy(p, "</BlockList>", &xmlDoc[sizeof(xmlDoc)] - p);

	headers = switch_curl_slist_append(headers, "Content-Type: application/xml");
	headers = azure_blob_append_headers(profile, headers, "PUT", strlen(xmlDoc), "application/xml", url, 0, &query_string);

	if (query_string) {
		full_url = switch_mprintf("%s?%s", url, query_string);
		free(query_string);
	} else {
		switch_strdup(full_url, url);
	}

	curl_handle = switch_curl_easy_init();
	if (!curl_handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_curl_easy_init() failure\n");
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	switch_curl_easy_setopt(curl_handle, CURLOPT_PUT, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
	switch_curl_easy_setopt(curl_handle, CURLOPT_URL, full_url);
	switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
	switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-http-cache/1.0");

	upload_info.read_ptr = xmlDoc;
	upload_info.size_left = strlen(xmlDoc);
	switch_curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, curl_memory_read_callback);
	switch_curl_easy_setopt(curl_handle, CURLOPT_READDATA, &upload_info);
	switch_curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)strlen(xmlDoc));

	//NB. we ignore connect_timeout, ssl_verifypeer, ssl_cacert, ssl_verifyhost cache options

	switch_curl_easy_perform(curl_handle);
	switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	switch_curl_easy_cleanup(curl_handle);

	if (httpRes == 200 || httpRes == 201 || httpRes == 202 || httpRes == 204) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "final saved to %s\n", url);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received HTTP error %ld trying to save %s\n", httpRes, url);
		status = SWITCH_STATUS_GENERR;
	}

	done:
		switch_safe_free(full_url);

		if (headers) {
			switch_curl_slist_free_all(headers);
		}

	return status;
}

/**
 * Append the specific Azure Blob Service headers
 * @param headers the list of headers to append to
 * @param base_domain (optional - Azure Blob assumed if not specified)
 * @param key secret access key
 * @param verb (PUT/GET)
 * @param content_length content length
 * @param content_type optional content type
 * @param url the url to send the request to
 * @param block_id the base64 encoded ID of the block
 * @param query_string returned (must be freed)
 * @return list of headers (must be freed)
 */

switch_curl_slist_t *azure_blob_append_headers(http_profile_t *profile, switch_curl_slist_t *headers,
	const char *verb, unsigned int content_length, const char *content_type, const char *url, const unsigned int block_num, char **query_string)
{
	char date[256];
	char header[1024];
	char *authenticate;
	char *my_query_string = NULL;

	if (!strcmp(verb, "PUT")) {
		if (block_num > 0) {
			char *block_id = azure_blob_block_num_to_id(block_num);
			my_query_string = switch_mprintf("blockid=%s&comp=block", block_id);
			switch_safe_free(block_id);
		} else {
			switch_strdup(my_query_string, "comp=blocklist");
		}
	}

	/* Date: */
	switch_rfc822_date(date, switch_time_now());
	switch_snprintf(header, sizeof(header), "Date: %s", date);
	headers = switch_curl_slist_append(headers, header);

	headers = switch_curl_slist_append(headers, "x-ms-version: " MS_VERSION);

	/* Authorization: */
	authenticate = azure_blob_authentication_create(verb, url, profile->base_domain, content_length,
		content_type, "", profile->secret_access_key, date, my_query_string);
	switch_snprintf(header, sizeof(header), "Authorization: %s", authenticate);
	free(authenticate);
	headers = switch_curl_slist_append(headers, header);

	if (query_string) {
		*query_string = my_query_string;
	} else {
		switch_safe_free(my_query_string);
	}

	return headers;
}

/**
 * Read the Azure Blob Service profile
 * @param name the name of the profile
 * @param xml the portion of the XML document containing the profile
 * @param access_key_id returned value of access_key_id in the configuration
 * @param secret_access_key returned value of secret_access_key in the configuration
 * @param base_domain returned value of base_domain in the configuration
 * @param bytes_per_block returned value of bytes_per_block in the configuration
 * @return SWITCH_STATUS_SUCCESS on success
 */
switch_status_t azure_blob_config_profile(switch_xml_t xml, http_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *key = NULL;
	switch_xml_t base_domain_xml = switch_xml_child(xml, "base-domain");

	profile->append_headers_ptr = azure_blob_append_headers;
	profile->finalise_put_ptr = azure_blob_finalise_put;

	/* check if environment variables set the keys */
	key = getenv("AZURE_STORAGE_ACCESS_KEY");
	if (!zstr(key)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						  "Using AZURE_STORAGE_ACCESS_KEY environment variables for Azure access on profile \"%s\"\n", profile->name);
		key = switch_safe_strdup(key);
	} else {
		/* use configuration for keys */
		switch_xml_t secret = switch_xml_child(xml, "secret-access-key");

		if (secret) {
			key = switch_strip_whitespace(switch_xml_txt(secret));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing key secret\n");
		}
	}

	if (zstr(key)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing Azure Blob credentials for profile \"%s\"\n", profile->name);
		status = SWITCH_STATUS_FALSE;
	} else {
		// convert to UTF-8
		switch_malloc(profile->secret_access_key, AZURE_SIGNATURE_LENGTH_MAX);
		switch_b64_decode((char *) key, profile->secret_access_key, AZURE_SIGNATURE_LENGTH_MAX);
	}
	switch_safe_free(key);

	profile->bytes_per_block = 4e6;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set number of bytes per block to %zu\n", profile->bytes_per_block);

	if (base_domain_xml) {
		profile->base_domain = switch_strip_whitespace(switch_xml_txt(base_domain_xml));
		if (zstr(profile->base_domain)) {
			switch_safe_free(profile->base_domain);
		}
	}

	return status;
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
