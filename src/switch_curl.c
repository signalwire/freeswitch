#include <switch.h>
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
	code = curl_easy_getinfo(curl, info, ap);
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

SWITCH_DECLARE(switch_CURLcode) switch_curl_easy_setopt(CURL *handle, switch_CURLoption option, ...)
{
	va_list ap;
	switch_CURLcode code;

	va_start(ap, option);
	code = curl_easy_setopt(handle, option, ap);
	va_end(ap);

	return code;
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

