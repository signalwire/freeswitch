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

/* 160 bits / 8 bits per byte */
#define SHA1_LENGTH 20

/**
 * Create the string to sign for a AWS signature calculation
 * @param verb (PUT/GET)
 * @param bucket bucket object is stored in
 * @param object to access (filename.ext)
 * @param content_type optional content type
 * @param content_md5 optional content MD5 checksum
 * @param date header
 * @return the string_to_sign (must be freed)
 */
static char *aws_s3_string_to_sign(const char *verb, const char *bucket, const char *object, const char *content_type, const char *content_md5, const char *date)
{
	/*
	 * String to sign has the following format:
	 *   <HTTP-VERB>\n<Content-MD5>\n<Content-Type>\n<Expires/Date>\n/bucket/object
	 */
	return switch_mprintf("%s\n%s\n%s\n%s\n/%s/%s",
		verb, content_md5 ? content_md5 : "", content_type ? content_type : "",
		date, bucket, object);
}

/**
 * Create the AWS S3 signature
 * @param signature buffer to store the signature
 * @param signature_length length of signature buffer
 * @param string_to_sign
 * @param aws_secret_access_key secret access key
 * @return the signature buffer or NULL if missing input
 */
static char *aws_s3_signature(char *signature, int signature_length, const char *string_to_sign, const char *aws_secret_access_key)
{
#if defined(HAVE_OPENSSL)
	unsigned int signature_raw_length = SHA1_LENGTH;
	char signature_raw[SHA1_LENGTH];
	signature_raw[0] = '\0';
	if (!signature || signature_length <= 0) {
		return NULL;
	}
	if (zstr(aws_secret_access_key)) {
		return NULL;
	}
	if (!string_to_sign) {
		string_to_sign = "";
	}
	HMAC(EVP_sha1(),
		 aws_secret_access_key,
		 strlen(aws_secret_access_key),
		 (const unsigned char *)string_to_sign,
		 strlen(string_to_sign),
		 (unsigned char *)signature_raw,
		 &signature_raw_length);

	/* convert result to base64 */
	switch_b64_encode((unsigned char *)signature_raw, signature_raw_length, (unsigned char *)signature, signature_length);
#endif
	return signature;
}

/**
 * Create a pre-signed URL for AWS S3
 * @param verb (PUT/GET)
 * @param url address (virtual-host-style)
 * @param base_domain (optional - amazon aws assumed if not specified)
 * @param content_type optional content type
 * @param content_md5 optional content MD5 checksum
 * @param aws_access_key_id secret access key identifier
 * @param aws_secret_access_key secret access key
 * @param expires seconds since the epoch
 * @return presigned_url
 */
SWITCH_MOD_DECLARE(char *) aws_s3_presigned_url_create(const char *verb, const char *url, const char *base_domain, const char *content_type, const char *content_md5, const char *aws_access_key_id, const char *aws_secret_access_key, const char *expires)
{
	char signature[S3_SIGNATURE_LENGTH_MAX];
	char signature_url_encoded[S3_SIGNATURE_LENGTH_MAX];
	char *string_to_sign;
	char *url_dup = strdup(url);
	char *bucket;
	char *object;

	/* create URL encoded signature */
	parse_url(url_dup, base_domain, "s3", &bucket, &object);
	string_to_sign = aws_s3_string_to_sign(verb, bucket, object, content_type, content_md5, expires);
	signature[0] = '\0';
	aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, string_to_sign, aws_secret_access_key);
	switch_url_encode(signature, signature_url_encoded, S3_SIGNATURE_LENGTH_MAX);
	free(string_to_sign);
	free(url_dup);

	/* create the presigned URL */
	return switch_mprintf("%s?Signature=%s&Expires=%s&AWSAccessKeyId=%s", url, signature_url_encoded, expires, aws_access_key_id);
}

/**
 * Create an authentication signature for AWS S3
 * @param authentication buffer to store result
 * @param authentication_length maximum result length
 * @param verb (PUT/GET)
 * @param url address (virtual-host-style)
 * @param base_domain (optional - amazon aws assumed if not specified)
 * @param content_type optional content type
 * @param content_md5 optional content MD5 checksum
 * @param aws_access_key_id secret access key identifier
 * @param aws_secret_access_key secret access key
 * @param date header
 * @return signature for Authorization header
 */
static char *aws_s3_authentication_create(const char *verb, const char *url, const char *base_domain, const char *content_type, const char *content_md5, const char *aws_access_key_id, const char *aws_secret_access_key, const char *date)
{
	char signature[S3_SIGNATURE_LENGTH_MAX];
	char *string_to_sign;
	char *url_dup = strdup(url);
	char *bucket;
	char *object;

	/* create base64 encoded signature */
	parse_url(url_dup, base_domain, "s3", &bucket, &object);
	string_to_sign = aws_s3_string_to_sign(verb, bucket, object, content_type, content_md5, date);
	signature[0] = '\0';
	aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, string_to_sign, aws_secret_access_key);
	free(string_to_sign);
	free(url_dup);

	return switch_mprintf("AWS %s:%s", aws_access_key_id, signature);
}

SWITCH_MOD_DECLARE(switch_status_t) aws_s3_config_profile(switch_xml_t xml, http_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t base_domain_xml = switch_xml_child(xml, "base-domain");

	profile->append_headers_ptr = aws_s3_append_headers;

	/* check if environment variables set the keys */
	profile->aws_s3_access_key_id = getenv("AWS_ACCESS_KEY_ID");
	profile->secret_access_key = getenv("AWS_SECRET_ACCESS_KEY");
	if (!zstr(profile->aws_s3_access_key_id) && !zstr(profile->secret_access_key)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						  "Using AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY environment variables for s3 access on profile \"%s\"\n", profile->name);
		profile->aws_s3_access_key_id = strdup(profile->aws_s3_access_key_id);
		profile->secret_access_key = strdup(profile->secret_access_key);
	} else {
		/* use configuration for keys */
		switch_xml_t id = switch_xml_child(xml, "access-key-id");
		switch_xml_t secret = switch_xml_child(xml, "secret-access-key");

		if (id && secret) {
			profile->aws_s3_access_key_id = switch_strip_whitespace(switch_xml_txt(id));
			profile->secret_access_key = switch_strip_whitespace(switch_xml_txt(secret));
			if (zstr(profile->aws_s3_access_key_id) || zstr(profile->secret_access_key)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing AWS S3 credentials for profile \"%s\"\n", profile->name);
				switch_safe_free(profile->aws_s3_access_key_id);
				profile->aws_s3_access_key_id = NULL;
				switch_safe_free(profile->secret_access_key);
				profile->secret_access_key = NULL;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing key id or secret\n");
			status = SWITCH_STATUS_FALSE;
		}
	}

	if (base_domain_xml) {
		profile->base_domain = switch_strip_whitespace(switch_xml_txt(base_domain_xml));
		if (zstr(profile->base_domain)) {
			switch_safe_free(profile->base_domain);
			profile->base_domain = NULL;
		}
	}
	return status;
}

/**
 * Append Amazon S3 headers to request if necessary
 * @param headers to add to.  If NULL, new headers are created.
 * @param profile with S3 credentials
 * @param content_type of object (PUT only)
 * @param verb (GET/PUT)
 * @param url
 * @return updated headers
 */
SWITCH_MOD_DECLARE(switch_curl_slist_t*) aws_s3_append_headers(http_profile_t *profile, switch_curl_slist_t *headers,
		const char *verb, unsigned int content_length, const char *content_type, const char *url, const unsigned int block_num, char **query_string)
{
	char date[256];
	char header[1024];
	char *authenticate;

	/* Date: */
	switch_rfc822_date(date, switch_time_now());
	snprintf(header, 1024, "Date: %s", date);
	headers = switch_curl_slist_append(headers, header);

	/* Authorization: */
	authenticate = aws_s3_authentication_create(verb, url, profile->base_domain, content_type, "", profile->aws_s3_access_key_id, profile->secret_access_key, date);
	snprintf(header, 1024, "Authorization: %s", authenticate);
	free(authenticate);
	headers = switch_curl_slist_append(headers, header);

	return headers;
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
