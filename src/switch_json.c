#include "switch.h"

SWITCH_DECLARE(cJSON *) cJSON_CreateStringPrintf(const char *fmt, ...)
{
	va_list ap;
	char *str;
	cJSON *item;

	va_start(ap, fmt);
	str = switch_vmprintf(fmt, ap);
	va_end(ap);

	if (!str) return NULL;

	item = cJSON_CreateString(str);

	free(str);

	return item;
}

SWITCH_DECLARE(const char *)cJSON_GetObjectCstr(const cJSON *object, const char *string)
{
       cJSON *cj = cJSON_GetObjectItem(object, string);

	   if (!cj || cj->type != cJSON_String || !cj->valuestring) return NULL;

	   return cj->valuestring;
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
