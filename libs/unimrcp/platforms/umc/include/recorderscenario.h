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
 * $Id: recorderscenario.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef RECORDER_SCENARIO_H
#define RECORDER_SCENARIO_H

/**
 * @file recorderscenario.h
 * @brief Recorder Scenario
 */ 

#include "umcscenario.h"

class RecorderScenario : public UmcScenario
{
public:
/* ============================ CREATORS =================================== */
	RecorderScenario();
	virtual ~RecorderScenario();

/* ============================ MANIPULATORS =============================== */
	virtual void Destroy();

	virtual UmcSession* CreateSession();

/* ============================ ACCESSORS ================================== */
	const char* GetAudioSource() const;

/* ============================ INQUIRIES ================================== */
	bool IsRecordEnabled() const;
protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool);

	bool LoadRecord(const apr_xml_elem* pElem, apr_pool_t* pool);

/* ============================ DATA ======================================= */
	bool        m_Record;
	const char* m_AudioSource;
};

/* ============================ INLINE METHODS ============================= */
inline const char* RecorderScenario::GetAudioSource() const
{
	return m_AudioSource;
}

inline bool RecorderScenario::IsRecordEnabled() const
{
	return m_Record;
}

#endif /* RECORDER_SCENARIO_H */
