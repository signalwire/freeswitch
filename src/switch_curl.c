#include <switch.h>
#include "switch_curl.h"
#include <curl/curl.h>

SWITCH_DECLARE(switch_CURL *) switch_curl_easy_init(void)
{
	return curl_easy_init();
}

SWITCH_DECLARE(switch_CURLcode) switch_curl_easy_perform(switch_CURL *handle)
{
	return curl_easy_perform((CURL *)handle);
}


SWITCH_DECLARE(switch_CURLcode) switch_curl_easy_getinfo(switch_CURL *curl, switch_CURLINFO info, ... )
{
	va_list ap;
	switch_CURLcode code;

	va_start(ap, info);
	code = curl_easy_getinfo(curl, info, va_arg(ap, void *));
	va_end(ap);

	return code;
}

SWITCH_DECLARE(void) switch_curl_easy_cleanup(switch_CURL *handle)
{
	curl_easy_cleanup((CURL *)handle);
}



SWITCH_DECLARE(switch_curl_slist_t *) switch_curl_slist_append(switch_curl_slist_t * list, const char * string )
{
	return (switch_curl_slist_t *) curl_slist_append((struct curl_slist *)list, string);
}


SWITCH_DECLARE(void) switch_curl_slist_free_all(switch_curl_slist_t * list)
{
	curl_slist_free_all((struct curl_slist *) list);
}

SWITCH_DECLARE(const char *) switch_curl_easy_strerror(switch_CURLcode errornum )
{
	return curl_easy_strerror(errornum);
}

SWITCH_DECLARE(void) switch_curl_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
}

SWITCH_DECLARE(void) switch_curl_destroy(void)
{
	curl_global_cleanup();
}

SWITCH_DECLARE(switch_status_t) switch_curl_process_mime(switch_event_t *event, switch_CURL *curl_handle, switch_curl_mime **mimep)
{
#if defined(LIBCURL_VERSION_NUM) && (LIBCURL_VERSION_NUM >= 0x073800)
	curl_mime *mime = NULL;
	curl_mimepart *part = NULL;
	uint8_t added = 0;
	switch_CURLcode curl_code = CURLE_OK;
#else
	struct curl_httppost *formpost=NULL;
	struct curl_httppost *lastptr=NULL;
#endif
	switch_event_header_t *hp;
	int go = 0;

	for (hp = event->headers; hp; hp = hp->next) {
		if (!strncasecmp(hp->name, "attach_file:", 12)) {
			go = 1;
			break;
		}
	}

	if (!go) {
		return SWITCH_STATUS_FALSE;
	}

#if defined(LIBCURL_VERSION_NUM) && (LIBCURL_VERSION_NUM >= 0x073800)
	mime = curl_mime_init(curl_handle);
#endif

	for (hp = event->headers; hp; hp = hp->next) {
		if (!strncasecmp(hp->name, "attach_file:", 12)) {
			char *pname = strdup(hp->name + 12);

			if (pname) {
				char *fname = strchr(pname, ':');

				if (fname) {
					*fname++ = '\0';

#if defined(LIBCURL_VERSION_NUM) && (LIBCURL_VERSION_NUM >= 0x073800)
					part = curl_mime_addpart(mime);
					if ((curl_code = curl_mime_name(part, pname))) {
						free(pname);
						goto error;
					}

					if ((curl_code = curl_mime_filename(part, fname))) {
						free(pname);
						goto error;
					}

					if ((curl_code = curl_mime_filedata(part, hp->value))) {
						free(pname);
						goto error;
					}

					added++;
#else
					curl_formadd(&formpost,
								 &lastptr,
								 CURLFORM_COPYNAME, pname,
								 CURLFORM_FILENAME, fname,
								 CURLFORM_FILE, hp->value,
								 CURLFORM_END);
#endif
				}

				free(pname);
			}
		} else {
#if defined(LIBCURL_VERSION_NUM) && (LIBCURL_VERSION_NUM >= 0x073800)
			part = curl_mime_addpart(mime);
			if ((curl_code = curl_mime_name(part, hp->name))) {
				goto error;
			}

			if ((curl_code = curl_mime_data(part, hp->value, CURL_ZERO_TERMINATED))) {
				goto error;
			}

			added++;
#else
			curl_formadd(&formpost,
						 &lastptr,
						 CURLFORM_COPYNAME, hp->name,
						 CURLFORM_COPYCONTENTS, hp->value,
						 CURLFORM_END);
#endif
		}
	}

#if defined(LIBCURL_VERSION_NUM) && (LIBCURL_VERSION_NUM >= 0x073800)
 error:
	if (curl_code) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CURL error occured. Error code: %d Error msg: [%s]\n", curl_code, switch_curl_easy_strerror(curl_code));
	}

	if (!added) {
		curl_mime_free(mime);
		mime = NULL;
	}

	*mimep = mime;
#else
	*mimep = formpost;
#endif

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_curl_mime_free(switch_curl_mime **mimep)
{
	if (mimep && *mimep) {
#if defined(LIBCURL_VERSION_NUM) && (LIBCURL_VERSION_NUM >= 0x073800)
		curl_mime_free(*mimep);
#else
		curl_formfree(*mimep);
#endif
		mimep = NULL;
	}
}

SWITCH_DECLARE(switch_CURLcode) switch_curl_easy_setopt_mime(switch_CURL *curl_handle, switch_curl_mime *mime)
{
#if defined(LIBCURL_VERSION_NUM) && (LIBCURL_VERSION_NUM >= 0x073800)
	return curl_easy_setopt(curl_handle, CURLOPT_MIMEPOST, mime);
#else
	return curl_easy_setopt(curl_handle, CURLOPT_HTTPPOST, mime);
#endif
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
