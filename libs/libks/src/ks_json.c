#include "ks.h"

KS_DECLARE(cJSON *) cJSON_CreateStringPrintf(const char *fmt, ...)
{
	va_list ap;
	char *str;
	cJSON *item;

	va_start(ap, fmt);
	str = ks_vmprintf(fmt, ap);
	va_end(ap);

	if (!str) return NULL;

	item = cJSON_CreateString(str);

	free(str);

	return item;
}

KS_DECLARE(const char *) cJSON_GetObjectCstr(const cJSON *object, const char *string)
{
       cJSON *cj = cJSON_GetObjectItem(object, string);

	   if (!cj || cj->type != cJSON_String || !cj->valuestring) return NULL;

	   return cj->valuestring;
}

KS_DECLARE(cJSON *) cJSON_CreatePtr(uintptr_t pointer)
{
	// @todo check for 32bit and use integer storage instead
	return cJSON_CreateStringPrintf("%p", (void *)pointer);
}

KS_DECLARE(uintptr_t) cJSON_GetPtrValue(const cJSON *object)
{
	// @todo check for 32bit and use integer storage instead
	void *pointer = NULL;
	if (object && object->type == cJSON_String) sscanf_s(object->valuestring, "%p", &pointer);
	return (uintptr_t)pointer;
}

KS_DECLARE(uintptr_t) cJSON_GetObjectPtr(const cJSON *object, const char *string)
{
	return cJSON_GetPtrValue(cJSON_GetObjectItem(object, string));
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
