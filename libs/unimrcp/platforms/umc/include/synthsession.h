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

#ifndef __SYNTH_SESSION_H__
#define __SYNTH_SESSION_H__

/**
 * @file synthsession.h
 * @brief Synthesizer Session
 */ 

#include "umcsession.h"

class SynthScenario;
struct SynthChannel;

class SynthSession : public UmcSession
{
public:
/* ============================ CREATORS =================================== */
	SynthSession(const SynthScenario* pScenario);
	virtual ~SynthSession();

protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool Start();

	SynthChannel* CreateSynthChannel();

	mrcp_message_t* CreateSpeakRequest(mrcp_channel_t* pMrcpChannel);
	FILE* GetAudioOut(const mpf_codec_descriptor_t* pDescriptor, apr_pool_t* pool) const;

/* ============================ HANDLERS =================================== */
	virtual bool OnSessionTerminate(mrcp_sig_status_code_e status);
	virtual bool OnChannelAdd(mrcp_channel_t* channel, mrcp_sig_status_code_e status);
	virtual bool OnMessageReceive(mrcp_channel_t* channel, mrcp_message_t* message);

/* ============================ ACCESSORS ================================== */
	const SynthScenario* GetScenario() const;
	
private:
/* ============================ DATA ======================================= */
	SynthChannel* m_pSynthChannel;
};


/* ============================ INLINE METHODS ============================= */
inline const SynthScenario* SynthSession::GetScenario() const
{
	return (SynthScenario*)m_pScenario;
}

#endif /*__SYNTH_SESSION_H__*/
