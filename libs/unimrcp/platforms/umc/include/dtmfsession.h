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
 * $Id: dtmfsession.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef DTMF_SESSION_H
#define DTMF_SESSION_H

/**
 * @file dtmfsession.h
 * @brief DTMF Recognition Session
 */ 

#include "umcsession.h"

class DtmfScenario;
struct RecogChannel;

class DtmfSession : public UmcSession
{
public:
/* ============================ CREATORS =================================== */
	DtmfSession(const DtmfScenario* pScenario);
	virtual ~DtmfSession();

protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool Start();

	RecogChannel* CreateRecogChannel();
	bool StartRecognition(mrcp_channel_t* pMrcpChannel);

	mrcp_message_t* CreateRecognizeRequest(mrcp_channel_t* pMrcpChannel);

	static bool ParseNLSMLResult(mrcp_message_t* pMrcpMessage);

/* ============================ HANDLERS =================================== */
	virtual bool OnSessionTerminate(mrcp_sig_status_code_e status);
	virtual bool OnChannelAdd(mrcp_channel_t* channel, mrcp_sig_status_code_e status);
	virtual bool OnMessageReceive(mrcp_channel_t* channel, mrcp_message_t* message);

/* ============================ ACCESSORS ================================== */
	const DtmfScenario* GetScenario() const;

private:
/* ============================ DATA ======================================= */
	RecogChannel*         m_pRecogChannel;
};


/* ============================ INLINE METHODS ============================= */
inline const DtmfScenario* DtmfSession::GetScenario() const
{
	return (DtmfScenario*)m_pScenario;
}

#endif /* DTMF_SESSION_H */
