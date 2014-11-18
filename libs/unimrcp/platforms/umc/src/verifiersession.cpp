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
 * $Id: verifiersession.cpp 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "verifiersession.h"
#include "verifierscenario.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_verifier_header.h"
#include "mrcp_verifier_resource.h"
#include "apt_nlsml_doc.h"
#include "apt_log.h"

struct VerifierChannel
{
	/** MRCP control channel */
	mrcp_channel_t* m_pMrcpChannel;
	/** IN-PROGRESS VERIFY request */
	mrcp_message_t* m_pVerificationRequest;
	/** Streaming is in-progress */
	bool            m_Streaming;
	/** File to read audio stream from */
	FILE*           m_pAudioIn;
	/** Estimated time to complete (used if no audio_in available) */
	apr_size_t      m_TimeToComplete;

	VerifierChannel() :
		m_pMrcpChannel(NULL),
		m_pVerificationRequest(NULL),
		m_Streaming(false),
		m_pAudioIn(NULL),
		m_TimeToComplete(0) {}
};

VerifierSession::VerifierSession(const VerifierScenario* pScenario) :
	UmcSession(pScenario),
	m_pVerifierChannel(NULL),
	m_ContentId("request1@form-level")
{
}

VerifierSession::~VerifierSession()
{
}

bool VerifierSession::Start()
{
	/* create channel and associate all the required data */
	m_pVerifierChannel = CreateVerifierChannel();
	if(!m_pVerifierChannel) 
		return false;

	/* add channel to session (send asynchronous request) */
	if(!AddMrcpChannel(m_pVerifierChannel->m_pMrcpChannel))
	{
		delete m_pVerifierChannel;
		m_pVerifierChannel = NULL;
		return false;
	}
	return true;
}

bool VerifierSession::Stop()
{
	if(!UmcSession::Stop())
		return false;

	if(!m_pVerifierChannel)
		return false;

	mrcp_message_t* pStopMessage = CreateMrcpMessage(m_pVerifierChannel->m_pMrcpChannel,VERIFIER_STOP);
	if(!pStopMessage)
		return false;

	return SendMrcpRequest(m_pVerifierChannel->m_pMrcpChannel,pStopMessage);
}

bool VerifierSession::OnSessionTerminate(mrcp_sig_status_code_e status)
{
	if(m_pVerifierChannel)
	{
		FILE* pAudioIn = m_pVerifierChannel->m_pAudioIn;
		if(pAudioIn)
		{
			m_pVerifierChannel->m_pAudioIn = NULL;
			fclose(pAudioIn);
		}
		
		delete m_pVerifierChannel;
		m_pVerifierChannel = NULL;
	}
	return UmcSession::OnSessionTerminate(status);
}

static apt_bool_t ReadStream(mpf_audio_stream_t* pStream, mpf_frame_t* pFrame)
{
	VerifierChannel* pVerifierChannel = (VerifierChannel*) pStream->obj;
	if(pVerifierChannel && pVerifierChannel->m_Streaming) 
	{
		if(pVerifierChannel->m_pAudioIn) 
		{
			if(fread(pFrame->codec_frame.buffer,1,pFrame->codec_frame.size,pVerifierChannel->m_pAudioIn) == pFrame->codec_frame.size) 
			{
				/* normal read */
				pFrame->type |= MEDIA_FRAME_TYPE_AUDIO;
			}
			else 
			{
				/* file is over */
				pVerifierChannel->m_Streaming = false;
			}
		}
		else 
		{
			/* fill with silence in case no file available */
			if(pVerifierChannel->m_TimeToComplete >= CODEC_FRAME_TIME_BASE) 
			{
				pFrame->type |= MEDIA_FRAME_TYPE_AUDIO;
				memset(pFrame->codec_frame.buffer,0,pFrame->codec_frame.size);
				pVerifierChannel->m_TimeToComplete -= CODEC_FRAME_TIME_BASE;
			}
			else 
			{
				pVerifierChannel->m_Streaming = false;
			}
		}
	}
	return TRUE;
}

VerifierChannel* VerifierSession::CreateVerifierChannel()
{
	mrcp_channel_t* pChannel;
	mpf_termination_t* pTermination;
	mpf_stream_capabilities_t* pCapabilities;
	apr_pool_t* pool = GetSessionPool();

	/* create channel */
	VerifierChannel* pVerifierChannel = new VerifierChannel;

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
			pVerifierChannel);         /* object to associate */

	pChannel = CreateMrcpChannel(
			MRCP_VERIFIER_RESOURCE,    /* MRCP resource identifier */
			pTermination,              /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			pVerifierChannel);         /* object to associate */
	if(!pChannel)
	{
		delete pVerifierChannel;
		return NULL;
	}
	
	pVerifierChannel->m_pMrcpChannel = pChannel;
	return pVerifierChannel;
}

bool VerifierSession::OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	if(!UmcSession::OnChannelAdd(pMrcpChannel,status))
		return false;

	if(status != MRCP_SIG_STATUS_CODE_SUCCESS)
	{
		/* error case, just terminate the demo */
		return Terminate();
	}

	return StartVerification(pMrcpChannel);
}

bool VerifierSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!UmcSession::OnMessageReceive(pMrcpChannel,pMrcpMessage))
		return false;

	VerifierChannel* pVerifierChannel = (VerifierChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) 
	{
		/* received MRCP response */
		if(pMrcpMessage->start_line.method_id == VERIFIER_START_SESSION)
		{
			/* received the response to START-SESSION request */
			/* create and send VERIFY request */
			mrcp_message_t* pMrcpMessage = CreateVerificationRequest(pMrcpChannel);
			if(pMrcpMessage)
			{
				SendMrcpRequest(pVerifierChannel->m_pMrcpChannel,pMrcpMessage);
			}
		}
		else if(pMrcpMessage->start_line.method_id == VERIFIER_END_SESSION)
		{
			/* received the response to END-SESSION request */
			Terminate();
		}
		else if(pMrcpMessage->start_line.method_id == VERIFIER_VERIFY)
		{
			/* received the response to VERIFY request */
			if(pMrcpMessage->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS)
			{
				VerifierChannel* pVerifierChannel = (VerifierChannel*) mrcp_application_channel_object_get(pMrcpChannel);
				if(pVerifierChannel)
					pVerifierChannel->m_pVerificationRequest = GetMrcpMessage();

				/* start to stream the speech to Verify */
				if(pVerifierChannel) 
					pVerifierChannel->m_Streaming = true;
			}
			else
			{
				/* create and send END-SESSION request */
				mrcp_message_t* pMrcpMessage = CreateEndSessionRequest(pMrcpChannel);
				if(pMrcpMessage)
				{
					SendMrcpRequest(pVerifierChannel->m_pMrcpChannel,pMrcpMessage);
				}
			}
		}
		else 
		{
			/* received unexpected response */
		}
	}
	else if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) 
	{
		if(pMrcpMessage->start_line.method_id == VERIFIER_VERIFICATION_COMPLETE) 
		{
			if(pVerifierChannel) 
				pVerifierChannel->m_Streaming = false;

			VerifierChannel* pVerifierChannel = (VerifierChannel*) mrcp_application_channel_object_get(pMrcpChannel);
			if(pVerifierChannel)
				pVerifierChannel->m_pVerificationRequest = NULL;

			/* create and send END-SESSION request */
			mrcp_message_t* pMrcpMessage = CreateEndSessionRequest(pMrcpChannel);
			if(pVerifierChannel && pMrcpMessage)
			{
				SendMrcpRequest(pVerifierChannel->m_pMrcpChannel,pMrcpMessage);
			}
		}
		else if(pMrcpMessage->start_line.method_id == VERIFIER_START_OF_INPUT) 
		{
			/* received start-of-input, do whatever you need here */
		}
	}
	return true;
}

bool VerifierSession::StartVerification(mrcp_channel_t* pMrcpChannel)
{
	const mpf_codec_descriptor_t* pDescriptor = mrcp_application_source_descriptor_get(pMrcpChannel);
	if(!pDescriptor)
	{
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Media Source Descriptor");
		return Terminate();
	}

	VerifierChannel* pVerifierChannel = (VerifierChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	/* create and send Verification request */
	mrcp_message_t* pMrcpMessage = CreateStartSessionRequest(pMrcpChannel);
	if(pMrcpMessage)
	{
		SendMrcpRequest(pVerifierChannel->m_pMrcpChannel,pMrcpMessage);
	}

	pVerifierChannel->m_pAudioIn = GetAudioIn(pDescriptor,GetSessionPool());
	if(!pVerifierChannel->m_pAudioIn)
	{
		/* no audio input availble, set some estimated time to complete instead */
		pVerifierChannel->m_TimeToComplete = 5000; // 5 sec
	}
	return true;
}

mrcp_message_t* VerifierSession::CreateStartSessionRequest(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,VERIFIER_START_SESSION);
	if(!pMrcpMessage)
		return NULL;

	mrcp_verifier_header_t* pVerifierHeader;

	/* get/allocate verifier header */
	pVerifierHeader = (mrcp_verifier_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pVerifierHeader)
	{
		const VerifierScenario* pScenario = GetScenario();
		const char* pRepositoryURI = pScenario->GetRepositoryURI();
		if(pRepositoryURI)
		{
			apt_string_set(&pVerifierHeader->repository_uri,pRepositoryURI);
			mrcp_resource_header_property_add(pMrcpMessage,VERIFIER_HEADER_REPOSITORY_URI);
		}
		const char* pVoiceprintIdentifier = pScenario->GetVoiceprintIdentifier();
		if(pVoiceprintIdentifier)
		{
			apt_string_set(&pVerifierHeader->voiceprint_identifier,pVoiceprintIdentifier);
			mrcp_resource_header_property_add(pMrcpMessage,VERIFIER_HEADER_VOICEPRINT_IDENTIFIER);
		}
		const char* pVerificationMode = pScenario->GetVerificationMode();
		if(pVerificationMode)
		{
			apt_string_set(&pVerifierHeader->verification_mode,pVerificationMode);
			mrcp_resource_header_property_add(pMrcpMessage,VERIFIER_HEADER_VERIFICATION_MODE);
		}
	}
	return pMrcpMessage;
}

mrcp_message_t* VerifierSession::CreateEndSessionRequest(mrcp_channel_t* pMrcpChannel)
{
	return CreateMrcpMessage(pMrcpChannel,VERIFIER_END_SESSION);
}

mrcp_message_t* VerifierSession::CreateVerificationRequest(mrcp_channel_t* pMrcpChannel)
{
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,VERIFIER_VERIFY);
	if(!pMrcpMessage)
		return NULL;

	mrcp_verifier_header_t* pVerifierHeader;

	/* get/allocate verifier header */
	pVerifierHeader = (mrcp_verifier_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pVerifierHeader)
	{
		pVerifierHeader->no_input_timeout = 5000;
		mrcp_resource_header_property_add(pMrcpMessage,VERIFIER_HEADER_NO_INPUT_TIMEOUT);
		pVerifierHeader->start_input_timers = TRUE;
		mrcp_resource_header_property_add(pMrcpMessage,VERIFIER_HEADER_START_INPUT_TIMERS);
	}
	return pMrcpMessage;
}

FILE* VerifierSession::GetAudioIn(const mpf_codec_descriptor_t* pDescriptor, apr_pool_t* pool) const
{
	const VerifierScenario* pScenario = GetScenario();
	const char* pVoiceprintIdentifier = pScenario->GetVoiceprintIdentifier();
	if(!pVoiceprintIdentifier)
	{
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No Voiceprint Specified");
		return NULL;
	}

	const char* pFileName = apr_psprintf(pool,"%s-%dkHz.pcm",
			pVoiceprintIdentifier,
			pDescriptor->sampling_rate/1000);
	apt_dir_layout_t* pDirLayout = pScenario->GetDirLayout();
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
