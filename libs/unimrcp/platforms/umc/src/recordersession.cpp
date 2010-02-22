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

#include "recordersession.h"
#include "recorderscenario.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recorder_header.h"
#include "mrcp_recorder_resource.h"
#include "apt_log.h"

struct RecorderChannel
{
	/** MRCP control channel */
	mrcp_channel_t* m_pMrcpChannel;
	/** Streaming is in-progress */
	bool            m_Streaming;
	/** File to read audio stream from */
	FILE*           m_pAudioIn;
};

RecorderSession::RecorderSession(const RecorderScenario* pScenario) :
	UmcSession(pScenario),
	m_pRecorderChannel(NULL)
{
}

RecorderSession::~RecorderSession()
{
}

bool RecorderSession::Start()
{
	const RecorderScenario* pScenario = GetScenario();
	if(!pScenario->IsRecordEnabled())
		return false;
	
	/* create channel and associate all the required data */
	m_pRecorderChannel = CreateRecorderChannel();
	if(!m_pRecorderChannel) 
		return false;

	/* add channel to session (send asynchronous request) */
	if(!AddMrcpChannel(m_pRecorderChannel->m_pMrcpChannel))
	{
		delete m_pRecorderChannel;
		m_pRecorderChannel = NULL;
		return false;
	}
	return true;
}

bool RecorderSession::OnSessionTerminate(mrcp_sig_status_code_e status)
{
	if(m_pRecorderChannel)
	{
		FILE* pAudioIn = m_pRecorderChannel->m_pAudioIn;
		if(pAudioIn)
		{
			m_pRecorderChannel->m_pAudioIn = NULL;
			fclose(pAudioIn);
		}
		
		delete m_pRecorderChannel;
		m_pRecorderChannel = NULL;
	}
	return UmcSession::OnSessionTerminate(status);
}

static apt_bool_t ReadStream(mpf_audio_stream_t* pStream, mpf_frame_t* pFrame)
{
	RecorderChannel* pRecorderChannel = (RecorderChannel*) pStream->obj;
	if(pRecorderChannel && pRecorderChannel->m_Streaming) 
	{
		if(pRecorderChannel->m_pAudioIn) 
		{
			if(fread(pFrame->codec_frame.buffer,1,pFrame->codec_frame.size,pRecorderChannel->m_pAudioIn) == pFrame->codec_frame.size) 
			{
				/* normal read */
				pFrame->type |= MEDIA_FRAME_TYPE_AUDIO;
			}
			else 
			{
				/* file is over */
				pRecorderChannel->m_Streaming = false;
			}
		}
	}
	return TRUE;
}

RecorderChannel* RecorderSession::CreateRecorderChannel()
{
	mrcp_channel_t* pChannel;
	mpf_termination_t* pTermination;
	mpf_stream_capabilities_t* pCapabilities;
	apr_pool_t* pool = GetSessionPool();

	/* create channel */
	RecorderChannel *pRecorderChannel = new RecorderChannel;
	pRecorderChannel->m_pMrcpChannel = NULL;
	pRecorderChannel->m_Streaming = false;
	pRecorderChannel->m_pAudioIn = NULL;

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
		NULL
	};

	pTermination = CreateAudioTermination(
			&audio_stream_vtable,      /* virtual methods table of audio stream */
			pCapabilities,             /* capabilities of audio stream */
			pRecorderChannel);         /* object to associate */

	pChannel = CreateMrcpChannel(
			MRCP_RECORDER_RESOURCE,    /* MRCP resource identifier */
			pTermination,              /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			pRecorderChannel);         /* object to associate */
	if(!pChannel)
	{
		delete pRecorderChannel;
		return NULL;
	}
	
	pRecorderChannel->m_pMrcpChannel = pChannel;
	return pRecorderChannel;
}

bool RecorderSession::OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	if(!UmcSession::OnChannelAdd(pMrcpChannel,status))
		return false;

	if(status != MRCP_SIG_STATUS_CODE_SUCCESS)
	{
		/* error case, just terminate the demo */
		return Terminate();
	}

	return StartRecorder(pMrcpChannel);
}

bool RecorderSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!UmcSession::OnMessageReceive(pMrcpChannel,pMrcpMessage))
		return false;

	RecorderChannel* pRecorderChannel = (RecorderChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) 
	{
		/* received MRCP response */
		if(pMrcpMessage->start_line.method_id == RECORDER_RECORD)
		{
			/* received the response to RECORD request */
			if(pMrcpMessage->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS)
			{
				/* start to stream the speech to record */
				if(pRecorderChannel)
				{
					pRecorderChannel->m_Streaming = true;
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
		if(pMrcpMessage->start_line.method_id == RECORDER_RECORD_COMPLETE) 
		{
			if(pRecorderChannel) 
			{
				pRecorderChannel->m_Streaming = false;
			}
			Terminate();
		}
		else if(pMrcpMessage->start_line.method_id == RECORDER_START_OF_INPUT) 
		{
			/* received start-of-input, do whatever you need here */
		}
	}
	return true;
}

bool RecorderSession::StartRecorder(mrcp_channel_t* pMrcpChannel)
{
	RecorderChannel* pRecorderChannel = (RecorderChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	/* create and send RECORD request */
	mrcp_message_t* pMrcpMessage = CreateRecordRequest(pMrcpChannel);
	if(pMrcpMessage)
	{
		SendMrcpRequest(pRecorderChannel->m_pMrcpChannel,pMrcpMessage);
	}

	const mpf_codec_descriptor_t* pDescriptor = mrcp_application_source_descriptor_get(pMrcpChannel);
	pRecorderChannel->m_pAudioIn = GetAudioIn(pDescriptor,GetSessionPool());
	return true;
}

mrcp_message_t* RecorderSession::CreateRecordRequest(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECORDER_RECORD);
	if(!pMrcpMessage)
		return NULL;

	mrcp_recorder_header_t* pRecorderHeader;

	/* get/allocate recorder header */
	pRecorderHeader = (mrcp_recorder_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pRecorderHeader)
	{
		/* set recorder header fields */
		pRecorderHeader->no_input_timeout = 5000;
		mrcp_resource_header_property_add(pMrcpMessage,RECORDER_HEADER_NO_INPUT_TIMEOUT);

		pRecorderHeader->final_silence = 300;
		mrcp_resource_header_property_add(pMrcpMessage,RECORDER_HEADER_FINAL_SILENCE);

		pRecorderHeader->max_time = 10000;
		mrcp_resource_header_property_add(pMrcpMessage,RECORDER_HEADER_MAX_TIME);
	}
	return pMrcpMessage;
}

FILE* RecorderSession::GetAudioIn(const mpf_codec_descriptor_t* pDescriptor, apr_pool_t* pool) const
{
	const char* pFileName = GetScenario()->GetAudioSource();
	if(!pFileName)
	{
		pFileName = apr_psprintf(pool,"demo-%dkHz.pcm",
			pDescriptor ? pDescriptor->sampling_rate/1000 : 8);
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
