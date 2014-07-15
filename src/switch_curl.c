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

SWITCH_DECLARE(switch_status_t) switch_curl_process_form_post_params(switch_event_t *event, switch_CURL *curl_handle, struct curl_httppost **formpostp)
{

	struct curl_httppost *formpost=NULL;
	struct curl_httppost *lastptr=NULL;
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

	for (hp = event->headers; hp; hp = hp->next) {

		if (!strncasecmp(hp->name, "attach_file:", 12)) {
			char *pname = strdup(hp->name + 12);
			
			if (pname) {
				char *fname = strchr(pname, ':');
				if (fname) {
					*fname++ = '\0';

					curl_formadd(&formpost,
								 &lastptr,
								 CURLFORM_COPYNAME, pname,
								 CURLFORM_FILENAME, fname,
								 CURLFORM_FILE, hp->value,
								 CURLFORM_END);
				}
				free(pname);
			}
		} else {
			curl_formadd(&formpost,
						 &lastptr,
						 CURLFORM_COPYNAME, hp->name,
						 CURLFORM_COPYCONTENTS, hp->value,
						 CURLFORM_END);

		}
	}

	*formpostp = formpost;

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
