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

#include "setparamsession.h"
#include "setparamscenario.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "apt_log.h"

struct RecogChannel
{
	/** MRCP control channel */
	mrcp_channel_t* m_pMrcpChannel;
};

SetParamSession::SetParamSession(const SetParamScenario* pScenario) :
	UmcSession(pScenario),
	m_pRecogChannel(NULL),
	m_RequestQueue(NULL),
	m_CurrentRequest(0)
{
}

SetParamSession::~SetParamSession()
{
}

bool SetParamSession::Start()
{
	/* create channel and associate all the required data */
	m_pRecogChannel = CreateRecogChannel();
	if(!m_pRecogChannel) 
		return false;

	/* add channel to session (send asynchronous request) */
	if(!AddMrcpChannel(m_pRecogChannel->m_pMrcpChannel))
	{
		delete m_pRecogChannel;
		m_pRecogChannel = NULL;
		return false;
	}
	return true;
}

bool SetParamSession::OnSessionTerminate(mrcp_sig_status_code_e status)
{
	if(m_pRecogChannel)
	{
		delete m_pRecogChannel;
		m_pRecogChannel = NULL;
	}
	return UmcSession::OnSessionTerminate(status);
}

static apt_bool_t ReadStream(mpf_audio_stream_t* pStream, mpf_frame_t* pFrame)
{
	return TRUE;
}

RecogChannel* SetParamSession::CreateRecogChannel()
{
	mrcp_channel_t* pChannel;
	mpf_termination_t* pTermination;
	mpf_stream_capabilities_t* pCapabilities;
	apr_pool_t* pool = GetSessionPool();

	/* create channel */
	RecogChannel *pRecogChannel = new RecogChannel;
	pRecogChannel->m_pMrcpChannel = NULL;

	/* create source stream capabilities */
	pCapabilities = mpf_source_stream_capabilities_create(pool);
	GetScenario()->InitCapabilities(pCapabilities);

	static const mpf_audio_stream_vtable_t audio_stream_vtable = 
	{
		NULL,
		NULL,
		NULL,
		ReadStream,
		NULL,
		NULL,
		NULL,
		NULL
	};

	pTermination = CreateAudioTermination(
			&audio_stream_vtable,      /* virtual methods table of audio stream */
			pCapabilities,             /* capabilities of audio stream */
			pRecogChannel);            /* object to associate */

	pChannel = CreateMrcpChannel(
			MRCP_RECOGNIZER_RESOURCE,  /* MRCP resource identifier */
			pTermination,              /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			pRecogChannel);            /* object to associate */
	if(!pChannel)
	{
		delete pRecogChannel;
		return NULL;
	}
	
	pRecogChannel->m_pMrcpChannel = pChannel;
	return pRecogChannel;
}

bool SetParamSession::OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	if(!UmcSession::OnChannelAdd(pMrcpChannel,status))
		return false;

	if(status != MRCP_SIG_STATUS_CODE_SUCCESS)
	{
		/* error case, just terminate the demo */
		return Terminate();
	}

	if(!CreateRequestQueue(pMrcpChannel))
	{
		return Terminate();
	}

	return ProcessNextRequest(pMrcpChannel);
}

bool SetParamSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!UmcSession::OnMessageReceive(pMrcpChannel,pMrcpMessage))
		return false;

	if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) 
	{
		/* received MRCP response */
		if(pMrcpMessage->start_line.method_id == RECOGNIZER_SET_PARAMS || pMrcpMessage->start_line.method_id == RECOGNIZER_GET_PARAMS)
		{
			/* received the response */
			if(pMrcpMessage->start_line.request_state == MRCP_REQUEST_STATE_COMPLETE) 
			{
				m_CurrentRequest++;
				ProcessNextRequest(pMrcpChannel);
			}
			else 
			{
				/* received unexpected response, terminate the session */
				Terminate();
			}
		}
		else 
		{
			/* received unexpected response */
			Terminate();
		}
	}
	return true;
}

bool SetParamSession::ProcessNextRequest(mrcp_channel_t* pMrcpChannel)
{
	if(m_CurrentRequest >= m_RequestQueue->nelts) 
	{
		return Terminate();
	}

	mrcp_message_t* pMrcpMessage = APR_ARRAY_IDX(m_RequestQueue,m_CurrentRequest,mrcp_message_t*);
	if(!pMrcpMessage)
	{
		return Terminate();
	}

	return SendMrcpRequest(pMrcpChannel,pMrcpMessage);
}

bool SetParamSession::CreateRequestQueue(mrcp_channel_t* pMrcpChannel)
{
	m_CurrentRequest = 0;
	m_RequestQueue = apr_array_make(GetSessionPool(),5,sizeof(mrcp_message_t*));
	mrcp_message_t* pMrcpMessage;
	
	pMrcpMessage = CreateSetParams1(pMrcpChannel);
	if(pMrcpMessage)
		*(mrcp_message_t**)apr_array_push(m_RequestQueue) = pMrcpMessage;

	pMrcpMessage = CreateGetParams1(pMrcpChannel);
	if(pMrcpMessage)
		*(mrcp_message_t**)apr_array_push(m_RequestQueue) = pMrcpMessage;

	pMrcpMessage = CreateSetParams2(pMrcpChannel);
	if(pMrcpMessage)
		*(mrcp_message_t**)apr_array_push(m_RequestQueue) = pMrcpMessage;

	pMrcpMessage = CreateGetParams2(pMrcpChannel);
	if(pMrcpMessage)
		*(mrcp_message_t**)apr_array_push(m_RequestQueue) = pMrcpMessage;

	pMrcpMessage = CreateSetParams3(pMrcpChannel);
	if(pMrcpMessage)
		*(mrcp_message_t**)apr_array_push(m_RequestQueue) = pMrcpMessage;

	pMrcpMessage = CreateGetParams3(pMrcpChannel);
	if(pMrcpMessage)
		*(mrcp_message_t**)apr_array_push(m_RequestQueue) = pMrcpMessage;

	return true;
}

mrcp_message_t* SetParamSession::CreateSetParams1(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_SET_PARAMS);
	if(!pMrcpMessage)
		return NULL;

	mrcp_recog_header_t* pRecogHeader;
	/* get/allocate recog header */
	pRecogHeader = (mrcp_recog_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pRecogHeader) 
	{
		/* set recog header fields */
		pRecogHeader->confidence_threshold = 0.4f;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
		pRecogHeader->sensitivity_level = 0.531f;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_SENSITIVITY_LEVEL);
		pRecogHeader->speed_vs_accuracy = 0.5f;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_SPEED_VS_ACCURACY);
		pRecogHeader->n_best_list_length = 5;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_N_BEST_LIST_LENGTH);
		pRecogHeader->no_input_timeout = 5000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
	}

	return pMrcpMessage;
}

mrcp_message_t* SetParamSession::CreateGetParams1(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_GET_PARAMS);
	if(!pMrcpMessage)
		return NULL;

	mrcp_recog_header_t* pRecogHeader;
	/* get/allocate recog header */
	pRecogHeader = (mrcp_recog_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pRecogHeader) 
	{
		/* set recog header fields */
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_SENSITIVITY_LEVEL);
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_SPEED_VS_ACCURACY);
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_N_BEST_LIST_LENGTH);
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
	}

	return pMrcpMessage;
}

mrcp_message_t* SetParamSession::CreateSetParams2(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_SET_PARAMS);
	if(!pMrcpMessage)
		return NULL;

	mrcp_recog_header_t* pRecogHeader;
	/* get/allocate recog header */
	pRecogHeader = (mrcp_recog_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pRecogHeader) 
	{
		/* set recog header fields */
		pRecogHeader->recognition_timeout = 5000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
		pRecogHeader->speech_complete_timeout = 1000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT);
		pRecogHeader->speech_incomplete_timeout = 2000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT);
		pRecogHeader->dtmf_interdigit_timeout = 3000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT);
	}

	return pMrcpMessage;
}

mrcp_message_t* SetParamSession::CreateGetParams2(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_GET_PARAMS);
	if(!pMrcpMessage)
		return NULL;

	mrcp_recog_header_t* pRecogHeader;
	/* get/allocate recog header */
	pRecogHeader = (mrcp_recog_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pRecogHeader) 
	{
		/* set recog header fields */
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT);
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT);
		mrcp_resource_header_name_property_add(pMrcpMessage,RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT);
	}

	return pMrcpMessage;
}

mrcp_message_t* SetParamSession::CreateSetParams3(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_SET_PARAMS);
	if(!pMrcpMessage)
		return NULL;

	mrcp_generic_header_t* pGenericHeader;
	/* get/allocate generic header */
	pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pMrcpMessage);
	if(pGenericHeader) 
	{
		apr_pool_t* pool = GetSessionPool();
		/* set generic header fields */
		apt_pair_arr_t* pVSP = apt_pair_array_create(3,pool);
		if(pVSP)
		{
			apt_str_t name;
			apt_str_t value;
			
			apt_string_set(&name,"confidencelevel");
			apt_string_set(&value,"500");
			apt_pair_array_append(pVSP,&name,&value,pool);
			
			apt_string_set(&name,"sensitivity");
			apt_string_set(&value,"0.500");
			apt_pair_array_append(pVSP,&name,&value,pool);

			apt_string_set(&name,"speedvsaccuracy");
			apt_string_set(&value,"0.789");
			apt_pair_array_append(pVSP,&name,&value,pool);

			apt_string_set(&name,"timeout");
			apt_string_set(&value,"1000");
			apt_pair_array_append(pVSP,&name,&value,pool);

			apt_string_set(&name,"swirec_application_name");
			apt_string_set(&value,"UniMRCP");
			apt_pair_array_append(pVSP,&name,&value,pool);

			apt_string_set(&name,"swirec_phoneme_lookahead_beam");
			apt_string_set(&value,"-50");
			apt_pair_array_append(pVSP,&name,&value,pool);

			pGenericHeader->vendor_specific_params = pVSP;
			mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS);
		}
	}

	return pMrcpMessage;
}

mrcp_message_t* SetParamSession::CreateGetParams3(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_GET_PARAMS);
	if(!pMrcpMessage)
		return NULL;

	mrcp_generic_header_t* pGenericHeader;
	/* get/allocate generic header */
	pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pMrcpMessage);
	if(pGenericHeader) 
	{
		mrcp_generic_header_name_property_add(pMrcpMessage,GENERIC_HEADER_ACCEPT_CHARSET);
		mrcp_generic_header_name_property_add(pMrcpMessage,GENERIC_HEADER_CACHE_CONTROL);
		mrcp_generic_header_name_property_add(pMrcpMessage,GENERIC_HEADER_LOGGING_TAG);
		mrcp_generic_header_name_property_add(pMrcpMessage,GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS);
		mrcp_generic_header_name_property_add(pMrcpMessage,GENERIC_HEADER_FETCH_TIMEOUT);
		mrcp_generic_header_name_property_add(pMrcpMessage,GENERIC_HEADER_SET_COOKIE);
		mrcp_generic_header_name_property_add(pMrcpMessage,GENERIC_HEADER_SET_COOKIE2);
	}

	return pMrcpMessage;
}
