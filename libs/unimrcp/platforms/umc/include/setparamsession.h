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

#ifndef __SETPARAM_SESSION_H__
#define __SETPARAM_SESSION_H__

/**
 * @file setparamsession.h
 * @brief Set Recognizer Params
 */ 

#include <apr_tables.h>
#include "umcsession.h"

class SetParamScenario;
struct RecogChannel;

class SetParamSession : public UmcSession
{
public:
/* ============================ CREATORS =================================== */
	SetParamSession(const SetParamScenario* pScenario);
	virtual ~SetParamSession();

protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool Start();

	RecogChannel* CreateRecogChannel();

	bool CreateRequestQueue(mrcp_channel_t* pMrcpChannel);
	mrcp_message_t* CreateSetParams1(mrcp_channel_t* pMrcpChannel);
	mrcp_message_t* CreateGetParams1(mrcp_channel_t* pMrcpChannel);
	mrcp_message_t* CreateSetParams2(mrcp_channel_t* pMrcpChannel);
	mrcp_message_t* CreateGetParams2(mrcp_channel_t* pMrcpChannel);
	mrcp_message_t* CreateSetParams3(mrcp_channel_t* pMrcpChannel);
	mrcp_message_t* CreateGetParams3(mrcp_channel_t* pMrcpChannel);

	bool ProcessNextRequest(mrcp_channel_t* pMrcpChannel);

/* ============================ HANDLERS =================================== */
	virtual bool OnSessionTerminate(mrcp_sig_status_code_e status);
	virtual bool OnChannelAdd(mrcp_channel_t* channel, mrcp_sig_status_code_e status);
	virtual bool OnMessageReceive(mrcp_channel_t* channel, mrcp_message_t* message);

/* ============================ ACCESSORS ================================== */
	const SetParamScenario* GetScenario() const;

private:
/* ============================ DATA ======================================= */
	RecogChannel*       m_pRecogChannel;
	apr_array_header_t* m_RequestQueue;
	int                 m_CurrentRequest;
};


/* ============================ INLINE METHODS ============================= */
inline const SetParamScenario* SetParamSession::GetScenario() const
{
	return (SetParamScenario*)m_pScenario;
}

#endif /*__SETPARAM_SESSION_H__*/
