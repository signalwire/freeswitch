/*
 * gcs.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * The Original Code is gcs.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * gcs.c -- Some GCS Blob Service helper functions
 *
 */
#include "gcs.h"
#include <switch.h>
#include <switch_curl.h>

#if defined(HAVE_OPENSSL)
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#endif

struct http_data {
	switch_stream_handle_t stream;
	switch_size_t bytes;
	switch_size_t max_bytes;
	int err;
};

// Maybe use switch_mprintf for token & payload
#if defined(HAVE_OPENSSL)
char *encoded_token(const char *token_uri, const char *client_email, const char *private_key_id, int *token_length) {
    time_t now = time(NULL);
    time_t then = now  + 3600;
    int tlength = 1 + snprintf(NULL, 0, "{\"typ\":\"JWT\",\"alg\":\"RS256\",\"kid\":\"%s\"}", private_key_id);
    int payload_length = 1 + snprintf(NULL, 0, "{\"iat\":\"%ld\",\"exp\":\"%ld\",\"iss\":\"%s\",\"aud\":\"%s\",\"scope\":\"https://www.googleapis.com/auth/devstorage.full_control https://www.googleapis.com/auth/devstorage.read_only https://www.googleapis.com/auth/devstorage.read_write\"}", now, then, client_email,token_uri);
    char token[tlength];
    char payload[payload_length];
    int encoded_tlength = tlength * 4 / 3 + (tlength % 3 ? 1 : 0); // Maybe make function
    int encoded_playload_length = payload_length * 4 / 3 + (payload_length % 3 ? 1 : 0);
    char *tokenb64 = malloc(encoded_tlength * sizeof(char));
    char *payloadb64 = malloc(encoded_playload_length* sizeof(char));
    int signee_length = encoded_tlength + encoded_playload_length;
    char *signee = malloc((signee_length) * sizeof(char));
    sprintf(token,"{\"typ\":\"JWT\",\"alg\":\"RS256\",\"kid\":\"%s\"}", private_key_id);
    sprintf(payload, "{\"iat\":\"%ld\",\"exp\":\"%ld\",\"iss\":\"%s\",\"aud\":\"%s\",\"scope\":\"https://www.googleapis.com/auth/devstorage.full_control https://www.googleapis.com/auth/devstorage.read_only https://www.googleapis.com/auth/devstorage.read_write\"}", now, then, client_email,token_uri);
    *token_length = signee_length - 1;
    switch_b64_encode((unsigned char *) token,sizeof(token), (unsigned char *) tokenb64, encoded_tlength);
    switch_b64_encode((unsigned char *) payload,sizeof(payload), (unsigned char *) payloadb64, encoded_playload_length);
    sprintf(signee, "%s.%s", tokenb64, payloadb64);
    free(tokenb64);
    free(payloadb64);
    return signee;
}

void signtoken(char *token, int tokenlen,char *pkey, char *out) {
    unsigned char *sig = NULL;
    BIO *b = NULL;
    RSA *r = NULL;
    unsigned int sig_len;
    unsigned char *digest = SHA256((const unsigned char *) token, tokenlen, NULL);
    b = BIO_new_mem_buf(pkey, -1);
    r = PEM_read_bio_RSAPrivateKey(b, NULL, NULL, NULL);
    BIO_set_close(b, BIO_CLOSE);
    BIO_free(b);
    sig = malloc(RSA_size(r));
    RSA_sign(NID_sha256, digest, SHA256_DIGEST_LENGTH, sig, &sig_len, r);
    switch_b64_encode(sig,(switch_size_t) sizeof(sig) * sig_len,(unsigned char *) out, 343 * sizeof(char));
    free(sig);
}

char *gcs_auth_request(char *content, char *url);
switch_status_t gcs_refresh_authorization (http_profile_t *profile)
{
	int token_length;
	char *token = NULL;
	char *encoded = NULL;
	char *assertion = NULL;
	char *auth = NULL;
	char content[GCS_SIGNATURE_LENGTH_MAX];
	char *signature_url_encoded = NULL;
	time_t exp;
	token = encoded_token(profile->region, profile->gcs_email, profile->aws_s3_access_key_id, &token_length);
	encoded = malloc(sizeof(char) * 343);
	signtoken(token, token_length, profile->secret_access_key, encoded);
    assertion = malloc(sizeof(char) * (1 + token_length + 343));
	sprintf(assertion, "%s.%s", token, encoded);
	free(token);
	signature_url_encoded = switch_string_replace(assertion, "+", "%2B");
    sprintf(content,"%s%s", "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=", signature_url_encoded);
	auth = gcs_auth_request(content, profile->region);
	if (profile->gcs_credentials != NULL) {
		free(profile->gcs_credentials);
	}
	profile->gcs_credentials = auth;
	exp = time(NULL) + 3540;
	profile->expires = exp;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Credecials Expries Unix Time: %ld", exp);
	free(assertion);
	return SWITCH_STATUS_SUCCESS;
}
#endif

/**
 * Append the specific GCS Blob Service headers
 * @param http_profile_t the provile
 * @param headers the list of headers to append to
 */

switch_curl_slist_t *gcs_append_headers(http_profile_t *profile, switch_curl_slist_t *headers, const char *verb,unsigned int content_length, const char *content_type, const char *url, const unsigned int block_num, char **query_string)
{
	char header[1024]; // I think we can get away with a smaller number like 256 or even 224
#if defined(HAVE_OPENSSL)
	switch_time_t now = time(NULL);
	//gcs_refresh_authorization(http_profile_t *profile);
	if (profile->expires < now) {
		gcs_refresh_authorization(profile);
	}
	switch_snprintf(header, sizeof(header), "Authorization: Bearer %s", profile->gcs_credentials);
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Credecials Token: %s", profile->gcs_credentials);
	headers = switch_curl_slist_append(headers, header);
#endif
	return headers;
}

/**
 * Read the GCS Blob Service profile
 * @param name the name of the profile
 * @param xml the portion of the XML document containing the profile
 * @param access_key_id returned value of access_key_id in the configuration
 * @param secret_access_key returned value of secret_access_key in the configuration
 * @param base_domain returned value of base_domain in the configuration
 * @param bytes_per_block returned value of bytes_per_block in the configuration
 * @return SWITCH_STATUS_SUCCESS on success
 */
switch_status_t gcs_config_profile(switch_xml_t xml, http_profile_t *profile,switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
#if defined(HAVE_OPENSSL)
	char *file = NULL;
	switch_xml_t base_domain_xml = switch_xml_child(xml, "base-domain");
	profile->append_headers_ptr = gcs_append_headers;

	/* check if environment variables set the keys */
	file = getenv("GOOGLE_APPLICATION_CREDENTIALS");
	if (!zstr(file)) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		//				  "Using GOOGLE_APPLICATION_CREDENTIALS environment variables for GCS access on profile \"%s\"\n", profile->name);
	} else {
		/* use configuration for keys */
		file = switch_strip_whitespace(switch_xml_txt(switch_xml_child(xml, "credential_file")));
	}
//	if (!zstr(file)) {}
	if (switch_file_exists(file, pool) == SWITCH_STATUS_SUCCESS){
		char *contents = NULL;
		char *jsonstr = NULL;
		switch_file_t *fd;
		switch_status_t status;
		switch_size_t size;
		cJSON *json = {0};

		status = switch_file_open(&fd, file, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, pool);
		if (status != SWITCH_STATUS_SUCCESS) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERR, "Could not open credencial file\n", profile->bytes_per_block);
			return status;
		}

		size = switch_file_get_size(fd);
		if (size) {
			contents = malloc(size);
			//contents = switch_core_sprintf(pool, "%" SWITCH_SIZE_T_FMT, size);
			switch_file_read(fd, (void *) contents, &size);
		}
		else {
			return SWITCH_STATUS_FALSE;
		}

		status = switch_file_close(fd);
		if (status != SWITCH_STATUS_SUCCESS) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERR, "Could not close credencial file\n", profile->bytes_per_block);
			return status;
		}
		json = cJSON_Parse(contents);
		// Need to allocate memory?
		jsonstr = cJSON_GetObjectItem(json,"private_key_id")->valuestring;
		profile->aws_s3_access_key_id = malloc(sizeof(char) * (1+ strlen(jsonstr)));
		strcpy(profile->aws_s3_access_key_id, jsonstr);
		jsonstr = cJSON_GetObjectItem(json,"private_key")->valuestring;
		profile->secret_access_key = malloc(sizeof(char) * (1+ strlen(jsonstr)));
		strcpy(profile->secret_access_key, jsonstr);
		jsonstr = cJSON_GetObjectItem(json,"client_email")->valuestring;
		profile->gcs_email = malloc(sizeof(char) * (1+ strlen(jsonstr)));
		strcpy(profile->gcs_email, jsonstr);
		jsonstr = cJSON_GetObjectItem(json,"token_uri")->valuestring;
		profile->region = malloc(sizeof(char) * (1+ strlen(jsonstr)));
		strcpy(profile->region, jsonstr);
		cJSON_Delete(json);
		//printf("private_key_id: %s\nprivate_key: %s\nclient_email: %s\ntoken_uri: %s\n", profile->aws_s3_access_key_id, profile->secret_access_key, profile->gcs_email, profile->region);
		free(contents);
	}
	else {
        switch_xml_t private_key = switch_xml_child(xml, "private_key");
        switch_xml_t private_key_id = switch_xml_child(xml, "private_key_id");
        switch_xml_t client_email = switch_xml_child(xml, "client_email");
        switch_xml_t token_uri = switch_xml_child(xml, "token_uri");
        if (private_key_id) {
            profile->aws_s3_access_key_id = switch_strip_whitespace(switch_xml_txt(private_key_id));
        } else {
            //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing key private_key_id\n");
			return SWITCH_STATUS_FALSE;
        }
        if (private_key) {
            profile->secret_access_key = switch_xml_txt(private_key);
        } else {
            //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing key private_key\n");
			return SWITCH_STATUS_FALSE;
        }
        if (client_email) {
            profile->gcs_email = switch_strip_whitespace(switch_xml_txt(client_email));
        } else {
            //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing key client_email\n");
			return SWITCH_STATUS_FALSE;
        }
        if (token_uri) {
            profile->region = switch_strip_whitespace(switch_xml_txt(token_uri));
        } else {
            //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing key token_uri\n");
			return SWITCH_STATUS_FALSE;
        }
	}
	profile->bytes_per_block = 4e6;
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set number of bytes per block to %zu\n", profile->bytes_per_block);
	status = gcs_refresh_authorization(profile);
	if (status != SWITCH_STATUS_SUCCESS){
		return status;
	}
	
	if (base_domain_xml) {
		profile->base_domain = switch_strip_whitespace(switch_xml_txt(base_domain_xml));
		if (zstr(profile->base_domain)) {
			switch_safe_free(profile->base_domain);
		}
	}
#endif
	return status;
}

// This should probably get length and whatnot but I don't think googles response will be large
/*
*/
#if defined(HAVE_OPENSSL)
static size_t gcs_auth_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
    register unsigned int realsize = (unsigned int) (size * nmemb);
    struct http_data *http_data = data;

    http_data->bytes += realsize;

    if (http_data->bytes > http_data->max_bytes) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Oversized file detected [%d bytes]\n", (int) http_data->bytes);
        http_data->err = 1;
        return 0;
    }

	printf("ptr: \n%s\n", (char *) ptr);
    http_data->stream.write_function(&http_data->stream, "%.*s", realsize, ptr);
    return realsize;
}

char *gcs_auth_request(char *content, char *url) {
	switch_CURL *curl_handle = NULL;
	long httpRes = 0;
	//char response[208];
	char *response = NULL;
	switch_curl_slist_t *headers = NULL;
	char *ct = "Content-Type: application/x-www-form-urlencoded";
    struct http_data http_data;
	CURLcode res;

    memset(&http_data, 0, sizeof(http_data));
    http_data.max_bytes = 10240;
    SWITCH_STANDARD_STREAM(http_data.stream);

	curl_handle = switch_curl_easy_init();
    switch_curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, content);
	switch_curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5);
	switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10);
//    switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
//    switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 15);

	headers = switch_curl_slist_append(headers, ct);
    switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-curl/1.0");
	switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
   	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, gcs_auth_callback);
	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&http_data);

	res = switch_curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    if (http_data.stream.data && !zstr((char *) http_data.stream.data) && strcmp(" ", http_data.stream.data)) {
		cJSON *json = {0};
		char *jsonstr;
		json = cJSON_Parse(http_data.stream.data);
		jsonstr = cJSON_GetObjectItem(json,"access_token")->valuestring;
		response = malloc(sizeof(char) * (1+strlen(jsonstr)));
		strcpy(response, jsonstr);
		cJSON_Delete(json);
		// Hack but works
		//strncpy(response, (char *) http_data.stream.data + 17, 207);
    }
	//free(http_data);
	
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
		  switch_curl_easy_strerror(res));
	return response;
}
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
