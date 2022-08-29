/*
 * Copyright 2008-2015 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apt_test_suite.h"
#include "apt_multipart_content.h"
#include "apt_log.h"

static apt_str_t* multipart_content_generate(apt_test_suite_t *suite)
{
	apt_multipart_content_t *multipart = apt_multipart_content_create(1500,NULL,suite->pool);
	apt_str_t content_type;
	apt_str_t content;
	apt_str_t *body;

	apt_string_set(&content_type,"text/plain");
	apt_string_set(&content,"This is the content of the first part");
	apt_multipart_content_add2(multipart,&content_type,NULL,&content);

	apt_string_set(&content_type,"application/ssml+xml");
	apt_string_set(&content,
		"<?xml version=\"1.0\"?>\r\n"
		"<speak version=\"1.0\"\r\n"
		"<p> <s>You have 4 new messages.</s> </p>\r\n"
		"</speak>");
	apt_multipart_content_add2(multipart,&content_type,NULL,&content);

	body = apt_multipart_content_finalize(multipart);
	if(body) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Genereted Multipart Content [%lu bytes]\n%s",
			body->length,
			body->buf);
	}
	return body;
}

static apt_bool_t multipart_content_parse(apt_test_suite_t *suite, apt_str_t *body)
{
	apt_multipart_content_t *multipart = apt_multipart_content_assign(body,NULL,suite->pool);
	if(multipart) {
		apt_bool_t is_final;
		apt_content_part_t content_part;
		while(apt_multipart_content_get(multipart,&content_part,&is_final) == TRUE) {
			if(is_final == TRUE) {
				break;
			}
			if(content_part.type && apt_string_is_empty(content_part.type) == FALSE) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Content Part Type: %.*s",
					content_part.type->length,
					content_part.type->buf);
			}
			if(content_part.id && apt_string_is_empty(content_part.id) == FALSE) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Content Part Id: %.*s",
					content_part.id->length,
					content_part.id->buf);
			}
			if(content_part.length && apt_string_is_empty(content_part.length) == FALSE) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Content Part Length: %.*s\n%.*s",
					content_part.length->length,
					content_part.length->buf,
					content_part.body.length,
					content_part.body.buf);
			}
		}
	}
	return TRUE;
}


static apt_bool_t multipart_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	apt_bool_t status = FALSE;
	apt_str_t *body = multipart_content_generate(suite);
	if(body) {
		status = multipart_content_parse(suite,body);
	}
	return status;
}

apt_test_suite_t* multipart_test_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"multipart",NULL,multipart_test_run);
	return suite;
}
