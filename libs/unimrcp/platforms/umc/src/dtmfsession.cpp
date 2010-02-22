/*
 * Copyright 2008 Arsen Chaloyan
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

struct RecogChannel
{
	/** MRCP control channel */
	mrcp_channel_t*       m_pMrcpChannel;
	/** DTMF generator */
	mpf_dtmf_generator_t* m_pDtmfGenerator;
	/** Audio stream */
	mpf_audio_stream_t*   m_pStream;
	/** Streaming is in-progress */
	bool                  m_Streaming;
};

DtmfSession::DtmfSession(const DtmfScenario* pScenario) :
	UmcSession(pScenario),
	m_pRecogChannel(NULL)
{
}

DtmfSession::~DtmfSession()
{
}

bool DtmfSession::Start()
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

static apt_bool_t OpenStream(mpf_audio_stream_t* pStream, mpf_codec_t *codec)
{
	RecogChannel* pRecogChannel = (RecogChannel*) pStream->obj;
	pRecogChannel->m_pStream = pStream;
	return TRUE;
}

static apt_bool_t ReadStream(mpf_audio_stream_t* pStream, mpf_frame_t* pFrame)
{
	RecogChannel* pRecogChannel = (RecogChannel*) pStream->obj;
	if(pRecogChannel && pRecogChannel->m_Streaming) 
	{
		if(pRecogChannel->m_pDtmfGenerator) 
		{
			mpf_dtmf_generator_put_frame(pRecogChannel->m_pDtmfGenerator,pFrame);
		}
	}
	return TRUE;
}

RecogChannel* DtmfSession::CreateRecogChannel()
{
	mrcp_channel_t* pChannel;
	mpf_termination_t* pTermination;
	mpf_stream_capabilities_t* pCapabilities;
	apr_pool_t* pool = GetSessionPool();

	/* create channel */
	RecogChannel *pRecogChannel = new RecogChannel;
	pRecogChannel->m_pMrcpChannel = NULL;
	pRecogChannel->m_pDtmfGenerator = NULL;
	pRecogChannel->m_pStream = NULL;
	pRecogChannel->m_Streaming = false;

	/* create source stream capabilities */
	pCapabilities = mpf_source_stream_capabilities_create(pool);
	GetScenario()->InitCapabilities(pCapabilities);

	static const mpf_audio_stream_vtable_t audio_stream_vtable = 
	{
		NULL,
		OpenStream,
		NULL,
		ReadStream,
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

	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(pRecogChannel)
	{
		pRecogChannel->m_pDtmfGenerator = mpf_dtmf_generator_create(pRecogChannel->m_pStream,GetSessionPool());
	}

	return StartRecognition(pMrcpChannel);
}

bool DtmfSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!UmcSession::OnMessageReceive(pMrcpChannel,pMrcpMessage))
		return false;

	const DtmfScenario* pScenario = GetScenario();
	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) 
	{
		if(pMrcpMessage->start_line.method_id == RECOGNIZER_RECOGNIZE)
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

bool DtmfSession::StartRecognition(mrcp_channel_t* pMrcpChannel)
{
	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	/* create and send RECOGNIZE request */
	mrcp_message_t* pMrcpMessage = CreateRecognizeRequest(pMrcpChannel);
	if(pMrcpMessage)
	{
		SendMrcpRequest(pRecogChannel->m_pMrcpChannel,pMrcpMessage);
	}

	return true;
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
		apt_string_assign(&pGenericHeader->content_type,pScenario->GetContentType(),pMrcpMessage->pool);
		mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_TYPE);
		/* set message body */
		if(pScenario->GetGrammar())
			apt_string_assign(&pMrcpMessage->body,pScenario->GetGrammar(),pMrcpMessage->pool);
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

bool DtmfSession::ParseNLSMLResult(mrcp_message_t* pMrcpMessage) const
{
	apr_xml_elem* pInterpret;
	apr_xml_elem* pInstance;
	apr_xml_elem* pInput;
	apr_xml_doc* pDoc = nlsml_doc_load(&pMrcpMessage->body,pMrcpMessage->pool);
	if(!pDoc)
		return false;
	
	/* walk through interpreted results */
	pInterpret = nlsml_first_interpret_get(pDoc);
	for(; pInterpret; pInterpret = nlsml_next_interpret_get(pInterpret)) 
	{
		/* get instance and input */
		nlsml_interpret_results_get(pInterpret,&pInstance,&pInput);
		if(pInstance) 
		{
			/* process instance */
			if(pInstance->first_cdata.first) 
			{
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpreted Instance [%s]",pInstance->first_cdata.first->text);
			}
		}
		if(pInput) 
		{
			/* process input */
			if(pInput->first_cdata.first)
			{
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpreted Input [%s]",pInput->first_cdata.first->text);
			}
		}
	}
	return true;
}
