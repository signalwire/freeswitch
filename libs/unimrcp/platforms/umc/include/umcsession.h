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

#ifndef UMC_SESSION_H
#define UMC_SESSION_H

/**
 * @file umcsession.h
 * @brief UMC Session
 */ 

#include "mrcp_application.h"

class UmcScenario;
class UmcSession;

class UmcSessionEventHandler
{
public:
/* ============================ CREATORS =================================== */
	virtual ~UmcSessionEventHandler() {}

/* ============================ HANDLERS =================================== */
	virtual bool OnSessionTerminate(mrcp_sig_status_code_e status) = 0;
	virtual bool OnSessionUpdate(mrcp_sig_status_code_e status) = 0;
	virtual bool OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status) = 0;
	virtual bool OnChannelRemove(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status) = 0;
	virtual bool OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage) = 0;
	virtual bool OnTerminateEvent(mrcp_channel_t* pMrcpChannel) = 0;
	virtual bool OnResourceDiscover(mrcp_session_descriptor_t* pDescriptor, mrcp_sig_status_code_e status) = 0;
};

class UmcSessionMethodProvider
{
public:
/* ============================ CREATORS =================================== */
	virtual ~UmcSessionMethodProvider() {}

/* ============================ MANIPULATORS =============================== */
	virtual void ExitSession(UmcSession* pUmcSession) = 0;
};

class UmcSession : protected UmcSessionEventHandler
{
public:
/* ============================ CREATORS =================================== */
	UmcSession(const UmcScenario* pScenario);
	virtual ~UmcSession();

/* ============================ MANIPULATORS =============================== */
	virtual bool Run();
	virtual bool Stop();
	virtual bool Terminate();

/* ============================ ACCESSORS ================================== */
	void SetMrcpProfile(const char* pMrcpProfile);
	void SetMrcpApplication(mrcp_application_t* pMrcpApplication);
	void SetMethodProvider(UmcSessionMethodProvider* pMethodProvider);

	const UmcScenario* GetScenario() const;
	apr_pool_t* GetSessionPool() const;

	const char* GetId() const;

protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool Start() = 0;

	bool CreateMrcpSession(const char* pProfileName);
	bool DestroyMrcpSession();

	bool AddMrcpChannel(mrcp_channel_t* pMrcpChannel);
	bool RemoveMrcpChannel(mrcp_channel_t* pMrcpChannel);
	bool SendMrcpRequest(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage);
	bool DiscoverResources();

	mrcp_channel_t* CreateMrcpChannel(
			mrcp_resource_id resource_id,
			mpf_termination_t* pTermination,
			mpf_rtp_termination_descriptor_t* pRtpDescriptor,
			void* pObj);
	mpf_termination_t* CreateAudioTermination(
			const mpf_audio_stream_vtable_t* pStreamVtable,
			mpf_stream_capabilities_t* pCapabilities,
			void* pObj);
	mrcp_message_t* CreateMrcpMessage(
			mrcp_channel_t* pMrcpChannel,
			mrcp_method_id method_id);

/* ============================ HANDLERS =================================== */
	virtual bool OnSessionTerminate(mrcp_sig_status_code_e status);
	virtual bool OnSessionUpdate(mrcp_sig_status_code_e status);
	virtual bool OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status);
	virtual bool OnChannelRemove(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status);
	virtual bool OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage);
	virtual bool OnTerminateEvent(mrcp_channel_t* pMrcpChannel);
	virtual bool OnResourceDiscover(mrcp_session_descriptor_t* pDescriptor, mrcp_sig_status_code_e status);

/* ============================ ACCESSORS ================================== */
	const char* GetMrcpSessionId() const;
	mrcp_message_t* GetMrcpMessage() const;

/* ============================ DATA ======================================= */
	const UmcScenario*          m_pScenario;
	const char*                 m_pMrcpProfile;
	const char*                 m_Id;

private:
/* ============================ DATA ======================================= */
	apr_pool_t*                 m_Pool;
	UmcSessionMethodProvider*   m_pMethodProvider;
	mrcp_application_t*         m_pMrcpApplication;
	mrcp_session_t*             m_pMrcpSession;
	mrcp_message_t*             m_pMrcpMessage; /* last message sent */
	bool                        m_Running;
	bool                        m_Terminating;
};

/* ============================ INLINE METHODS ============================= */
inline const UmcScenario* UmcSession::GetScenario() const
{
	return m_pScenario;
}

inline const char* UmcSession::GetId() const
{
	return m_Id;
}

inline apr_pool_t* UmcSession::GetSessionPool() const
{
	return m_Pool;
}

inline void UmcSession::SetMrcpApplication(mrcp_application_t* pMrcpApplication)
{
	m_pMrcpApplication = pMrcpApplication;
}

inline void UmcSession::SetMrcpProfile(const char* pMrcpProfile)
{
	m_pMrcpProfile = pMrcpProfile;
}


inline void UmcSession::SetMethodProvider(UmcSessionMethodProvider* pMethodProvider)
{
	m_pMethodProvider = pMethodProvider;
}

inline mrcp_message_t* UmcSession::GetMrcpMessage() const
{
	return m_pMrcpMessage;
}

#endif /* UMC_SESSION_H */
