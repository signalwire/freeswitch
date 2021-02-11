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

#include "umcsession.h"
#include "umcscenario.h"
#include "mrcp_message.h"
#include "apt_pool.h"

UmcSession::UmcSession(const UmcScenario* pScenario) :
	m_pScenario(pScenario),
	m_pMrcpProfile(NULL),
	m_pMethodProvider(NULL),
	m_pMrcpApplication(NULL),
	m_pMrcpSession(NULL),
	m_pMrcpMessage(NULL),
	m_Running(false),
	m_Terminating(false)
{
	static int id = 0;
	if(id == INT_MAX)
		id = 0;
	id++;

	m_Pool = apt_pool_create();
	m_Id = apr_psprintf(m_Pool,"%d",id);
}

UmcSession::~UmcSession()
{
	apr_pool_destroy(m_Pool);
}

bool UmcSession::Run()
{
	if(m_Running)
		return false;

	if(!m_pMrcpProfile)
		m_pMrcpProfile = m_pScenario->GetMrcpProfile();

	if(!m_pMrcpProfile || !m_pMrcpApplication)
		return false;

	/* create session */
	if(!CreateMrcpSession(m_pMrcpProfile))
		return false;
	
	m_Running = true;
	
	bool ret = false;
	if(m_pScenario->IsDiscoveryEnabled())
		ret = DiscoverResources();
	else
		ret = Start();

	if(!ret)
	{
		m_Running = false;
		DestroyMrcpSession();
	}
	return ret;
}

bool UmcSession::Stop()
{
	if(m_Terminating)
		return false;

	return true;
}

bool UmcSession::Terminate()
{
	if(m_Terminating)
		return false;

	m_Running = false;
	m_Terminating = true;
	return (mrcp_application_session_terminate(m_pMrcpSession) == TRUE);
}

bool UmcSession::OnSessionTerminate(mrcp_sig_status_code_e status)
{
	if(!m_Terminating)
		return false;

	m_Terminating = false;
	DestroyMrcpSession();
	if(m_pMethodProvider)
		m_pMethodProvider->ExitSession(this);
	return true;
}

bool UmcSession::OnSessionUpdate(mrcp_sig_status_code_e status)
{
	return m_Running;
}

bool UmcSession::OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	return m_Running;
}

bool UmcSession::OnChannelRemove(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	return m_Running;
}

bool UmcSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!m_Running)
		return false;

	if(!m_pMrcpMessage)
		return false;

	/* match request identifiers */
	if(m_pMrcpMessage->start_line.request_id != pMrcpMessage->start_line.request_id)
		return false;

	return true;
}

bool UmcSession::OnTerminateEvent(mrcp_channel_t* pMrcpChannel)
{
	if(!m_Running)
		return false;

	return Terminate();
}

bool UmcSession::OnResourceDiscover(mrcp_session_descriptor_t* pDescriptor, mrcp_sig_status_code_e status)
{
	if(!m_Running)
		return false;

	if(!Start())
		Terminate();
	return true;
}

bool UmcSession::CreateMrcpSession(const char* pProfileName)
{
	m_pMrcpSession = mrcp_application_session_create_ex(m_pMrcpApplication,pProfileName,this,FALSE,m_Pool);
	if(!m_pMrcpSession)
		return false;

	const char* name = apr_psprintf(m_Pool,"umc-%s",m_Id);
	mrcp_application_session_name_set(m_pMrcpSession,name);
	return true;
}

bool UmcSession::DestroyMrcpSession()
{
	if(!m_pMrcpSession)
		return false;

	mrcp_application_session_destroy(m_pMrcpSession);
	m_pMrcpSession = NULL;
	return true;
}

bool UmcSession::AddMrcpChannel(mrcp_channel_t* pMrcpChannel)
{
	if(!m_Running)
		return false;

	return (mrcp_application_channel_add(m_pMrcpSession,pMrcpChannel) == TRUE);
}

bool UmcSession::RemoveMrcpChannel(mrcp_channel_t* pMrcpChannel)
{
	if(!m_Running)
		return false;

	return (mrcp_application_channel_remove(m_pMrcpSession,pMrcpChannel) == TRUE);
}

bool UmcSession::SendMrcpRequest(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!m_Running)
		return false;

	m_pMrcpMessage = pMrcpMessage;
	return (mrcp_application_message_send(m_pMrcpSession,pMrcpChannel,pMrcpMessage) == TRUE);
}

bool UmcSession::DiscoverResources()
{
	if(!m_Running)
		return false;

	return (mrcp_application_resource_discover(m_pMrcpSession) == TRUE);
}

mrcp_channel_t* UmcSession::CreateMrcpChannel(
						mrcp_resource_id resource_id, 
						mpf_termination_t* pTermination, 
						mpf_rtp_termination_descriptor_t* pRtpDescriptor, 
						void* pObj)
{
	return mrcp_application_channel_create(
			m_pMrcpSession,            /* session, channel belongs to */
			resource_id,               /* MRCP resource identifier */
			pTermination,              /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			pObj);                     /* object to associate */
}

mpf_termination_t* UmcSession::CreateAudioTermination(
						const mpf_audio_stream_vtable_t* pStreamVtable,
						mpf_stream_capabilities_t* pCapabilities,
						void* pObj)
{
	return mrcp_application_audio_termination_create(
			m_pMrcpSession,  /* session, termination belongs to */
			pStreamVtable,   /* virtual methods table of audio stream */
			pCapabilities,   /* capabilities of audio stream */
			pObj);           /* object to associate */
}

mrcp_message_t* UmcSession::CreateMrcpMessage(
		mrcp_channel_t* pMrcpChannel, 
		mrcp_method_id method_id)
{
	return mrcp_application_message_create(m_pMrcpSession,pMrcpChannel,method_id);
}

const char* UmcSession::GetMrcpSessionId() const
{
	if(!m_pMrcpSession)
		return NULL;

	const apt_str_t *pId = mrcp_application_session_id_get(m_pMrcpSession);
	return pId->buf;
}
