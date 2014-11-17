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
 * $Id: synthsession.cpp 2193 2014-10-08 03:44:33Z achaloyan@gmail.com $
 */

#include "synthsession.h"
#include "synthscenario.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"
#include "apt_log.h"

struct SynthChannel
{
	/** MRCP channel */
	mrcp_channel_t* m_pMrcpChannel;
	/** IN-PROGRESS SPEAK request */
	mrcp_message_t* m_pSpeakRequest;
	/** File to write audio stream to */
	FILE*           m_pAudioOut;

	SynthChannel() : m_pMrcpChannel(NULL), m_pSpeakRequest(NULL), m_pAudioOut(NULL) {}
};

SynthSession::SynthSession(const SynthScenario* pScenario) :
	UmcSession(pScenario),
	m_pSynthChannel(NULL)
{
}

SynthSession::~SynthSession()
{
}

bool SynthSession::Start()
{
	if(!GetScenario()->IsSpeakEnabled())
		return false;
	
	/* create channel and associate all the required data */
	m_pSynthChannel = CreateSynthChannel();
	if(!m_pSynthChannel) 
		return false;

	/* add channel to session (send asynchronous request) */
	if(!AddMrcpChannel(m_pSynthChannel->m_pMrcpChannel))
	{
		delete m_pSynthChannel;
		m_pSynthChannel = NULL;
		return false;
	}
	return true;
}

bool SynthSession::Stop()
{
	if(!UmcSession::Stop())
		return false;

	if(!m_pSynthChannel)
		return false;

	mrcp_message_t* pStopMessage = CreateMrcpMessage(m_pSynthChannel->m_pMrcpChannel,SYNTHESIZER_STOP);
	if(!pStopMessage)
		return false;

	if(m_pSynthChannel->m_pSpeakRequest)
	{
		mrcp_generic_header_t* pGenericHeader;
		/* get/allocate generic header */
		pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pStopMessage);
		if(pGenericHeader) 
		{
			pGenericHeader->active_request_id_list.count = 1;
			pGenericHeader->active_request_id_list.ids[0] = 
				m_pSynthChannel->m_pSpeakRequest->start_line.request_id;
			mrcp_generic_header_property_add(pStopMessage,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
		}

		m_pSynthChannel->m_pSpeakRequest = NULL;
	}
	
	return SendMrcpRequest(m_pSynthChannel->m_pMrcpChannel,pStopMessage);
}

bool SynthSession::OnSessionTerminate(mrcp_sig_status_code_e status)
{
	if(m_pSynthChannel)
	{
		FILE* pAudioOut = m_pSynthChannel->m_pAudioOut;
		if(pAudioOut) 
		{
			m_pSynthChannel->m_pAudioOut = NULL;
			fclose(pAudioOut);
		}

		delete m_pSynthChannel;
		m_pSynthChannel = NULL;
	}
	return UmcSession::OnSessionTerminate(status);
}

static apt_bool_t WriteStream(mpf_audio_stream_t* pStream, const mpf_frame_t* pFrame)
{
	SynthChannel* pSynthChannel = (SynthChannel*) pStream->obj;
	if(pSynthChannel && pSynthChannel->m_pAudioOut) 
	{
		fwrite(pFrame->codec_frame.buffer,1,pFrame->codec_frame.size,pSynthChannel->m_pAudioOut);
	}
	return TRUE;
}

SynthChannel* SynthSession::CreateSynthChannel()
{
	mrcp_channel_t* pChannel;
	mpf_termination_t* pTermination;
	mpf_stream_capabilities_t* pCapabilities;
	apr_pool_t* pool = GetSessionPool();

	/* create channel */
	SynthChannel* pSynthChannel = new SynthChannel;

	/* create sink stream capabilities */
	pCapabilities = mpf_sink_stream_capabilities_create(pool);
	GetScenario()->InitCapabilities(pCapabilities);

	static const mpf_audio_stream_vtable_t audio_stream_vtable = 
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		WriteStream,
		NULL
	};

	pTermination = CreateAudioTermination(
			&audio_stream_vtable,      /* virtual methods table of audio stream */
			pCapabilities,             /* capabilities of audio stream */
			pSynthChannel);            /* object to associate */
	
	pChannel = CreateMrcpChannel(
			MRCP_SYNTHESIZER_RESOURCE, /* MRCP resource identifier */
			pTermination,              /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			pSynthChannel);            /* object to associate */
	if(!pChannel)
	{
		delete pSynthChannel;
		return NULL;
	}

	pSynthChannel->m_pMrcpChannel = pChannel;
	return pSynthChannel;
}

bool SynthSession::OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	if(!UmcSession::OnChannelAdd(pMrcpChannel,status))
		return false;

	const mpf_codec_descriptor_t* pDescriptor = mrcp_application_sink_descriptor_get(pMrcpChannel);
	if(!pDescriptor) 
	{
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Media Sink Descriptor");
		return Terminate();
	}

	SynthChannel* pSynthChannel = (SynthChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(status != MRCP_SIG_STATUS_CODE_SUCCESS)
	{
		/* error case, just terminate the demo */
		return Terminate();
	}

	/* create MRCP message */
	mrcp_message_t* pMrcpMessage = CreateSpeakRequest(pMrcpChannel);
	if(pMrcpMessage) 
	{
		SendMrcpRequest(pSynthChannel->m_pMrcpChannel,pMrcpMessage);
	}

	pSynthChannel->m_pAudioOut = GetAudioOut(pDescriptor,GetSessionPool());
	return true;
}

bool SynthSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!UmcSession::OnMessageReceive(pMrcpChannel,pMrcpMessage))
		return false;

	if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) 
	{
		/* received MRCP response */
		if(pMrcpMessage->start_line.method_id == SYNTHESIZER_SPEAK) 
		{
			/* received the response to SPEAK request */
			if(pMrcpMessage->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) 
			{
				SynthChannel* pSynthChannel = (SynthChannel*) mrcp_application_channel_object_get(pMrcpChannel);
				if(pSynthChannel)
					pSynthChannel->m_pSpeakRequest = GetMrcpMessage();
				
				/* waiting for SPEAK-COMPLETE event */
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
		/* received MRCP event */
		if(pMrcpMessage->start_line.method_id == SYNTHESIZER_SPEAK_COMPLETE) 
		{
			SynthChannel* pSynthChannel = (SynthChannel*) mrcp_application_channel_object_get(pMrcpChannel);
			if(pSynthChannel)
				pSynthChannel->m_pSpeakRequest = NULL;
			/* received SPEAK-COMPLETE event, terminate the session */
			Terminate();
		}
	}
	return true;
}

mrcp_message_t* SynthSession::CreateSpeakRequest(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,SYNTHESIZER_SPEAK);
	if(!pMrcpMessage)
		return NULL;

	const SynthScenario* pScenario = GetScenario();

	mrcp_generic_header_t* pGenericHeader;
	mrcp_synth_header_t* pSynthHeader;
	/* get/allocate generic header */
	pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pMrcpMessage);
	if(pGenericHeader) 
	{
		/* set generic header fields */
		apt_string_assign(&pGenericHeader->content_type,pScenario->GetContentType(),pMrcpMessage->pool);
		mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_TYPE);

		/* set message body */
		if(pScenario->GetContent())
			apt_string_assign(&pMrcpMessage->body,pScenario->GetContent(),pMrcpMessage->pool);
	}
	/* get/allocate synthesizer header */
	pSynthHeader = (mrcp_synth_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pSynthHeader) 
	{
		/* set synthesizer header fields */
		pSynthHeader->voice_param.age = 28;
		mrcp_resource_header_property_add(pMrcpMessage,SYNTHESIZER_HEADER_VOICE_AGE);
	}

	return pMrcpMessage;
}

FILE* SynthSession::GetAudioOut(const mpf_codec_descriptor_t* pDescriptor, apr_pool_t* pool) const
{
	FILE* file;
	char* pFileName = apr_psprintf(pool,"synth-%dkHz-%s.pcm",pDescriptor->sampling_rate/1000, GetMrcpSessionId());
	apt_dir_layout_t* pDirLayout = GetScenario()->GetDirLayout();
	char* pFilePath = apt_vardir_filepath_get(pDirLayout,pFileName,pool);
	if(!pFilePath)
		return NULL;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open Speech Output File [%s] for Writing",pFilePath);
	file = fopen(pFilePath,"wb");
	if(!file)
	{
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open Speech Output File [%s] for Writing",pFilePath);
		return NULL;
	}
	return file;
}
