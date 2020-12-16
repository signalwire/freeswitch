/*
 * aws.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * Quoc-Bao Nguyen <baonq5@vng.com.vn>
 *
 * aws.c -- Some Amazon Web Services helper functions
 *
 */
#include "aws.h"
#include <switch.h>

#if defined(HAVE_OPENSSL)
#include <openssl/hmac.h>
#include <openssl/sha.h>
#endif

#if defined(HAVE_OPENSSL)
/**
 * Calculate HMAC-SHA256 hash of a message
 * @param buffer buffer to store the HMAC-SHA256 version of message as byte array
 * @param buffer_length length of buffer
 * @param key buffer that store the key to run HMAC-SHA256
 * @param key_length length of the key
 * @param message message that will be hashed
 * @return byte array, equals to buffer
 */
static char *hmac256(char* buffer, unsigned int buffer_length, const char* key, unsigned int key_length, const char* message)
{
	if (zstr(key) || zstr(message) || buffer_length < SHA256_DIGEST_LENGTH) {
		return NULL;
	}

	HMAC(EVP_sha256(),
		 key,
		 (int)key_length,
		 (unsigned char *)message,
		 strlen(message),
		 (unsigned char*)buffer,
		 &buffer_length);

	return (char*)buffer;
}


/**
 * Calculate HMAC-SHA256 hash of a message
 * @param buffer buffer to store the HMAC-SHA256 version of the message as hex string
 * @param key buffer that store the key to run HMAC-SHA256
 * @param key_length length of the key
 * @param message message that will be hashed
 * @return hex string that store the HMAC-SHA256 version of the message
 */
static char *hmac256_hex(char* buffer, const char* key, unsigned int key_length, const char* message)
{
	char hmac256_raw[SHA256_DIGEST_LENGTH] = { 0 };

	if (hmac256(hmac256_raw, SHA256_DIGEST_LENGTH, key, key_length, message) == NULL) {
		return NULL;
	}

	for (unsigned int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		snprintf(buffer + i*2, 3, "%02x", (unsigned char)hmac256_raw[i]);
	}
	buffer[SHA256_DIGEST_LENGTH * 2] = '\0';

	return buffer;
}


/**
 * Calculate SHA256 hash of a message
 * @param buffer buffer to store the SHA256 version of the message as hex string
 * @param string string to be hashed
 * @return hex string that store the SHA256 version of the message
 */
static char *sha256_hex(char* buffer, const char* string)
{
	unsigned char sha256_raw[SHA256_DIGEST_LENGTH] = { 0 };

	SHA256((unsigned char*)string, strlen(string), sha256_raw);

	for (unsigned int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		snprintf(buffer + i*2, 3, "%02x", sha256_raw[i]);
	}
	buffer[SHA256_DIGEST_LENGTH * 2] = '\0';

	return buffer;
}


/**
 * Get current time_stamp. Example: 20190724T110316Z
 * @param format format of the time in strftime format
 * @param buffer buffer to store the result
 * @param buffer_length length of buffer
 * @return current time stamp
 */
static char *get_time(char* format, char* buffer, unsigned int buffer_length)
{
	switch_time_exp_t time;
	switch_size_t size;

	switch_time_exp_gmt(&time, switch_time_now());

	switch_strftime(buffer, &size, buffer_length, format, &time);

	return buffer;
}


/**
 * Get signature key
 * @param key_signing buffer to store signature key
 * @param aws_s3_profile AWS profile
 * @return key_signing
 */
static char* aws_s3_signature_key(char* key_signing, switch_aws_s3_profile* aws_s3_profile) {

	char key_date[SHA256_DIGEST_LENGTH];
	char key_region[SHA256_DIGEST_LENGTH];
	char key_service[SHA256_DIGEST_LENGTH];
	char* aws4_secret_access_key = switch_mprintf("AWS4%s", aws_s3_profile->access_key_secret);

	hmac256(key_date, SHA256_DIGEST_LENGTH, aws4_secret_access_key, strlen(aws4_secret_access_key), aws_s3_profile->date_stamp);
	hmac256(key_region, SHA256_DIGEST_LENGTH, key_date, SHA256_DIGEST_LENGTH, aws_s3_profile->region);
	hmac256(key_service, SHA256_DIGEST_LENGTH, key_region, SHA256_DIGEST_LENGTH, "s3");
	hmac256(key_signing, SHA256_DIGEST_LENGTH, key_service, SHA256_DIGEST_LENGTH, "aws4_request");

	switch_safe_free(aws4_secret_access_key);

	return key_signing;
}

/**
 * Get query string that will be put together with the signature
 * @param aws_s3_profile AWS profile
 * @return the query string (must be freed)
 */
static char* aws_s3_standardized_query_string(switch_aws_s3_profile* aws_s3_profile)
{
	char* credential;
	char expires[10];
	char* standardized_query_string;

	credential = switch_mprintf("%s%%2F%s%%2F%s%%2Fs3%%2Faws4_request", aws_s3_profile->access_key_id, aws_s3_profile->date_stamp, aws_s3_profile->region);
	switch_snprintf(expires, 9, "%ld", aws_s3_profile->expires);

	standardized_query_string = switch_mprintf(
			"X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=%s&X-Amz-Date=%s&X-Amz-Expires=%s&X-Amz-SignedHeaders=host",
			credential, aws_s3_profile->time_stamp, expires
	);

	switch_safe_free(credential);

	return standardized_query_string;
}

/**
 * Get request string that is used to build string to sign
 * @param aws_s3_profile AWS profile
 * @return the request string (must be freed)
 */
static char* aws_s3_standardized_request(switch_aws_s3_profile* aws_s3_profile) {

	char* standardized_query_string = aws_s3_standardized_query_string(aws_s3_profile);

	char* standardized_request = switch_mprintf(
		"%s\n/%s\n%s\nhost:%s.%s\n\nhost\nUNSIGNED-PAYLOAD",
		aws_s3_profile->verb, aws_s3_profile->object, standardized_query_string, aws_s3_profile->bucket, aws_s3_profile->base_domain
	);

	switch_safe_free(standardized_query_string);

	return standardized_request;
}


/**
 * Create the string to sign for a AWS signature version 4
 * @param standardized_request request string that is used to build string to sign
 * @param aws_s3_profile AWS profile
 * @return the string to sign (must be freed)
 */
static char *aws_s3_string_to_sign(char* standardized_request, switch_aws_s3_profile* aws_s3_profile) {

	char standardized_request_hex[SHA256_DIGEST_LENGTH * 2 + 1] = {'\0'};
	char* string_to_sign;

	sha256_hex(standardized_request_hex, standardized_request);

	string_to_sign = switch_mprintf(
		"AWS4-HMAC-SHA256\n%s\n%s/%s/s3/aws4_request\n%s",
		aws_s3_profile->time_stamp, aws_s3_profile->date_stamp, aws_s3_profile->region, standardized_request_hex
	);

	return string_to_sign;
}

/**
 * Create a full query string that contains signature version 4 for AWS request
 * @param aws_s3_profile AWS profile
 * @return full query string that include the signature
 */
static char *aws_s3_authentication_create(switch_aws_s3_profile* aws_s3_profile) {
	char signature[SHA256_DIGEST_LENGTH * 2 + 1];
	char *string_to_sign;

	char* standardized_query_string;
	char* standardized_request;
	char signature_key[SHA256_DIGEST_LENGTH];
	char* query_param;

	// Get standardized_query_string
	standardized_query_string = aws_s3_standardized_query_string(aws_s3_profile);

	// Get standardized_request
	standardized_request = aws_s3_standardized_request(aws_s3_profile);

	// Get string_to_sign
	string_to_sign = aws_s3_string_to_sign(standardized_request, aws_s3_profile);

	// Get signature_key
	aws_s3_signature_key(signature_key, aws_s3_profile);

	// Get signature
	hmac256_hex(signature, signature_key, SHA256_DIGEST_LENGTH, string_to_sign);

	// Build final query string
	query_param = switch_mprintf("%s&X-Amz-Signature=%s", standardized_query_string, signature);

	switch_safe_free(string_to_sign);
	switch_safe_free(standardized_query_string);
	switch_safe_free(standardized_request);

	return query_param;
}
#endif

/**
 * Append Amazon S3 query params to request if necessary
 * @param headers to add to. AWS signature v4 requires no header to be appended
 * @param profile with S3 credentials
 * @param content_type of object (PUT only)
 * @param verb http methods (GET/PUT)
 * @param url full url
 * @param block_num block number, only used by Azure
 * @param query_string pointer to query param string that will be calculated
 * @return updated headers
 */
SWITCH_MOD_DECLARE(switch_curl_slist_t *) aws_s3_append_headers(
		http_profile_t *profile,
		switch_curl_slist_t *headers,
		const char *verb,
		unsigned int content_length,
		const char *content_type,
		const char *url,
		const unsigned int block_num,
		char **query_string
) {
#if defined(HAVE_OPENSSL)
	switch_aws_s3_profile aws_s3_profile;
	char* url_dup;

	if (!query_string) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required arg query_string.\n");
		return headers;
	}

	// Get bucket and object name from url
	switch_strdup(url_dup, url);
	parse_url(url_dup, profile->base_domain, "s3", &aws_s3_profile.bucket, &aws_s3_profile.object);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "bucket: %s\n", aws_s3_profile.bucket);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "object: %s\n", aws_s3_profile.object);

	// Get date and time
	get_time("%Y%m%d", aws_s3_profile.date_stamp, DATE_STAMP_LENGTH);
	get_time("%Y%m%dT%H%M%SZ", aws_s3_profile.time_stamp, TIME_STAMP_LENGTH);

	// Get access key id and secret
	aws_s3_profile.access_key_id = profile->aws_s3_access_key_id;
	aws_s3_profile.access_key_secret = profile->secret_access_key;

	// Get base domain
	aws_s3_profile.base_domain = profile->base_domain;
	aws_s3_profile.region = profile->region;
	aws_s3_profile.verb = verb;
	aws_s3_profile.expires = profile->expires;

	*query_string = aws_s3_authentication_create(&aws_s3_profile);

	switch_safe_free(url_dup);
#endif
	return headers;
}

/**
 * Get key id, secret and region from env variables or config file
 * @param xml object that store config file
 * @param profile pointer that config will be written to
 * @return status
 */
SWITCH_MOD_DECLARE(switch_status_t) aws_s3_config_profile(switch_xml_t xml, http_profile_t *profile)
{
#if defined(HAVE_OPENSSL)
	switch_xml_t base_domain_xml = switch_xml_child(xml, "base-domain");
	switch_xml_t region_xml = switch_xml_child(xml, "region");
	switch_xml_t expires_xml = switch_xml_child(xml, "expires");

	// Function pointer to be called to append query params to original url
	profile->append_headers_ptr = aws_s3_append_headers;

	/* check if environment variables set the keys */
	profile->aws_s3_access_key_id = getenv("AWS_ACCESS_KEY_ID");
	profile->secret_access_key = getenv("AWS_SECRET_ACCESS_KEY");
	if (!zstr(profile->aws_s3_access_key_id) && !zstr(profile->secret_access_key)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Using AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY environment variables for AWS S3 access for profile \"%s\"\n", profile->name);
		profile->aws_s3_access_key_id = strdup(profile->aws_s3_access_key_id);
		profile->secret_access_key = strdup(profile->secret_access_key);
	} else {
		/* use configuration for keys */
		switch_xml_t id = switch_xml_child(xml, "access-key-id");
		switch_xml_t secret = switch_xml_child(xml, "secret-access-key");
		if (!id || !secret)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing access-key-id or secret-access-key in http_cache.conf.xml for profile \"%s\"\n", profile->name);
			return SWITCH_STATUS_FALSE;
		}

		profile->aws_s3_access_key_id = switch_strip_whitespace(switch_xml_txt(id));
		profile->secret_access_key = switch_strip_whitespace(switch_xml_txt(secret));
		if (zstr(profile->aws_s3_access_key_id) || zstr(profile->secret_access_key)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Empty access-key-id or secret-access-key in http_cache.conf.xml for profile \"%s\"\n", profile->name);
			switch_safe_free(profile->aws_s3_access_key_id);
			switch_safe_free(profile->secret_access_key);
			return SWITCH_STATUS_FALSE;
		}
	}

	// Get region
	if (!region_xml) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing region in http_cache.conf.xml for profile \"%s\"\n", profile->name);
		return SWITCH_STATUS_FALSE;
	}
	profile->region = switch_strip_whitespace(switch_xml_txt(region_xml));
	if (zstr(profile->region)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Empty region in http_cache.conf.xml for profile \"%s\"\n", profile->name);
		switch_safe_free(profile->region);
		return SWITCH_STATUS_FALSE;
	}

	// Get base domain for AWS S3 compatible services. Default base domain is s3.amazonaws.com
	if (base_domain_xml) {
		profile->base_domain = switch_strip_whitespace(switch_xml_txt(base_domain_xml));
		if (zstr(profile->base_domain)) {
			switch_safe_free(profile->base_domain);
			profile->base_domain = switch_mprintf(DEFAULT_BASE_DOMAIN, profile->region);
		}
	} else
	{
		profile->base_domain = switch_mprintf(DEFAULT_BASE_DOMAIN, profile->region);
	}

	// Get expire time for URL signature
	if (expires_xml) {
		char* expires = switch_strip_whitespace(switch_xml_txt(expires_xml));
		if (!zstr(expires) && switch_is_number(expires))
		{
			profile->expires = switch_safe_atoi(expires, DEFAULT_EXPIRATION_TIME);
		} else
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid \"expires\" in http_cache.conf.xml for profile \"%s\"\n", profile->name);
			profile->expires = DEFAULT_EXPIRATION_TIME;
		}
		switch_safe_free(expires);
	} else
	{
		profile->expires = DEFAULT_EXPIRATION_TIME;
	}

#endif

	return SWITCH_STATUS_SUCCESS;
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
