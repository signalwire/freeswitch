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

#include <apr_file_info.h>
#include <apr_file_io.h>
#include "apt_test_suite.h"
#include "apt_log.h"
#include "mrcp_resource_loader.h"
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "mrcp_stream.h"

static apt_bool_t test_stream_generate(mrcp_generator_t *generator, mrcp_message_t *message)
{
	char buffer[500];
	apt_text_stream_t stream;
	apt_message_status_e status;
	apt_bool_t continuation;

	do {
		apt_text_stream_init(&stream,buffer,sizeof(buffer)-1);
		continuation = FALSE;
		status = mrcp_generator_run(generator,message,&stream);
		if(status == APT_MESSAGE_STATUS_COMPLETE) {
			stream.text.length = stream.pos - stream.text.buf;
			*stream.pos = '\0';
			apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Generated MRCPv2 Data [%"APR_SIZE_T_FMT" bytes]\n%s",stream.text.length,stream.text.buf);
		}
		else if(status == APT_MESSAGE_STATUS_INCOMPLETE) {
			*stream.pos = '\0';
			apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Generated MRCPv2 Data [%"APR_SIZE_T_FMT" bytes] continuation awaited\n%s",stream.text.length,stream.text.buf);
			continuation = TRUE;
		}
		else if(status == APT_MESSAGE_STATUS_INVALID) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate MRCPv2 Data");
		}
	}
	while(continuation == TRUE);
	return TRUE;
}

static apt_bool_t mrcp_message_handler(mrcp_generator_t *generator, mrcp_message_t *message, apt_message_status_e status)
{
	if(status == APT_MESSAGE_STATUS_COMPLETE) {
		/* message is completely parsed */
		test_stream_generate(generator,message);
	}
	return TRUE;
}

static apt_bool_t resource_name_read(apr_file_t *file, mrcp_parser_t *parser)
{
	char buffer[100];
	apt_text_stream_t stream;
	apt_bool_t status = FALSE;
	apt_text_stream_init(&stream,buffer,sizeof(buffer)-1);
	if(apr_file_read(file,stream.pos,&stream.text.length) != APR_SUCCESS) {
		return FALSE;
	}

	/* skip the first line in a test file, which indicates resource name */
	if(*stream.pos =='/' && *(stream.pos+1)=='/') {
		apt_str_t line;
		stream.pos += 2;
		if(apt_text_line_read(&stream,&line) == TRUE) {
			apr_off_t offset = stream.pos - stream.text.buf;
			apr_file_seek(file,APR_SET,&offset);
			mrcp_parser_resource_set(parser,&line);
			status = TRUE;
		}
	}
	return status;
}

static apt_bool_t test_file_process(apt_test_suite_t *suite, mrcp_resource_factory_t *factory, mrcp_version_e version, const char *file_path)
{
	apr_file_t *file;
	char buffer[500];
	apt_text_stream_t stream;
	mrcp_parser_t *parser;
	mrcp_generator_t *generator;
	apr_size_t length;
	apr_size_t offset;
	apt_str_t resource_name;
	mrcp_message_t *message;
	apt_message_status_e msg_status;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open File [%s]",file_path);
	if(apr_file_open(&file,file_path,APR_FOPEN_READ | APR_FOPEN_BINARY,APR_OS_DEFAULT,suite->pool) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open File");
		return FALSE;
	}

	parser = mrcp_parser_create(factory,suite->pool);
	generator = mrcp_generator_create(factory,suite->pool);

	apt_string_reset(&resource_name);
	if(version == MRCP_VERSION_1) {
		resource_name_read(file,parser);
	}

	apt_text_stream_init(&stream,buffer,sizeof(buffer)-1);

	do {
		/* calculate offset remaining from the previous receive / if any */
		offset = stream.pos - stream.text.buf;
		/* calculate available length */
		length = sizeof(buffer) - 1 - offset;

		if(apr_file_read(file,stream.pos,&length) != APR_SUCCESS) {
			break;
		}
		/* calculate actual length of the stream */
		stream.text.length = offset + length;
		stream.pos[length] = '\0';
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Parse MRCPv2 Data [%"APR_SIZE_T_FMT" bytes]\n%s",length,stream.pos);

		/* reset pos */
		apt_text_stream_reset(&stream);
		
		do {
			msg_status = mrcp_parser_run(parser,&stream,&message);
			mrcp_message_handler(generator,message,msg_status);
		}
		while(apt_text_is_eos(&stream) == FALSE);

		/* scroll remaining stream */
		apt_text_stream_scroll(&stream);
	}
	while(apr_file_eof(file) != APR_EOF);

	apr_file_close(file);
	return TRUE;
}

static apt_bool_t test_dir_process(apt_test_suite_t *suite, mrcp_resource_factory_t *factory, mrcp_version_e version)
{
	apr_status_t rv;
	apr_dir_t *dir;

	const char *dir_name = "v2";
	if(version == MRCP_VERSION_1) {
		dir_name = "v1";
	}
	if(apr_dir_open(&dir,dir_name,suite->pool) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot Open Directory [%s]",dir_name);
		return FALSE;
	}

	do {
		apr_finfo_t finfo;
		rv = apr_dir_read(&finfo,APR_FINFO_DIRENT,dir);
		if(rv == APR_SUCCESS) {
			if(finfo.filetype == APR_REG && finfo.name) {
				int ch;
				char *file_path;
				apr_filepath_merge(&file_path,dir_name,finfo.name,APR_FILEPATH_NATIVE,suite->pool);
				test_file_process(suite,factory,version,file_path);
				printf("\nPress ENTER to continue\n");
				do {ch = getchar(); } while ((ch != '\n') && (ch != EOF));
			}
		}
	} 
	while(rv == APR_SUCCESS);

	apr_dir_close(dir);
	return TRUE;
}

static apt_bool_t parse_gen_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	mrcp_resource_factory_t *factory;
	mrcp_resource_loader_t *resource_loader;
	resource_loader = mrcp_resource_loader_create(TRUE,suite->pool);
	if(!resource_loader) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Resource Loader");
		return FALSE;
	}
	
	factory = mrcp_resource_factory_get(resource_loader);
	if(!factory) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Resource Factory");
		return FALSE;
	}

	test_dir_process(suite,factory,MRCP_VERSION_2);
	test_dir_process(suite,factory,MRCP_VERSION_1);

	mrcp_resource_factory_destroy(factory);
	return TRUE;
}

apt_test_suite_t* parse_gen_test_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"parse-gen",NULL,parse_gen_test_run);
	return suite;
}
