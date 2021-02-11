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

#ifndef RECOG_SCENARIO_H
#define RECOG_SCENARIO_H

/**
 * @file recogscenario.h
 * @brief Recognizer Scenario
 */ 

#include "umcscenario.h"

class RecogScenario : public UmcScenario
{
public:
/* ============================ CREATORS =================================== */
	RecogScenario();
	virtual ~RecogScenario();

/* ============================ MANIPULATORS =============================== */
	virtual void Destroy();

	virtual UmcSession* CreateSession();

/* ============================ ACCESSORS ================================== */
	const char* GetContentType() const;
	const char* GetContent() const;
	apr_size_t GetContentLength() const;
	const char* GetAudioSource() const;

/* ============================ INQUIRIES ================================== */
	bool IsDefineGrammarEnabled() const;
	bool IsRecognizeEnabled() const;
protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool);

	bool LoadRecognize(const apr_xml_elem* pElem, apr_pool_t* pool);
	bool LoadDefineGrammar(const apr_xml_elem* pElem, apr_pool_t* pool);

/* ============================ DATA ======================================= */
	bool        m_DefineGrammar;
	bool        m_Recognize;
	const char* m_ContentType;
	const char* m_Content;
	apr_size_t  m_ContentLength;
	const char* m_AudioSource;
};

/* ============================ INLINE METHODS ============================= */
inline const char* RecogScenario::GetContentType() const
{
	return m_ContentType;
}

inline const char* RecogScenario::GetContent() const
{
	return m_Content;
}

inline apr_size_t RecogScenario::GetContentLength() const
{
	return m_ContentLength;
}

inline const char* RecogScenario::GetAudioSource() const
{
	return m_AudioSource;
}

inline bool RecogScenario::IsDefineGrammarEnabled() const
{
	return m_DefineGrammar;
}

inline bool RecogScenario::IsRecognizeEnabled() const
{
	return m_Recognize;
}

#endif /* RECOG_SCENARIO_H */
