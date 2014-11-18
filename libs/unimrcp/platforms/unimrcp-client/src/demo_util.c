/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: demo_util.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "demo_util.h"
/* common includes */
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
/* synthesizer includes */
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"
/* recognizer includes */
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
/* logger include */
#include "apt_log.h"

static void demo_message_body_set(mrcp_message_t *mrcp_message, const apt_dir_layout_t *dir_layout, const char *file_name)
{
	char *file_path = apt_datadir_filepath_get(dir_layout,file_name,mrcp_message->pool);
	if(file_path) {
		FILE *file = fopen(file_path,"r");
		if(file) {
			char text[1024];
			apr_size_t size;
			size = fread(text,1,sizeof(text),file);
			apt_string_assign_n(&mrcp_message->body,text,size,mrcp_message->pool);
			fclose(file);
		}
	}
}

/** Create demo MRCP message (SPEAK request) */
mrcp_message_t* demo_speak_message_create(mrcp_session_t *session, mrcp_channel_t *channel, const apt_dir_layout_t *dir_layout)
{
	/* create MRCP message */
	mrcp_message_t *mrcp_message = mrcp_application_message_create(session,channel,SYNTHESIZER_SPEAK);
	if(mrcp_message) {
		mrcp_generic_header_t *generic_header;
		mrcp_synth_header_t *synth_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			/* set generic header fields */
			apt_string_assign(&generic_header->content_type,"application/synthesis+ssml",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
		}
		/* get/allocate synthesizer header */
		synth_header = mrcp_resource_header_prepare(mrcp_message);
		if(synth_header) {
			/* set synthesizer header fields */
			synth_header->voice_param.age = 28;
			mrcp_resource_header_property_add(mrcp_message,SYNTHESIZER_HEADER_VOICE_AGE);
		}
		/* set message body */
		demo_message_body_set(mrcp_message,dir_layout,"speak.xml");
	}
	return mrcp_message;
}

/** Create demo MRCP message (DEFINE-GRAMMAR request) */
mrcp_message_t* demo_define_grammar_message_create(mrcp_session_t *session, mrcp_channel_t *channel, const apt_dir_layout_t *dir_layout)
{
	/* create MRCP message */
	mrcp_message_t *mrcp_message = mrcp_application_message_create(session,channel,RECOGNIZER_DEFINE_GRAMMAR);
	if(mrcp_message) {
		mrcp_generic_header_t *generic_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			/* set generic header fields */
			if(mrcp_message->start_line.version == MRCP_VERSION_2) {
				apt_string_assign(&generic_header->content_type,"application/srgs+xml",mrcp_message->pool);
			}
			else {
				apt_string_assign(&generic_header->content_type,"application/grammar+xml",mrcp_message->pool);
			}
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
			apt_string_assign(&generic_header->content_id,"request1@form-level.store",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_ID);
		}
		/* set message body */
		demo_message_body_set(mrcp_message,dir_layout,"grammar.xml");
	}
	return mrcp_message;
}

/** Create demo MRCP message (RECOGNIZE request) */
mrcp_message_t* demo_recognize_message_create(mrcp_session_t *session, mrcp_channel_t *channel, const apt_dir_layout_t *dir_layout)
{
	const char text[] = "session:request1@form-level.store";

	/* create MRCP message */
	mrcp_message_t *mrcp_message = mrcp_application_message_create(session,channel,RECOGNIZER_RECOGNIZE);
	if(mrcp_message) {
		mrcp_recog_header_t *recog_header;
		mrcp_generic_header_t *generic_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			/* set generic header fields */
			apt_string_assign(&generic_header->content_type,"text/uri-list",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
		}
		/* get/allocate recognizer header */
		recog_header = mrcp_resource_header_prepare(mrcp_message);
		if(recog_header) {
			if(mrcp_message->start_line.version == MRCP_VERSION_2) {
				/* set recognizer header fields */
				recog_header->cancel_if_queue = FALSE;
				mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CANCEL_IF_QUEUE);
			}
			recog_header->no_input_timeout = 5000;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
			recog_header->recognition_timeout = 10000;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
			recog_header->start_input_timers = TRUE;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_START_INPUT_TIMERS);
			recog_header->confidence_threshold = 0.87f;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
		}
		/* set message body */
		apt_string_assign(&mrcp_message->body,text,mrcp_message->pool);
	}
	return mrcp_message;
}

/** Create demo RTP termination descriptor */
mpf_rtp_termination_descriptor_t* demo_rtp_descriptor_create(apr_pool_t *pool)
{
	mpf_codec_descriptor_t *codec_descriptor;
	mpf_rtp_media_descriptor_t *media;
	/* create rtp descriptor */
	mpf_rtp_termination_descriptor_t *rtp_descriptor = apr_palloc(pool,sizeof(mpf_rtp_termination_descriptor_t));
	mpf_rtp_termination_descriptor_init(rtp_descriptor);
	/* create rtp local media */
	media = apr_palloc(pool,sizeof(mpf_rtp_media_descriptor_t));
	mpf_rtp_media_descriptor_init(media);
	apt_string_assign(&media->ip,"127.0.0.1",pool);
	media->port = 6000;
	media->state = MPF_MEDIA_ENABLED;
	media->direction = STREAM_DIRECTION_RECEIVE;

	/* initialize codec list */
	mpf_codec_list_init(&media->codec_list,2,pool);
	/* set codec descriptor */
	codec_descriptor = mpf_codec_list_add(&media->codec_list);
	if(codec_descriptor) {
		codec_descriptor->payload_type = 0;
	}
	/* set another codec descriptor */
	codec_descriptor = mpf_codec_list_add(&media->codec_list);
	if(codec_descriptor) {
		codec_descriptor->payload_type = 96;
		apt_string_set(&codec_descriptor->name,"PCMU");
		codec_descriptor->sampling_rate = 16000;
		codec_descriptor->channel_count = 1;
	}

	rtp_descriptor->audio.local = media;
	return rtp_descriptor;
}
