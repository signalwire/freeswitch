/*
  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/
#include "switch.h"
#ifndef SWITCH_JSON__h
#define SWITCH_JSON__h

#ifdef FREESWITCHCORE_EXPORTS
#ifndef CJSON_EXPORT_SYMBOLS
#define CJSON_EXPORT_SYMBOLS 1
#endif
#endif

#ifdef SWITCH_API_VISIBILITY
#ifndef CJSON_API_VISIBILITY
#define CJSON_API_VISIBILITY 1
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include "switch_cJSON.h"
#include "switch_cJSON_Utils.h"
	
SWITCH_DECLARE(cJSON *) cJSON_CreateStringPrintf(const char *fmt, ...);
SWITCH_DECLARE(const char *)cJSON_GetObjectCstr(const cJSON *object, const char *string);

static inline cJSON *json_add_child_obj(cJSON *json, const char *name, cJSON *obj)
{
	cJSON *new_json = NULL;

	switch_assert(json);

	if (obj) {
		new_json = obj;
	} else {
		new_json = cJSON_CreateObject();
	}

	switch_assert(new_json);

	cJSON_AddItemToObject(json, name, new_json);

	return new_json;
}

static inline cJSON *json_add_child_array(cJSON *json, const char *name)
{
	cJSON *new_json = NULL;

	switch_assert(json);

	new_json = cJSON_CreateArray();
	switch_assert(new_json);

	cJSON_AddItemToObject(json, name, new_json);

	return new_json;
}

static inline cJSON *json_add_child_string(cJSON *json, const char *name, const char *val)
{
	cJSON *new_json = NULL;

	switch_assert(json);

	new_json = cJSON_CreateString(val);
	switch_assert(new_json);

	cJSON_AddItemToObject(json, name, new_json);

	return new_json;
}

static inline int cJSON_isTrue(cJSON *json)
{
	if (!json) return 0;

	if (json->type == cJSON_True) return 1;

	if (json->type == cJSON_String && (
		!strcasecmp(json->valuestring, "yes") ||
		!strcasecmp(json->valuestring, "on") ||
		!strcasecmp(json->valuestring, "true") ||
		!strcasecmp(json->valuestring, "t") ||
		!strcasecmp(json->valuestring, "enabled") ||
		!strcasecmp(json->valuestring, "active") ||
		!strcasecmp(json->valuestring, "allow") ||
		atoi(json->valuestring))) {
		return 1;
	}

	if (json->type == cJSON_Number && (json->valueint || json->valuedouble)) {
		return 1;
	}

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
