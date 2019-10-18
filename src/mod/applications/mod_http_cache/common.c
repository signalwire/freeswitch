/*
 * common.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * The Original Code is common.c for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 * Quoc-Bao Nguyen <baonq5@vng.com.vn>
 * 
 * common.c - Functions common to the store provider
 *
 */

#include <switch.h>
#include <switch_utils.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>


/**
 * Create a folder if it is not exists. Equivalent to 'mkdir -p'
 * @param path
 */
SWITCH_MOD_DECLARE(void) recursive_mkdir(char *path)
{
    mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; //S_IRUSR | S_IWUSR;        // 600
    struct stat sb;

    char *sep = strrchr(path, '/' );
    if (sep != NULL) {
        *sep = '\0';
        recursive_mkdir(path);
        *sep = '/';
    }

    if (stat(path, &sb) != 0 || !S_ISDIR(sb.st_mode))
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Making folder: %s\n", path);
        mkdir(path, mode);
    }
}


static int recursive_open(const char *path, int oflag, mode_t mode)
{
    char* path_dup;

    char *sep = strrchr(path, '/' );
    if (sep) {
        switch_strdup(path_dup, path);
        path_dup[sep - path] = 0;
        recursive_mkdir(path_dup);
        switch_safe_free(path_dup);
    }

    return open(path, oflag, mode);
}


/**
 * Backup file to local storage. Used with http_put in case of failed
 * @param source
 * @param dest
 * @return
 */
SWITCH_MOD_DECLARE(int) backup_file(const char* source, const char* dest)
{
    int source_fd, dest_fd;
    int num_read;
    mode_t dest_fd_mode;
    char cwd[128];
    const unsigned int BUFF_SIZE = 65536;
    char* buffer;
    int return_code = 0;

    getcwd(cwd, sizeof(cwd));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Current working dir: %s\n", cwd);

    source_fd = open(source, O_RDONLY);
    if (source_fd < 0)
    {
        return errno;
    }

    dest_fd_mode = S_IRWXU | S_IRWXG | S_IRWXO; //S_IRUSR | S_IWUSR;        // 600
    dest_fd = recursive_open(dest, O_CREAT | O_WRONLY | O_TRUNC, dest_fd_mode);        // will overwrite existing file
    if (dest_fd < 0)
    {
        close(source_fd);
        return errno;
    }

    switch_malloc(buffer, BUFF_SIZE);
    while ((num_read = read(source_fd, buffer, BUFF_SIZE)) > 0)
    {
        write(dest_fd, buffer, num_read);
    }

    if (num_read < 0)
    {
        return_code = errno;
    }

    close(source_fd);
    close(dest_fd);
    switch_safe_free(buffer);

    return return_code;
}


/**
 * Calculate HMAC-SHA256 hash of a message
 * @param buffer buffer to store the HMAC-SHA256 version of message as byte array
 * @param buffer_length length of buffer
 * @param key buffer that store the key to run HMAC-SHA256
 * @param key_length length of the key
 * @param message message that will be hashed
 * @return byte array, equals to buffer
 */
SWITCH_MOD_DECLARE(char*) hmac256(char* buffer, unsigned int buffer_length, const char* key, unsigned int key_length, const char* message)
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
SWITCH_MOD_DECLARE(char*) hmac256_hex(char* buffer, const char* key, unsigned int key_length, const char* message)
{
    char hmac256_raw[SHA256_DIGEST_LENGTH];

    hmac256(hmac256_raw, SHA256_DIGEST_LENGTH, key, key_length, message);

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
SWITCH_MOD_DECLARE(char*) sha256_hex(char* buffer, const char* string)
{
    unsigned char sha256_raw[SHA256_DIGEST_LENGTH];

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
SWITCH_MOD_DECLARE(char*) get_time(char* format, char* buffer, unsigned int buffer_length)
{
	switch_time_exp_t time;
	switch_size_t size;

	switch_time_exp_gmt(&time, switch_time_now());

	switch_strftime(buffer, &size, buffer_length, format, &time);

	return buffer;
}

/**
* Reverse string substring search
*/
static char *my_strrstr(const char *haystack, const char *needle)
{
	char *s;
	size_t needle_len;
	size_t haystack_len;

	if (zstr(haystack)) {
		return NULL;
	}

	if (zstr(needle)) {
		return (char *)haystack;
	}

	needle_len = strlen(needle);
	haystack_len = strlen(haystack);
	if (needle_len > haystack_len) {
		return NULL;
	}

	s = (char *)(haystack + haystack_len - needle_len);
	do {
		if (!strncmp(s, needle, needle_len)) {
			return s;
		}
	} while (s-- != haystack);

	return NULL;
}

SWITCH_MOD_DECLARE(void) parse_url(char *url, const char *base_domain, const char *default_base_domain, char **bucket, char **object)
{
	char *bucket_start = NULL;
	char *bucket_end;
	char *object_start;
	char *p;
	char base_domain_match[1024];

	*bucket = NULL;
	*object = NULL;

	if (zstr(url)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "url is empty\n");
        return;
	}

	/* expect: http(s)://bucket.foo-bar.s3.amazonaws.com/object */
	if (!strncasecmp(url, "https://", 8)) {
		bucket_start = url + 8;
	} else if (!strncasecmp(url, "http://", 7)) {
		bucket_start = url + 7;
	}

	if (zstr(bucket_start)) { /* invalid URL */
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid url\n");
        return;
	}

	if (zstr(base_domain)) {
		base_domain = default_base_domain;
	}
	switch_snprintf(base_domain_match, 1024, ".%s", base_domain);
	bucket_end = my_strrstr(bucket_start, base_domain_match);

	if (!bucket_end) { /* invalid URL */
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid url\n");
        return;
	}

	*bucket_end = '\0';

	object_start = strchr(bucket_end + 1, '/');

	if (!object_start) { /* invalid URL */
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid url\n");
        return;
	}

	object_start++;

	if (zstr(bucket_start) || zstr(object_start)) { /* invalid URL */
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid url\n");
        return;
	}

	/* ignore the query string from the end of the URL */
	if ((p = strchr(object_start, '&'))) {
		*p = '\0';
	}

	*bucket = bucket_start;
	*object = object_start;
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
