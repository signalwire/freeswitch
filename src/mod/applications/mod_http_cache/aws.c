/*
 * aws.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013, Grasshopper
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
 * @param url to check
 * @return true if this is an S3 url
 */
int aws_s3_is_s3_url(const char *url)
{
	/* AWS bucket naming rules are complex... this match only supports virtual hosting of buckets */
	return !zstr(url) && switch_regex_match(url, "^https?://[a-z0-9][-a-z0-9.]{1,61}[a-z0-9]\\.s3\\.amazonaws\\.com/.*$") == SWITCH_STATUS_SUCCESS;
}

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
char *aws_s3_string_to_sign(const char *verb, const char *bucket, const char *object, const char *content_type, const char *content_md5, const char *date)
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
char *aws_s3_signature(char *signature, int signature_length, const char *string_to_sign, const char *aws_secret_access_key)
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
 * Parse bucket and object from URL
 * @param url to parse.  This value is modified.
 * @param bucket to store result in
 * @param bucket_length of result buffer
 */
void aws_s3_parse_url(char *url, char **bucket, char **object)
{
	char *bucket_start;
	char *bucket_end;
	char *object_start;

	*bucket = NULL;
	*object = NULL;

	if (!aws_s3_is_s3_url(url)) {
		return;
	}

	/* expect: http(s)://bucket.s3.amazonaws.com/object */
 	bucket_start = strstr(url, "://");
	if (!bucket_start) {
		/* invalid URL */
		return;
	}
	bucket_start += 3;

	bucket_end = strchr(bucket_start, '.');
	if (!bucket_end) {
		/* invalid URL */
		return;
	}
	*bucket_end = '\0';

	object_start = strchr(bucket_end + 1, '/');
	if (!object_start) {
		/* invalid URL */
		return;
	}
	object_start++;

	if (strchr(object_start, '/')) {
		/* invalid URL */
		return;
	}

	if (zstr(bucket_start) || zstr(object_start)) {
		/* invalid URL */
		return;
	}

	*bucket = bucket_start;
	*object = object_start;
}

/**
 * Create a pre-signed URL for AWS S3
 * @param verb (PUT/GET)
 * @param url address (virtual-host-style)
 * @param content_type optional content type
 * @param content_md5 optional content MD5 checksum
 * @param aws_access_key_id secret access key identifier
 * @param aws_secret_access_key secret access key
 * @param expires seconds since the epoch
 * @return presigned_url
 */
char *aws_s3_presigned_url_create(const char *verb, const char *url, const char *content_type, const char *content_md5, const char *aws_access_key_id, const char *aws_secret_access_key, const char *expires)
{
	char signature[S3_SIGNATURE_LENGTH_MAX];
	char signature_url_encoded[S3_SIGNATURE_LENGTH_MAX];
	char *string_to_sign;
	char *url_dup = strdup(url);
	char *bucket;
	char *object;

	/* create URL encoded signature */
	aws_s3_parse_url(url_dup, &bucket, &object);
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
 * @param content_type optional content type
 * @param content_md5 optional content MD5 checksum
 * @param aws_access_key_id secret access key identifier
 * @param aws_secret_access_key secret access key
 * @param date header
 * @return signature for Authorization header
 */
char *aws_s3_authentication_create(const char *verb, const char *url, const char *content_type, const char *content_md5, const char *aws_access_key_id, const char *aws_secret_access_key, const char *date)
{
	char signature[S3_SIGNATURE_LENGTH_MAX];
	char *string_to_sign;
	char *url_dup = strdup(url);
	char *bucket;
	char *object;

	/* create base64 encoded signature */
	aws_s3_parse_url(url_dup, &bucket, &object);
	string_to_sign = aws_s3_string_to_sign(verb, bucket, object, content_type, content_md5, date);
	signature[0] = '\0';
	aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, string_to_sign, aws_secret_access_key);
	free(string_to_sign);
	free(url_dup);

	return switch_mprintf("AWS %s:%s", aws_access_key_id, signature);
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
