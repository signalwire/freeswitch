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
 * $Id: recordersession.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef RECORDER_SESSION_H
#define RECORDER_SESSION_H

/**
 * @file recordersession.h
 * @brief Recorder Session
 */ 

#include "umcsession.h"

class RecorderScenario;
struct RecorderChannel;

class RecorderSession : public UmcSession
{
public:
/* ============================ CREATORS =================================== */
	RecorderSession(const RecorderScenario* pScenario);
	virtual ~RecorderSession();

protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool Start();

	RecorderChannel* CreateRecorderChannel();
	bool StartRecorder(mrcp_channel_t* pMrcpChannel);

	mrcp_message_t* CreateRecordRequest(mrcp_channel_t* pMrcpChannel);

	FILE* GetAudioIn(const mpf_codec_descriptor_t* pDescriptor, apr_pool_t* pool) const;

/* ============================ HANDLERS =================================== */
	virtual bool OnSessionTerminate(mrcp_sig_status_code_e status);
	virtual bool OnChannelAdd(mrcp_channel_t* channel, mrcp_sig_status_code_e status);
	virtual bool OnMessageReceive(mrcp_channel_t* channel, mrcp_message_t* message);

/* ============================ ACCESSORS ================================== */
	const RecorderScenario* GetScenario() const;

private:
/* ============================ DATA ======================================= */
	RecorderChannel* m_pRecorderChannel;
};


/* ============================ INLINE METHODS ============================= */
inline const RecorderScenario* RecorderSession::GetScenario() const
{
	return (RecorderScenario*)m_pScenario;
}

#endif /* RECORDER_SESSION_H */
