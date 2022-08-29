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

#ifndef VERIFIER_SESSION_H
#define VERIFIER_SESSION_H

/**
 * @file verifiersession.h
 * @brief Verifier Session
 */ 

#include "umcsession.h"

class VerifierScenario;
struct VerifierChannel;

class VerifierSession : public UmcSession
{
public:
/* ============================ CREATORS =================================== */
	VerifierSession(const VerifierScenario* pScenario);
	virtual ~VerifierSession();

protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool Start();
	virtual bool Stop();

	VerifierChannel* CreateVerifierChannel();
	bool StartVerification(mrcp_channel_t* pMrcpChannel);

	mrcp_message_t* CreateStartSessionRequest(mrcp_channel_t* pMrcpChannel);
	mrcp_message_t* CreateEndSessionRequest(mrcp_channel_t* pMrcpChannel);
	mrcp_message_t* CreateVerificationRequest(mrcp_channel_t* pMrcpChannel);

	FILE* GetAudioIn(const mpf_codec_descriptor_t* pDescriptor, apr_pool_t* pool) const;

/* ============================ HANDLERS =================================== */
	virtual bool OnSessionTerminate(mrcp_sig_status_code_e status);
	virtual bool OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status);
	virtual bool OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage);

/* ============================ ACCESSORS ================================== */
	const VerifierScenario* GetScenario() const;

private:
/* ============================ DATA ======================================= */
	VerifierChannel* m_pVerifierChannel;
	const char*      m_ContentId;
};


/* ============================ INLINE METHODS ============================= */
inline const VerifierScenario* VerifierSession::GetScenario() const
{
	return (VerifierScenario*)m_pScenario;
}

#endif /* VERIFIER_SESSION_H */
