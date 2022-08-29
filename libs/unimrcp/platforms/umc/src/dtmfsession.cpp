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

#include "dtmfsession.h"
#include "dtmfscenario.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "mpf_dtmf_generator.h"
#include "apt_nlsml_doc.h"
#include "apt_log.h"

struct DtmfRecogChannel
{
	/** MRCP control channel */
	mrcp_channel_t*       m_pMrcpChannel;
	/** DTMF generator */
	mpf_dtmf_generator_t* m_pDtmfGenerator;
	/** IN-PROGRESS RECOGNIZE request */
	mrcp_message_t*       m_pRecogRequest;
	/** Streaming is in-progress */
	bool                  m_Streaming;

	DtmfRecogChannel() :
		m_pMrcpChannel(NULL),
		m_pDtmfGenerator(NULL),
		m_pRecogRequest(NULL),
		m_Streaming(false) {}
};

DtmfSession::DtmfSession(const DtmfScenario* pScenario) :
	UmcSession(pScenario),
	m_pRecogChannel(NULL),
	m_ContentId("request1@form-level")
{
}

DtmfSession::~DtmfSession()
{
}

bool DtmfSession::Start()
{
	const DtmfScenario* pScenario = GetScenario();
	if(!pScenario->IsDefineGrammarEnabled() && !pScenario->IsRecognizeEnabled())
		return false;
	
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

bool DtmfSession::Stop()
{
	if(!UmcSession::Stop())
		return false;

	if(!m_pRecogChannel)
		return false;

	mrcp_message_t* pStopMessage = CreateMrcpMessage(m_pRecogChannel->m_pMrcpChannel,RECOGNIZER_STOP);
	if(!pStopMessage)
		return false;

	if(m_pRecogChannel->m_pRecogRequest)
	{
		mrcp_generic_header_t* pGenericHeader;
		/* get/allocate generic header */
		pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pStopMessage);
		if(pGenericHeader) 
		{
			pGenericHeader->active_request_id_list.count = 1;
			pGenericHeader->active_request_id_list.ids[0] = 
				m_pRecogChannel->m_pRecogRequest->start_line.request_id;
			mrcp_generic_header_property_add(pStopMessage,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
		}

		m_pRecogChannel->m_pRecogRequest = NULL;
	}
	
	return SendMrcpRequest(m_pRecogChannel->m_pMrcpChannel,pStopMessage);
}

bool DtmfSession::OnSessionTerminate(mrcp_sig_status_code_e status)
{
	if(m_pRecogChannel)
	{
		if(m_pRecogChannel->m_pDtmfGenerator)
		{
			mpf_dtmf_generator_destroy(m_pRecogChannel->m_pDtmfGenerator);
			m_pRecogChannel->m_pDtmfGenerator = NULL;
		}
		
		delete m_pRecogChannel;
		m_pRecogChannel = NULL;
	}
	return UmcSession::OnSessionTerminate(status);
}

static apt_bool_t ReadStream(mpf_audio_stream_t* pStream, mpf_frame_t* pFrame)
{
	DtmfRecogChannel* pRecogChannel = (DtmfRecogChannel*) pStream->obj;
	if(pRecogChannel && pRecogChannel->m_Streaming) 
	{
		if(pRecogChannel->m_pDtmfGenerator) 
		{
			mpf_dtmf_generator_put_frame(pRecogChannel->m_pDtmfGenerator,pFrame);
		}
	}
	return TRUE;
}

DtmfRecogChannel* DtmfSession::CreateRecogChannel()
{
	mrcp_channel_t* pChannel;
	mpf_termination_t* pTermination;
	mpf_stream_capabilities_t* pCapabilities;
	apr_pool_t* pool = GetSessionPool();

	/* create channel */
	DtmfRecogChannel* pRecogChannel = new DtmfRecogChannel;

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

bool DtmfSession::OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	if(!UmcSession::OnChannelAdd(pMrcpChannel,status))
		return false;

	if(status != MRCP_SIG_STATUS_CODE_SUCCESS)
	{
		/* error case, just terminate the demo */
		return Terminate();
	}

	DtmfRecogChannel* pRecogChannel = (DtmfRecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(pRecogChannel)
	{
		const mpf_audio_stream_t* pStream = mrcp_application_audio_stream_get(pMrcpChannel);
		if(pStream)
		{
			pRecogChannel->m_pDtmfGenerator = mpf_dtmf_generator_create(pStream,GetSessionPool());
		}
	}

	if(GetScenario()->IsDefineGrammarEnabled())
	{
		mrcp_message_t* pMrcpMessage = CreateDefineGrammarRequest(pMrcpChannel);
		if(pMrcpMessage)
			SendMrcpRequest(pMrcpChannel,pMrcpMessage);
		return true;
	}

	return StartRecognition(pMrcpChannel);
}

bool DtmfSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!UmcSession::OnMessageReceive(pMrcpChannel,pMrcpMessage))
		return false;

	const DtmfScenario* pScenario = GetScenario();
	DtmfRecogChannel* pRecogChannel = (DtmfRecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(!pRecogChannel)
		return false;

	if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) 
	{
		/* received MRCP response */
		if(pMrcpMessage->start_line.method_id == RECOGNIZER_DEFINE_GRAMMAR) 
		{
			/* received the response to DEFINE-GRAMMAR request */
			if(pMrcpMessage->start_line.request_state == MRCP_REQUEST_STATE_COMPLETE) 
			{
				OnDefineGrammar(pMrcpChannel);
			}
			else 
			{
				/* received unexpected response, terminate the session */
				Terminate();
			}
		}
		else if(pMrcpMessage->start_line.method_id == RECOGNIZER_RECOGNIZE)
		{
			/* received the response to RECOGNIZE request */
			if(pMrcpMessage->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS)
			{
				/* start to stream the DTMFs to recognize */
				if(pRecogChannel && pRecogChannel->m_pDtmfGenerator)
				{
					const char* digits = pScenario->GetDigits();
					if(digits)
					{
						mpf_dtmf_generator_enqueue(pRecogChannel->m_pDtmfGenerator,digits);
						pRecogChannel->m_Streaming = true;
					}
				}
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
		}
	}
	else if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) 
	{
		if(pMrcpMessage->start_line.method_id == RECOGNIZER_RECOGNITION_COMPLETE) 
		{
			ParseNLSMLResult(pMrcpMessage);
			if(pRecogChannel) 
			{
				pRecogChannel->m_Streaming = false;
			}
			Terminate();
		}
		else if(pMrcpMessage->start_line.method_id == RECOGNIZER_START_OF_INPUT) 
		{
			/* received start-of-input, do whatever you need here */
		}
	}
	return true;
}

bool DtmfSession::OnDefineGrammar(mrcp_channel_t* pMrcpChannel)
{
	if(GetScenario()->IsRecognizeEnabled())
	{
		return StartRecognition(pMrcpChannel);
	}

	return Terminate();
}

bool DtmfSession::StartRecognition(mrcp_channel_t* pMrcpChannel)
{
	DtmfRecogChannel* pRecogChannel = (DtmfRecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	/* create and send RECOGNIZE request */
	mrcp_message_t* pMrcpMessage = CreateRecognizeRequest(pMrcpChannel);
	if(pMrcpMessage)
	{
		SendMrcpRequest(pRecogChannel->m_pMrcpChannel,pMrcpMessage);
	}

	return true;
}

mrcp_message_t* DtmfSession::CreateDefineGrammarRequest(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_DEFINE_GRAMMAR);
	if(!pMrcpMessage)
		return NULL;

	const DtmfScenario* pScenario = GetScenario();

	mrcp_generic_header_t* pGenericHeader;
	/* get/allocate generic header */
	pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pMrcpMessage);
	if(pGenericHeader) 
	{
		/* set generic header fields */
		if(pScenario->GetContentType())
		{
			apt_string_assign(&pGenericHeader->content_type,pScenario->GetContentType(),pMrcpMessage->pool);
			mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_TYPE);
		}
		apt_string_assign(&pGenericHeader->content_id,m_ContentId,pMrcpMessage->pool);
		mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_ID);
	}

	/* set message body */
	if(pScenario->GetGrammar())
		apt_string_assign(&pMrcpMessage->body,pScenario->GetGrammar(),pMrcpMessage->pool);
	return pMrcpMessage;
}

mrcp_message_t* DtmfSession::CreateRecognizeRequest(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_RECOGNIZE);
	if(!pMrcpMessage)
		return NULL;

	const DtmfScenario* pScenario = GetScenario();

	mrcp_generic_header_t* pGenericHeader;
	mrcp_recog_header_t* pRecogHeader;

	/* get/allocate generic header */
	pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pMrcpMessage);
	if(pGenericHeader)
	{
		/* set generic header fields */
		if(pScenario->IsDefineGrammarEnabled())
		{
			apt_string_assign(&pGenericHeader->content_type,"text/uri-list",pMrcpMessage->pool);
			/* set message body */
			const char* pContent = apr_pstrcat(pMrcpMessage->pool, "session:", m_ContentId, NULL);
			apt_string_set(&pMrcpMessage->body,pContent);
		}
		else
		{
			/* set content-id */
			apt_string_assign(&pGenericHeader->content_id, m_ContentId, pMrcpMessage->pool);
			mrcp_generic_header_property_add(pMrcpMessage, GENERIC_HEADER_CONTENT_ID);
			apt_string_assign(&pGenericHeader->content_type,pScenario->GetContentType(),pMrcpMessage->pool);
			/* set message body */
			if(pScenario->GetGrammar())
				apt_string_assign(&pMrcpMessage->body,pScenario->GetGrammar(),pMrcpMessage->pool);
		}
		mrcp_generic_header_property_add(pMrcpMessage, GENERIC_HEADER_CONTENT_TYPE);
	}
	/* get/allocate recognizer header */
	pRecogHeader = (mrcp_recog_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pRecogHeader)
	{
		/* set recognizer header fields */
		if(pMrcpMessage->start_line.version == MRCP_VERSION_2)
		{
			pRecogHeader->cancel_if_queue = FALSE;
			mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_CANCEL_IF_QUEUE);
		}
	}
	return pMrcpMessage;
}

bool DtmfSession::ParseNLSMLResult(mrcp_message_t* pMrcpMessage)
{
	nlsml_result_t *pResult = nlsml_result_parse(pMrcpMessage->body.buf, pMrcpMessage->body.length, pMrcpMessage->pool);
	if(!pResult)
		return false;

	nlsml_result_trace(pResult, pMrcpMessage->pool);
	return true;
}
