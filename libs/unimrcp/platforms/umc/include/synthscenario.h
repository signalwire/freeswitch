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

#ifndef __SYNTH_SCENARIO_H__
#define __SYNTH_SCENARIO_H__

/**
 * @file synthscenario.h
 * @brief Synthesizer Scenario
 */ 

#include "umcscenario.h"

class SynthScenario : public UmcScenario
{
public:
/* ============================ CREATORS =================================== */
	SynthScenario();
	virtual ~SynthScenario();

/* ============================ MANIPULATORS =============================== */
	virtual void Destroy();

	virtual UmcSession* CreateSession();

/* ============================ ACCESSORS ================================== */
	const char* GetContentType() const;
	const char* GetContent() const;

/* ============================ INQUIRIES ================================== */
	bool IsSpeakEnabled() const;

protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool);

	bool LoadSpeak(const apr_xml_elem* pElem, apr_pool_t* pool);

/* ============================ DATA ======================================= */
	bool        m_Speak;
	const char* m_ContentType;
	const char* m_Content;
};

/* ============================ INLINE METHODS ============================= */
inline const char* SynthScenario::GetContentType() const
{
	return m_ContentType;
}

inline const char* SynthScenario::GetContent() const
{
	return m_Content;
}

inline bool SynthScenario::IsSpeakEnabled() const
{
	return m_Speak;
}

#endif /*__SYNTH_SCENARIO_H__*/
