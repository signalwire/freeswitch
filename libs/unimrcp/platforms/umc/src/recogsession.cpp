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
 * $Id: recogsession.cpp 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "recogsession.h"
#include "recogscenario.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "apt_nlsml_doc.h"
#include "apt_log.h"

struct RecogChannel
{
	/** MRCP control channel */
	mrcp_channel_t* m_pMrcpChannel;
	/** IN-PROGRESS RECOGNIZE request */
	mrcp_message_t* m_pRecogRequest;
	/** Streaming is in-progress */
	bool            m_Streaming;
	/** File to read audio stream from */
	FILE*           m_pAudioIn;
	/** Estimated time to complete (used if no audio_in available) */
	apr_size_t      m_TimeToComplete;

	RecogChannel() :
		m_pMrcpChannel(NULL),
		m_pRecogRequest(NULL),
		m_Streaming(false),
		m_pAudioIn(NULL),
		m_TimeToComplete(0) {}
};

RecogSession::RecogSession(const RecogScenario* pScenario) :
	UmcSession(pScenario),
	m_pRecogChannel(NULL),
	m_ContentId("request1@form-level")
{
}

RecogSession::~RecogSession()
{
}

bool RecogSession::Start()
{
	const RecogScenario* pScenario = GetScenario();
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

bool RecogSession::Stop()
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

bool RecogSession::OnSessionTerminate(mrcp_sig_status_code_e status)
{
	if(m_pRecogChannel)
	{
		FILE* pAudioIn = m_pRecogChannel->m_pAudioIn;
		if(pAudioIn)
		{
			m_pRecogChannel->m_pAudioIn = NULL;
			fclose(pAudioIn);
		}
		
		delete m_pRecogChannel;
		m_pRecogChannel = NULL;
	}
	return UmcSession::OnSessionTerminate(status);
}

static apt_bool_t ReadStream(mpf_audio_stream_t* pStream, mpf_frame_t* pFrame)
{
	RecogChannel* pRecogChannel = (RecogChannel*) pStream->obj;
	if(pRecogChannel && pRecogChannel->m_Streaming) 
	{
		if(pRecogChannel->m_pAudioIn) 
		{
			if(fread(pFrame->codec_frame.buffer,1,pFrame->codec_frame.size,pRecogChannel->m_pAudioIn) == pFrame->codec_frame.size) 
			{
				/* normal read */
				pFrame->type |= MEDIA_FRAME_TYPE_AUDIO;
			}
			else 
			{
				/* file is over */
				pRecogChannel->m_Streaming = false;
			}
		}
		else 
		{
			/* fill with silence in case no file available */
			if(pRecogChannel->m_TimeToComplete >= CODEC_FRAME_TIME_BASE) 
			{
				pFrame->type |= MEDIA_FRAME_TYPE_AUDIO;
				memset(pFrame->codec_frame.buffer,0,pFrame->codec_frame.size);
				pRecogChannel->m_TimeToComplete -= CODEC_FRAME_TIME_BASE;
			}
			else 
			{
				pRecogChannel->m_Streaming = false;
			}
		}
	}
	return TRUE;
}

RecogChannel* RecogSession::CreateRecogChannel()
{
	mrcp_channel_t* pChannel;
	mpf_termination_t* pTermination;
	mpf_stream_capabilities_t* pCapabilities;
	apr_pool_t* pool = GetSessionPool();

	/* create channel */
	RecogChannel* pRecogChannel = new RecogChannel;

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

bool RecogSession::OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	if(!UmcSession::OnChannelAdd(pMrcpChannel,status))
		return false;

	if(status != MRCP_SIG_STATUS_CODE_SUCCESS)
	{
		/* error case, just terminate the demo */
		return Terminate();
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

bool RecogSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!UmcSession::OnMessageReceive(pMrcpChannel,pMrcpMessage))
		return false;

	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
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
				RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
				if(pRecogChannel)
					pRecogChannel->m_pRecogRequest = GetMrcpMessage();

				/* start to stream the speech to recognize */
				if(pRecogChannel) 
					pRecogChannel->m_Streaming = true;
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
				pRecogChannel->m_Streaming = false;

			RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
			if(pRecogChannel)
				pRecogChannel->m_pRecogRequest = NULL;

			Terminate();
		}
		else if(pMrcpMessage->start_line.method_id == RECOGNIZER_START_OF_INPUT) 
		{
			/* received start-of-input, do whatever you need here */
		}
	}
	return true;
}

bool RecogSession::OnDefineGrammar(mrcp_channel_t* pMrcpChannel)
{
	if(GetScenario()->IsRecognizeEnabled())
	{
		return StartRecognition(pMrcpChannel);
	}

	return Terminate();
}

bool RecogSession::StartRecognition(mrcp_channel_t* pMrcpChannel)
{
	const mpf_codec_descriptor_t* pDescriptor = mrcp_application_source_descriptor_get(pMrcpChannel);
	if(!pDescriptor)
	{
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Media Source Descriptor");
		return Terminate();
	}

	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	/* create and send RECOGNIZE request */
	mrcp_message_t* pMrcpMessage = CreateRecognizeRequest(pMrcpChannel);
	if(pMrcpMessage)
	{
		SendMrcpRequest(pRecogChannel->m_pMrcpChannel,pMrcpMessage);
	}

	pRecogChannel->m_pAudioIn = GetAudioIn(pDescriptor,GetSessionPool());
	if(!pRecogChannel->m_pAudioIn)
	{
		/* no audio input availble, set some estimated time to complete instead */
		pRecogChannel->m_TimeToComplete = 5000; // 5 sec
	}
	return true;
}

mrcp_message_t* RecogSession::CreateDefineGrammarRequest(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_DEFINE_GRAMMAR);
	if(!pMrcpMessage)
		return NULL;

	const RecogScenario* pScenario = GetScenario();

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
	if(pScenario->GetContent())
		apt_string_assign_n(&pMrcpMessage->body,pScenario->GetContent(),pScenario->GetContentLength(),pMrcpMessage->pool);
	return pMrcpMessage;
}

mrcp_message_t* RecogSession::CreateRecognizeRequest(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_RECOGNIZE);
	if(!pMrcpMessage)
		return NULL;

	const RecogScenario* pScenario = GetScenario();

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
			const char* pContent = apr_pstrcat(pMrcpMessage->pool,"session:",m_ContentId,NULL);
			apt_string_set(&pMrcpMessage->body,pContent);
		}
		else
		{
			apt_string_assign(&pGenericHeader->content_type,pScenario->GetContentType(),pMrcpMessage->pool);
			/* set content-id */
			apt_string_assign(&pGenericHeader->content_id,m_ContentId,pMrcpMessage->pool);
			mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_ID);
			/* set message body */
			if(pScenario->GetContent())
				apt_string_assign(&pMrcpMessage->body,pScenario->GetContent(),pMrcpMessage->pool);
		}
		mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_TYPE);
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
		pRecogHeader->no_input_timeout = 5000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
		pRecogHeader->recognition_timeout = 10000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
		pRecogHeader->start_input_timers = TRUE;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_START_INPUT_TIMERS);
		pRecogHeader->confidence_threshold = 0.87f;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
		pRecogHeader->save_waveform = TRUE;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_SAVE_WAVEFORM);
	}
	return pMrcpMessage;
}

bool RecogSession::ParseNLSMLResult(mrcp_message_t* pMrcpMessage)
{
	nlsml_result_t *pResult = nlsml_result_parse(pMrcpMessage->body.buf, pMrcpMessage->body.length, pMrcpMessage->pool);
	if(!pResult)
		return false;

	nlsml_result_trace(pResult, pMrcpMessage->pool);
	return true;
}

FILE* RecogSession::GetAudioIn(const mpf_codec_descriptor_t* pDescriptor, apr_pool_t* pool) const
{
	const char* pFileName = GetScenario()->GetAudioSource();
	if(!pFileName)
	{
		pFileName = apr_psprintf(pool,"one-%dkHz.pcm",pDescriptor->sampling_rate/1000);
	}
	apt_dir_layout_t* pDirLayout = GetScenario()->GetDirLayout();
	const char* pFilePath = apt_datadir_filepath_get(pDirLayout,pFileName,pool);
	if(!pFilePath)
		return NULL;
	
	FILE* pFile = fopen(pFilePath,"rb");
	if(!pFile)
	{
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Cannot Find [%s]",pFilePath);
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set [%s] as Speech Source",pFilePath);
	return pFile;
}
