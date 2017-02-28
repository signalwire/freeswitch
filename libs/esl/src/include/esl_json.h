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
#include "esl.h"
#ifndef ESL_JSON__h
#define ESL_JSON__h

#ifdef ESL_EXPORTS
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

#include "esl_cJSON.h"
#include "esl_cJSON_Utils.h"
	
ESL_DECLARE(const char *)cJSON_GetObjectCstr(const cJSON *object, const char *string);

static inline cJSON *esl_json_add_child_obj(cJSON *json, const char *name, cJSON *obj)
{
	cJSON *new_json = NULL;

	esl_assert(json);

	if (obj) {
		new_json = obj;
	} else {
		new_json = cJSON_CreateObject();
	}

	esl_assert(new_json);

	cJSON_AddItemToObject(json, name, new_json);

	return new_json;
}

static inline cJSON *esl_json_add_child_array(cJSON *json, const char *name)
{
	cJSON *new_json = NULL;

	esl_assert(json);

	new_json = cJSON_CreateArray();
	esl_assert(new_json);

	cJSON_AddItemToObject(json, name, new_json);

	return new_json;
}

static inline cJSON *esl_json_add_child_string(cJSON *json, const char *name, const char *val)
{
	cJSON *new_json = NULL;

	esl_assert(json);

	new_json = cJSON_CreateString(val);
	esl_assert(new_json);

	cJSON_AddItemToObject(json, name, new_json);

	return new_json;
}

#ifdef __cplusplus
}
#endif

#endif
