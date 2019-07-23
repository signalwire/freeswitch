#ifndef COMMON_H
#define COMMON_H

#include <switch.h>

/**
 * An http profile.  Defines optional credentials
 * for access to Amazon S3 and Azure Blob Service
 */
struct http_profile {
	const char *name;
	char *aws_s3_access_key_id;
	char *secret_access_key;
	char *base_domain;
	switch_size_t bytes_per_block;

	// function to be called to add the profile specific headers to the GET/PUT requests
	switch_curl_slist_t *(*append_headers_ptr)(struct http_profile *profile, switch_curl_slist_t *headers,
		const char *verb, unsigned int content_length, const char *content_type, const char *url, const unsigned int block_num, char **query_string);
	// function to be called to perform the profile-specific actions at the end of the PUT operation
	switch_status_t (*finalise_put_ptr)(struct http_profile *profile, const char *url, const unsigned int num_blocks);
};
typedef struct http_profile http_profile_t;


SWITCH_MOD_DECLARE(void) parse_url(char *url, const char *base_domain, const char *default_base_domain, char **bucket, char **object);

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
